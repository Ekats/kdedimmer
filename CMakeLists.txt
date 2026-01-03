cmake_minimum_required(VERSION 3.16)
project(kdedimmer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui WaylandClient DBus)
find_package(LayerShellQt REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED wayland-client)

add_executable(kdedimmer main.cpp)

# Get Qt6 private headers path
get_target_property(_qt6_gui_include Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
list(GET _qt6_gui_include 0 _qt6_gui_include_dir)
set(Qt6Gui_PRIVATE_INCLUDE_DIRS "${_qt6_gui_include_dir}/${Qt6_VERSION}/QtGui")

target_include_directories(kdedimmer PRIVATE
    ${WAYLAND_INCLUDE_DIRS}
    ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(kdedimmer
    Qt6::Widgets
    Qt6::WaylandClient
    Qt6::DBus
    LayerShellQt::Interface
    ${WAYLAND_LIBRARIES}
)

# Install targets
install(TARGETS kdedimmer DESTINATION bin)
install(FILES kdedimmer.service DESTINATION lib/systemd/user)
