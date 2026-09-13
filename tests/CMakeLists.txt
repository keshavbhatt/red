find_package(Qt6 ${RED_QT_MIN_VERSION} REQUIRED COMPONENTS Test Qml)

set(RED_TEST_ENV
    "QT_QPA_PLATFORM=offscreen"
    "QT_LOGGING_RULES=red.*.debug=false"
    "QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu"
    "QT_FORCE_STDERR_LOGGING=1"
)

set(RED_SNAP_RUNTIME "" CACHE PATH "kf6-core24 runtime snap root for running tests (dev builds)")
set(RED_SNAP_RUNTIME_LIB_PATH "" CACHE STRING "LD_LIBRARY_PATH for the snap runtime (dev builds)")
if(RED_SNAP_RUNTIME)
    list(APPEND RED_TEST_ENV
        "LD_LIBRARY_PATH=${RED_SNAP_RUNTIME_LIB_PATH}"
        "QT_PLUGIN_PATH=${RED_SNAP_RUNTIME}/usr/lib/x86_64-linux-gnu/qt6/plugins"
        "QTWEBENGINEPROCESS_PATH=${RED_SNAP_RUNTIME}/usr/lib/qt6/libexec/QtWebEngineProcess"
        "QTWEBENGINE_RESOURCES_PATH=${RED_SNAP_RUNTIME}/usr/share/qt6/resources"
    )
endif()

function(red_add_test name source link_target)
    qt_add_executable(${name} ${source})
    set_target_properties(${name} PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    red_set_warnings(${name})
    target_link_libraries(${name} PRIVATE ${link_target} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${RED_TEST_ENV}")
    if(ARGC GREATER 3 AND ARGV3 STREQUAL "GUI")
        set_tests_properties(${name} PROPERTIES LABELS "gui" TIMEOUT 120)
    else()
        set_tests_properties(${name} PROPERTIES LABELS "unit" TIMEOUT 30)
    endif()
endfunction()

# core
red_add_test(tst_settings            tst_settings.cpp            red_core)
red_add_test(tst_legacy_account      tst_legacy_account.cpp      red_core)
red_add_test(tst_daily_allowance     tst_daily_allowance.cpp     red_core)
red_add_test(tst_changelog           tst_changelog.cpp           red_core)
target_compile_definitions(tst_changelog PRIVATE
    RED_CHANGELOG_PATH="${PROJECT_SOURCE_DIR}/CHANGELOG.md"
    RED_VERSION="${PROJECT_VERSION}")
red_add_test(tst_youtube_url         tst_youtube_url.cpp         red_core)
red_add_test(tst_navigation_policy   tst_navigation_policy.cpp   red_core)
red_add_test(tst_block_list          tst_block_list.cpp          red_core)
red_add_test(tst_sponsorblock        tst_sponsorblock.cpp        red_core)
red_add_test(tst_media_info          tst_media_info.cpp          red_core)
red_add_test(tst_download_options    tst_download_options.cpp    red_core)
red_add_test(tst_ytdlp_output        tst_ytdlp_output.cpp        red_core)
red_add_test(tst_download_queue      tst_download_queue.cpp      red_core)
red_add_test(tst_engine_spec         tst_engine_spec.cpp         red_core)
red_add_test(tst_youtube_pref        tst_youtube_pref.cpp        red_core)
red_add_test(tst_zoom_policy         tst_zoom_policy.cpp         red_core)
red_add_test(tst_render_crash_policy tst_render_crash_policy.cpp red_core)
red_add_test(tst_graphics_fallback   tst_graphics_fallback.cpp   red_core)
red_add_test(tst_gpu_storm_detector  tst_gpu_storm_detector.cpp  red_core)
red_add_test(tst_log_sink            tst_log_sink.cpp            red_core)
red_add_test(tst_storage_and_flags   tst_storage_and_flags.cpp   red_core)
red_add_test(tst_theme_service       tst_theme_service.cpp       red_core GUI)
red_add_test(tst_notification_service tst_notification_service.cpp red_core GUI)
# services
red_add_test(tst_netscape_cookies    tst_netscape_cookies.cpp    red_services)
target_link_libraries(tst_netscape_cookies PRIVATE Qt6::Sql)
red_add_test(tst_dislikes_client     tst_dislikes_client.cpp     red_services)
red_add_test(tst_engine_manager      tst_engine_manager.cpp      red_services)
set_tests_properties(tst_engine_manager PROPERTIES TIMEOUT 900)
# platform
if(LINUX)
    red_add_test(tst_freedesktop_notifier tst_freedesktop_notifier.cpp red_platform GUI)
endif()
# web
red_add_test(tst_user_agent          tst_user_agent.cpp          red_web)
red_add_test(tst_web_scripts         tst_web_scripts.cpp         red_web GUI)
target_link_libraries(tst_web_scripts PRIVATE Qt6::Qml)
# app / ui
red_add_test(tst_cli_options         tst_cli_options.cpp         red_app)
red_add_test(tst_single_instance     tst_single_instance.cpp     red_app)
red_add_test(tst_smoke               tst_smoke.cpp               red_ui GUI)
