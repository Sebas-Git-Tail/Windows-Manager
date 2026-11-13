# Windows-Manager: versión para Windows del Manager (descargas de videos y música, y respaldo).
# Se compila con ..\Compilar-en-Windows.bat (Qt 6 y MinGW). Los .ui se editan con Qt Designer.

cmake_minimum_required(VERSION 3.21)
project(WindowsManager VERSION 1.1.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Código compartido (repo comun-qt, traído con git subtree a ../comun): diálogo de pistas, consulta a
# yt-dlp, resumen de rsync, datos del servidor (servidor.json) y aviso de versiones nuevas
set(COMUN_DESCARGAS ON)
set(COMUN_RED ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../comun ${CMAKE_CURRENT_BINARY_DIR}/comun)

add_executable(Windows-Manager WIN32
    main.cpp
    Herramientas.cpp Herramientas.h
    VentanaPrincipal.cpp VentanaPrincipal.h VentanaPrincipal.ui
    PaginaDescargas.cpp PaginaDescargas.h PaginaDescargas.ui
    PaginaRespaldo.cpp PaginaRespaldo.h PaginaRespaldo.ui
    Recursos.qrc
    Windows-Manager.rc
)
target_link_libraries(Windows-Manager PRIVATE Qt6::Widgets Qt6::Network comun_descargas comun_red)
target_compile_definitions(Windows-Manager PRIVATE VERSION_PROGRAMA="${PROJECT_VERSION}")

# Le entrega a ssh la contraseña del respaldo. De consola y sin dependencias
add_executable(askpass askpass.cpp)
target_link_options(askpass PRIVATE -static)
