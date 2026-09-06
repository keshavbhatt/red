# Layered libraries. Dependency direction is enforced by target_link_libraries:
#
#   ui ──► web ──► services ─► core
#   │        │
#   └──► platform ─► core
#
# Nothing links to `ui` except the executable (via `app`).

add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(platform)
add_subdirectory(web)
add_subdirectory(ui)
add_subdirectory(app)

qt_add_executable(red
    main.cpp
)
red_set_warnings(red)
target_link_libraries(red PRIVATE red_app)

if(LINUX)
    # QtWebEngine drags in libproxy, whose backends are not always resolvable at
    # link time in the KDE Qt6 content snaps; those symbols come from shared
    # libraries and resolve at runtime.
    target_link_options(red PRIVATE -Wl,--allow-shlib-undefined)
endif()

set_target_properties(red PROPERTIES
    # Export symbols (-rdynamic) so the crash handler's backtrace shows function
    # names instead of bare addresses.
    ENABLE_EXPORTS ON
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    # Pure QtWidgets app: no QML anywhere, skip the qmlimportscanner finalizer.
    QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)

install(TARGETS red
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
