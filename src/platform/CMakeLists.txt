# platform: OS-specific backends behind interfaces declared in core/.
# Implementations are selected here per OS — never with #ifdef inside classes
# (the notifier factory is the one sanctioned switch).

qt_add_library(red_platform STATIC
    crash_handler.h
    file_manager.h
    gpu_stderr_watch.h
    logging.cpp
    logging.h
    mpris_service.h
    notifier_factory.cpp
    notifier_factory.h
    platform_info.cpp
    platform_info.h
    screen_inhibitor.h
    taskbar_progress.h
)
red_set_warnings(red_platform)

target_link_libraries(red_platform
    PUBLIC
        red_core
)

if(LINUX)
    find_package(Qt6 ${RED_QT_MIN_VERSION} REQUIRED COMPONENTS DBus)
    target_sources(red_platform PRIVATE
        linux/crash_handler_posix.cpp
        linux/dbus_image.cpp
        linux/dbus_image.h
        linux/file_manager_dbus.cpp
        linux/gpu_stderr_watch.cpp
        linux/freedesktop_notifier.cpp
        linux/freedesktop_notifier.h
        linux/portal_notifier.cpp
        linux/portal_notifier.h
        linux/mpris_service_dbus.cpp
        linux/mpris_service_dbus.h
        linux/mpris_adaptors.cpp
        linux/mpris_adaptors.h
        linux/screen_inhibitor_dbus.cpp
        linux/taskbar_progress_dbus.cpp
    )
    target_link_libraries(red_platform PUBLIC Qt6::DBus)
else()
    target_sources(red_platform PRIVATE
        crash_handler_generic.cpp
        file_manager_generic.cpp
        gpu_stderr_watch_generic.cpp
        mpris_service_generic.cpp
        screen_inhibitor_generic.cpp
        taskbar_progress_generic.cpp
    )
endif()
