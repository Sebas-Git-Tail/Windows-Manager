# Pruebas de la lógica de Windows-Manager que no depende de Windows (corren en Linux).
#   cmake -S gui -B build-pruebas -G Ninja -DWM_PRUEBAS=ON && cmake --build build-pruebas
#   QT_QPA_PLATFORM=offscreen ctest --test-dir build-pruebas --output-on-failure
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_AUTOMOC ON)
function(prueba nombre)
    add_executable(${nombre} ${nombre}.cpp)
    target_link_libraries(${nombre} PRIVATE Qt6::Test windows_manager_logica)
    add_test(NAME ${nombre} COMMAND ${nombre})
    set_tests_properties(${nombre} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()
prueba(tst_respaldo)
prueba(tst_winget)
prueba(tst_descargas_wm)
