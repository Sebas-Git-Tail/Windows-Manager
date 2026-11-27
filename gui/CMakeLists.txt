# Windows-Manager: versión para Windows del Manager (descargas de videos y música, y respaldo).
# Se compila con ..\Compilar-en-Windows.bat (Qt 6 y MinGW). Los .ui se editan con Qt Designer.

cmake_minimum_required(VERSION 3.21)
project(WindowsManager VERSION 1.7.0 LANGUAGES C CXX)
# Las pruebas (tests/) corren en Linux: ahí no hay compilador de recursos de Windows
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Lo que no depende de Windows (se prueba en Linux: ../tests)
add_library(windows_manager_logica STATIC
    Herramientas.cpp Herramientas.h
    LogicaRespaldo.cpp LogicaRespaldo.h
    LogicaDescargas.cpp LogicaDescargas.h
    Winget.cpp Winget.h
)
target_include_directories(windows_manager_logica PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(windows_manager_logica PUBLIC Qt6::Core)

option(WM_PRUEBAS "Compilar solo la lógica y sus pruebas (en Linux)" OFF)
if(WM_PRUEBAS)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
    return()
endif()

# Código compartido (repo comun-qt, traído con git subtree a ../comun): diálogo de pistas, consulta a
# yt-dlp, resumen de rsync, datos del servidor (servidor.json) y aviso de versiones nuevas
set(COMUN_DESCARGAS ON)
set(COMUN_RED ON)
set(COMUN_APPS_WIDGETS ON)   # actualizarse sola e instalar las otras apps (comun/apps)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../comun ${CMAKE_CURRENT_BINARY_DIR}/comun)

add_executable(Windows-Manager WIN32
    main.cpp
    VentanaPrincipal.cpp VentanaPrincipal.h VentanaPrincipal.ui
    PaginaDescargas.cpp PaginaDescargasServidor.cpp PaginaDescargas.h PaginaDescargas.ui
    PaginaRespaldo.cpp PaginaRespaldo.h PaginaRespaldo.ui
    PaginaProgramas.cpp PaginaProgramas.h PaginaProgramas.ui
    Recursos.qrc
    Windows-Manager.rc
)
target_link_libraries(Windows-Manager PRIVATE Qt6::Widgets Qt6::Network windows_manager_logica comun_descargas comun_red comun_apps_widgets comun_ui)
target_compile_definitions(Windows-Manager PRIVATE VERSION_PROGRAMA="${PROJECT_VERSION}")
comun_novedades(Windows-Manager ${CMAKE_CURRENT_SOURCE_DIR}/../NOVEDADES.md)   # qué cambió en cada versión

# Le entrega a ssh la contraseña del respaldo. De consola y sin dependencias
add_executable(askpass askpass.cpp)
target_link_options(askpass PRIVATE -static)
