# ui: QtWidgets layer. May use everything. Nothing depends on it except app/.
# No .ui files — widgets are built in code (see CODING_STANDARDS.md §5).

qt_add_library(red_ui STATIC
    about_dialog.cpp
    account_dialog.cpp
    account_dialog.h
    about_dialog.h
    actions.cpp
    actions.h
    diagnostics.cpp
    diagnostics.h
    download_card_delegate.cpp
    download_card_delegate.h
    download_dialog.cpp
    download_dialog.h
    kind_card.cpp
    kind_card.h
    message_sheet.cpp
    message_sheet.h
    downloads_hub.cpp
    downloads_hub.h
    downloads_panel.cpp
    downloads_panel.h
    engine_setup_dialog.cpp
    engine_setup_dialog.h
    icons.cpp
    icons.h
    logging.cpp
    logging.h
    main_window.cpp
    main_window.h
    media_controller.cpp
    media_controller.h
    permission_prompt.cpp
    permission_prompt.h
    red_style.cpp
    red_style.h
    settings_dialog.cpp
    settings_dialog.h
    shortcuts_dialog.cpp
    shortcuts_dialog.h
    side_rail.cpp
    side_rail.h
    theme_applier.cpp
    theme_applier.h
    thumbnail_cache.cpp
    thumbnail_cache.h
    tray_controller.cpp
    tray_controller.h
    whats_new_dialog.cpp
    whats_new_dialog.h
)
red_set_warnings(red_ui)

set(RED_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../resources/icons")
file(GLOB RED_UI_ICONS "${RED_ICON_DIR}/ui/*.svg")
qt_add_resources(red_ui "icons"
    PREFIX "/icons"
    BASE "${RED_ICON_DIR}"
    FILES
        "${RED_ICON_DIR}/red.svg"
        "${RED_ICON_DIR}/red-symbolic.svg"
        "${RED_ICON_DIR}/hicolor/64x64/apps/com.ktechpit.red.png"
        "${RED_ICON_DIR}/hicolor/128x128/apps/com.ktechpit.red.png"
        "${RED_ICON_DIR}/hicolor/256x256/apps/com.ktechpit.red.png"
        ${RED_UI_ICONS}
)

# The changelog feeds the "What's new" dialog (:/text/CHANGELOG.md).
qt_add_resources(red_ui "text"
    PREFIX "/text"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
)

target_link_libraries(red_ui
    PUBLIC
        red_web
        red_services
        red_platform
        Qt6::Widgets
        Qt6::Svg
        Qt6::Concurrent
)
