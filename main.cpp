#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QScreen>
#include <KStatusNotifierItem>
#include <QMenu>
#include <QSlider>
#include <QMouseEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWindow>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QCommandLineParser>
#include <QTimer>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

#define DBUS_SERVICE "org.kde.kdedimmer"
#define DBUS_PATH "/Dimmer"
#define DBUS_INTERFACE "org.kde.kdedimmer"

class DimOverlay : public QWidget {
    Q_OBJECT
public:
    DimOverlay(QWidget *parent = nullptr) : QWidget(parent), m_opacity(50) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAutoFillBackground(false);
    }

    void setDimOpacity(int opacity) {
        m_opacity = qBound(0, opacity, 90);
        update();
    }

    int dimOpacity() const { return m_opacity; }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(rect(), QColor(0, 0, 0, m_opacity * 255 / 100));
    }

    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        configureLayerShell();
        // Re-apply after surface is fully ready (Wayland resets input region on remap)
        QTimer::singleShot(50, this, &DimOverlay::configureLayerShell);
    }

private:
    void configureLayerShell() {
        auto *window = windowHandle();
        if (!window) return;

        auto *lsh = LayerShellQt::Window::get(window);
        if (!lsh) return;

        lsh->setLayer(LayerShellQt::Window::LayerOverlay);
        lsh->setAnchors(LayerShellQt::Window::Anchors(
            LayerShellQt::Window::AnchorTop |
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft |
            LayerShellQt::Window::AnchorRight));
        lsh->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        lsh->setExclusiveZone(-1);

        // Set empty input region for click-through
        auto *native = QGuiApplication::platformNativeInterface();
        if (!native) return;

        auto *compositor = static_cast<wl_compositor*>(
            native->nativeResourceForIntegration("compositor"));
        auto *surface = static_cast<wl_surface*>(
            native->nativeResourceForWindow("surface", window));

        if (compositor && surface) {
            wl_region *region = wl_compositor_create_region(compositor);
            // Empty region = click-through
            wl_surface_set_input_region(surface, region);
            wl_surface_commit(surface);
            wl_region_destroy(region);
        }
    }

    int m_opacity;
};

class SliderPopup : public QWidget {
    Q_OBJECT
public:
    SliderPopup(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFixedSize(220, 50);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);

        m_slider = new QSlider(Qt::Horizontal);
        m_slider->setRange(0, 90);
        m_slider->setValue(50);
        m_slider->setMinimumWidth(120);

        m_valueLabel = new QLabel("50%");
        m_valueLabel->setMinimumWidth(35);

        layout->addWidget(new QLabel("Dim:"));
        layout->addWidget(m_slider);
        layout->addWidget(m_valueLabel);
    }

    void showOnScreen(const QPoint &pos) {
        // Find screen from SNI click coordinates
        m_screen = nullptr;
        for (auto *s : QGuiApplication::screens()) {
            QRect geo = s->geometry();
            qreal dpr = s->devicePixelRatio();
            QRect phys(geo.x(), geo.y(),
                       (int)(geo.width() * dpr), (int)(geo.height() * dpr));
            if (phys.contains(pos)) {
                m_screen = s;
                break;
            }
        }
        if (!m_screen) m_screen = QGuiApplication::primaryScreen();

        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
        }
    }

    QSlider *slider() { return m_slider; }
    QLabel *valueLabel() { return m_valueLabel; }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        configureLayerShell();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (!m_slider->geometry().contains(event->pos())) {
            hide();
        }
        QWidget::mousePressEvent(event);
    }

private:
    void configureLayerShell() {
        auto *window = windowHandle();
        if (!window) return;
        auto *lsh = LayerShellQt::Window::get(window);
        if (!lsh) return;

        lsh->setLayer(LayerShellQt::Window::LayerOverlay);
        lsh->setAnchors(LayerShellQt::Window::Anchors(
            LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorRight));
        lsh->setExclusiveZone(0);
        lsh->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);

        lsh->setMargins(QMargins(0, 0, 8, 48));
        if (m_screen)
            window->setScreen(m_screen);
    }

    QSlider *m_slider;
    QLabel *m_valueLabel;
    QScreen *m_screen = nullptr;
};

class TrayController : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", DBUS_INTERFACE)

public:
    TrayController(QList<DimOverlay*> overlays, QObject *parent = nullptr)
        : QObject(parent), m_overlays(overlays), m_enabled(true), m_opacity(50) {

        m_popup = new SliderPopup();
        m_slider = m_popup->slider();
        m_valueLabel = m_popup->valueLabel();

        m_sni = new KStatusNotifierItem(this);
        m_sni->setIconByName("brightness-low");
        m_sni->setToolTipTitle("Screen Dimmer");
        m_sni->setToolTipSubTitle(QString("%1 monitors").arg(overlays.size()));
        m_sni->setCategory(KStatusNotifierItem::SystemServices);
        m_sni->setStatus(KStatusNotifierItem::Active);

        // Right-click menu (rendered natively by KDE via DBusMenu)
        auto *menu = new QMenu();
        m_toggleAction = menu->addAction("Enabled");
        m_toggleAction->setCheckable(true);
        m_toggleAction->setChecked(true);
        m_sni->setContextMenu(menu);

        // Left-click: show slider popup (pos comes from KDE panel via D-Bus)
        connect(m_sni, &KStatusNotifierItem::activateRequested, this,
            [this](bool, const QPoint &pos) {
                m_popup->showOnScreen(pos);
            });

        // Middle-click: toggle
        connect(m_sni, &KStatusNotifierItem::secondaryActivateRequested, this,
            [this](const QPoint &) { m_toggleAction->toggle(); });

        // Scroll: adjust opacity (scroll up = less dim)
        connect(m_sni, &KStatusNotifierItem::scrollRequested, this,
            [this](int delta, Qt::Orientation) {
                AdjustOpacity(delta > 0 ? -5 : 5);
            });

        connect(m_slider, &QSlider::valueChanged, this, &TrayController::onSliderChanged);
        connect(m_toggleAction, &QAction::toggled, this, &TrayController::onToggle);

        // Register D-Bus service
        auto bus = QDBusConnection::sessionBus();
        bus.registerObject(DBUS_PATH, this, QDBusConnection::ExportAllSlots);
        bus.registerService(DBUS_SERVICE);
    }

