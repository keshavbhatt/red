# web: everything that touches Qt WebEngine. Depends on core, services (cookie
# export), platform. Injected JavaScript lives in scripts/ as resource files —
# never as C++ strings.

qt_add_library(red_web STATIC
    bridge.cpp
    bridge.h
    cookie_exporter.cpp
    cookie_exporter.h
    error_page.cpp
    error_page.h
    full_screen_hint.cpp
    full_screen_hint.h
    logging.cpp
    logging.h
    permission_controller.cpp
    permission_controller.h
    popup_window.cpp
    popup_window.h
    script_dialogs.h
    request_interceptor.cpp
    request_interceptor.h
    script_bundle.cpp
    script_bundle.h
    user_agent.cpp
    user_agent.h
    web_page.cpp
    web_page.h
    web_profile.cpp
    web_profile.h
    swipe_indicator.cpp
    swipe_indicator.h
    web_view.cpp
    web_view.h
)
red_set_warnings(red_web)

qt_add_resources(red_web "scripts"
    PREFIX "/scripts"
    BASE "scripts"
    FILES
        scripts/bootstrap.js
        scripts/hooks.js
        scripts/adblock.js
        scripts/sponsorblock.js
        scripts/media-session.js
        scripts/hide-shorts.js
        scripts/codec-preference.js
        scripts/background-play.js
        scripts/theme-control.js
        scripts/mini-player.js
        scripts/dislikes.js
        scripts/tv/h5vcc.js
        scripts/tv/identification.js
        scripts/tv/leanback-fixes.js
        scripts/tv/input.js
        scripts/tv/gamepad.js
        scripts/music/download-menu.js
        scripts/music/mini-player.js
)

target_link_libraries(red_web
    PUBLIC
        red_core
        red_services
        red_platform
        Qt6::WebEngineWidgets
        Qt6::WebChannel
        Qt6::Network
)
