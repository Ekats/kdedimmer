# Maintainer: Your Name <your.email@example.com>
pkgname=kdedimmer
pkgver=1.0.0
pkgrel=1
pkgdesc="Click-through screen dimmer overlay for KDE Plasma Wayland"
arch=('x86_64')
url="https://github.com/YOURUSERNAME/kdedimmer"
license=('GPL-3.0-or-later')
depends=('qt6-base' 'qt6-wayland' 'layer-shell-qt' 'wayland')
makedepends=('cmake')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$pkgname-$pkgver"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    cd "$pkgname-$pkgver"
    install -Dm755 build/kdedimmer "$pkgdir/usr/bin/kdedimmer"
    install -Dm644 kdedimmer.service "$pkgdir/usr/lib/systemd/user/kdedimmer.service"
}
