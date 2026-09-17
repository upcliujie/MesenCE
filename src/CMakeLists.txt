add_executable(${PROJECT_NAME}
    main.cpp
    emulationsession.cpp
    emulationsession.h
    frontenddefaults.cpp
    frontenddefaults.h
    cheatdialog.cpp
    cheatdialog.h
    gamesettingsdialog.cpp
    gamesettingsdialog.h
    mainwindow.cpp
    mainwindow.h
    openglvideowidget.cpp
    openglvideowidget.h
    qtkeymanager.cpp
    qtkeymanager.h
    recentfiles.cpp
    recentfiles.h
    recentgameswidget.cpp
    recentgameswidget.h
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/CheatDatabase
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/UI/Dependencies/Internal/CheatDb.Nes.json
        ${PROJECT_SOURCE_DIR}/UI/Dependencies/Internal/CheatDb.Snes.json
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/CheatDatabase
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/Core
    ${PROJECT_SOURCE_DIR}/Utilities
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    mesen_core

    Qt5::Widgets
    SDL2::SDL2
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MESEN_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES mesen.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES
    ${PROJECT_SOURCE_DIR}/UI/Dependencies/Internal/CheatDb.Nes.json
    ${PROJECT_SOURCE_DIR}/UI/Dependencies/Internal/CheatDb.Snes.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/CheatDatabase
)
