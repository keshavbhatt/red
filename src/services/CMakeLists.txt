# services: network-backed and process-backed services (engine provisioning,
# yt-dlp probing, SponsorBlock). Links core + Qt6::Network (+ Qt6::Sql to read
# the profile's cookie database). No widgets, no WebEngine.

qt_add_library(red_services STATIC
    engine_manager.cpp
    engine_manager.h
    licensing/license_config.h
    licensing/license_service.cpp
    licensing/license_service.h
    dislikes_client.cpp
    dislikes_client.h
    logging.cpp
    logging.h
    media_probe.cpp
    media_probe.h
    netscape_cookies.cpp
    netscape_cookies.h
    sponsorblock_client.cpp
    sponsorblock_client.h
)
red_set_warnings(red_services)

target_link_libraries(red_services
    PUBLIC
        red_core
        account_license
        Qt6::Network
    PRIVATE
        Qt6::Sql
)
