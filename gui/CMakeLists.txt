# Windows-Manager: versión para Windows del Manager (descargas de videos y música, y respaldo).
# Se compila con ..\Compilar-en-Windows.bat (Qt 6 y MinGW). Los .ui se editan con Qt Designer.

cmake_minimum_required(VERSION 3.21)
project(WindowsManager VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(Windows-Manager WIN32
    main.cpp
    Herramientas.cpp Herramientas.h
    VentanaPrincipal.cpp VentanaPrincipal.h VentanaPrincipal.ui
    PaginaDescargas.cpp PaginaDescargas.h PaginaDescargas.ui
    PaginaRespaldo.cpp PaginaRespaldo.h PaginaRespaldo.ui
    ResumenSimulacion.cpp ResumenSimulacion.h
    Recursos.qrc
    Windows-Manager.rc
)
target_link_libraries(Windows-Manager PRIVATE Qt6::Widgets Qt6::Network)

# Le entrega a ssh la contraseña del respaldo. De consola y sin dependencias
add_executable(askpass askpass.cpp)
target_link_options(askpass PRIVATE -static)