public slots:
    // D-Bus methods
    int GetOpacity() { return m_opacity; }
    bool GetEnabled() { return m_enabled; }

    void SetOpacity(int value) {
        m_opacity = qBound(0, value, 90);
        m_slider->setValue(m_opacity);
    }

    void AdjustOpacity(int delta) {
        SetOpacity(m_opacity + delta);
    }

    void SetEnabled(bool enabled) {
        m_toggleAction->setChecked(enabled);
    }

    void Toggle() {
        m_toggleAction->toggle();
    }

private slots:
    void onSliderChanged(int value) {
        m_opacity = value;
        for (auto *overlay : m_overlays) {
            overlay->setDimOpacity(value);
        }
        m_valueLabel->setText(QString("%1%").arg(value));
        if (value > 0 && !m_enabled) {
            m_enabled = true;
            m_toggleAction->setChecked(true);
            for (auto *overlay : m_overlays) {
                overlay->show();
            }
        }
    }

    void onToggle(bool checked) {
        m_enabled = checked;
        for (auto *overlay : m_overlays) {
            if (checked) {
                overlay->show();
            } else {
                overlay->hide();
            }
        }
    }

private:
    QList<DimOverlay*> m_overlays;
    KStatusNotifierItem *m_sni;
    SliderPopup *m_popup;
    QSlider *m_slider;
    QLabel *m_valueLabel;
    QAction *m_toggleAction;
    bool m_enabled;
    int m_opacity;
};

// CLI client functions
int sendCommand(const QString &method, const QVariant &arg = QVariant()) {
    QDBusInterface iface(DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        fprintf(stderr, "Error: kdedimmer service not running\n");
        return 1;
    }

    QDBusReply<void> reply;
    if (arg.isValid()) {
        reply = iface.call(method, arg);
    } else {
        reply = iface.call(method);
    }

    if (!reply.isValid()) {
        fprintf(stderr, "Error: %s\n", qPrintable(reply.error().message()));
        return 1;
    }
    return 0;
}

int getOpacity() {
    QDBusInterface iface(DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        fprintf(stderr, "Error: kdedimmer service not running\n");
        return -1;
    }
    QDBusReply<int> reply = iface.call("GetOpacity");
    if (!reply.isValid()) {
        fprintf(stderr, "Error: %s\n", qPrintable(reply.error().message()));
        return -1;
    }
    return reply.value();
}

void printUsage() {
    printf("Usage: kdedimmer [command]\n\n");
    printf("Commands:\n");
    printf("  (none)      Start the dimmer daemon\n");
    printf("  set <0-90>  Set opacity to value\n");
    printf("  +<N>        Increase opacity by N%%\n");
    printf("  -<N>        Decrease opacity by N%%\n");
    printf("  toggle      Toggle dimmer on/off\n");
    printf("  on          Enable dimmer\n");
    printf("  off         Disable dimmer\n");
    printf("  get         Print current opacity\n");
    printf("  status      Print current status\n");
}

int main(int argc, char *argv[]) {
    // Handle CLI commands (no GUI needed)
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        QString cmd = argv[1];

        if (cmd == "help" || cmd == "--help" || cmd == "-h") {
            printUsage();
            return 0;
        } else if (cmd == "get") {
            int val = getOpacity();
            if (val >= 0) printf("%d\n", val);
            return val >= 0 ? 0 : 1;
        } else if (cmd == "status") {
            QDBusInterface iface(DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, QDBusConnection::sessionBus());
            if (!iface.isValid()) {
                printf("stopped\n");
                return 1;
            }
            QDBusReply<int> opacity = iface.call("GetOpacity");
            QDBusReply<bool> enabled = iface.call("GetEnabled");
            printf("running, %s, opacity=%d%%\n",
                   enabled.value() ? "enabled" : "disabled",
                   opacity.value());
            return 0;
        } else if (cmd == "toggle") {
            return sendCommand("Toggle");
        } else if (cmd == "on") {
            return sendCommand("SetEnabled", true);
        } else if (cmd == "off") {
            return sendCommand("SetEnabled", false);
        } else if (cmd == "set" && argc > 2) {
            return sendCommand("SetOpacity", QString(argv[2]).toInt());
        } else if (cmd.startsWith('+') || cmd.startsWith('-')) {
            int delta = cmd.toInt();
            return sendCommand("AdjustOpacity", delta);
        } else {
            fprintf(stderr, "Unknown command: %s\n", argv[1]);
            printUsage();
            return 1;
        }
    }

    // Daemon mode
    LayerShellQt::Shell::useLayerShell();

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("kdedimmer");

    // Check if already running
    QDBusInterface iface(DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, QDBusConnection::sessionBus());
    if (iface.isValid()) {
        fprintf(stderr, "kdedimmer is already running\n");
        return 1;
    }

    QList<DimOverlay*> overlays;

    for (QScreen *screen : app.screens()) {
        auto *overlay = new DimOverlay();
        overlay->setGeometry(screen->geometry());
        overlay->show();
        overlays.append(overlay);
    }

    TrayController controller(overlays);

    return app.exec();
}

#include "main.moc"
