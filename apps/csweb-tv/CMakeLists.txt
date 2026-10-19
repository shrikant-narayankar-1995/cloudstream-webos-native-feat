# SPDX-License-Identifier: GPL-3.0-or-later
# CSWeb TV, the native app (apps/csweb-tv/README.md). Built inside the kit's tree through
# WNK_EXTRA_APP_DIRS (docs/kit-patches/0001-extra-app-dirs.md).

set(CSWEB_APP_ID org.csweb.tv)
set(CSWEB_TITLE "CSWeb TV")
set(CSWEB_VERSION 0.1.0)
get_filename_component(_csweb_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# ---- csweb_core: no UI; runtime client, plugins, persistence, utilities ---------------------------
find_package(Threads REQUIRED)
add_library(csweb_core STATIC
        src/app/deep_link.cpp
        src/core/child_process.cpp
        src/core/dir_snapshot.cpp
        src/core/sha256.cpp
        src/core/stall_sampler.cpp
        src/core/zip_check.cpp
        src/library/library.cpp
        src/library/library_store.cpp
        src/plugins/plugin_data.cpp
        src/plugins/plugin_manager.cpp
        src/plugins/plugin_store.cpp
        src/plugins/repository.cpp
        src/profiles/profiles.cpp
        src/settings/preferences.cpp
        src/settings/settings_store.cpp
        src/runtime/episode_groups.cpp
        src/runtime/provider_test.cpp
        src/runtime/runtime_client.cpp
        src/runtime/runtime_launcher.cpp
        src/runtime/runtime_types.cpp)
target_include_directories(csweb_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(csweb_core SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party")
target_link_libraries(csweb_core PUBLIC wnk_storage wnk_logging wnk_net wnk_async Threads::Threads)

# ---- tests (host) -----------------------------------------------------------------------------------
if (WNK_BUILD_TESTS)
    foreach (t sha256 zip_check repository runtime_types plugin_store runtime_client episode_groups redact deep_link library dir_snapshot preferences provider_test profiles)
        add_executable(csweb_test_${t} tests/test_${t}.cpp)
        target_include_directories(csweb_test_${t} PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit")
        target_compile_definitions(csweb_test_${t} PRIVATE CSWEB_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(csweb_test_${t} PRIVATE csweb_core)
        add_test(NAME csweb.${t} COMMAND csweb_test_${t})
    endforeach ()
endif ()

# ---- csweb_player: link choice, subtitle files, the kit media pipeline for a link ----------------------
if (NOT TARGET wnk_media_ffmpeg)
    message(STATUS "CSWeb TV skipped: configure with WNK_FFMPEG=ON (the player needs FFmpeg)")
    return()
endif ()
add_library(csweb_player STATIC
        src/player/link_priority.cpp
        src/player/media_factory.cpp
        src/player/merged_source.cpp
        src/player/subtitle_file.cpp)
target_link_libraries(csweb_player PUBLIC csweb_core wnk_media wnk_media_ffmpeg wnk_media_ndl wnk_media_starfish)

# ---- csweb-tv: the app ------------------------------------------------------------------------------
add_executable(csweb-tv
        src/main.cpp
        src/app/application.cpp
        src/app/router.cpp
        src/ui/dialogs/multi_select.cpp
        src/ui/dialogs/plugin_dialog_host.cpp
        src/ui/screens/details_screen.cpp
        src/ui/screens/extensions_screen.cpp
        src/ui/screens/home_screen.cpp
        src/ui/screens/library_screen.cpp
        src/ui/screens/player_screen.cpp
        src/ui/screens/profiles_screen.cpp
        src/ui/screens/provider_test_screen.cpp
        src/ui/screens/search_screen.cpp
        src/ui/screens/settings_screen.cpp
        src/ui/screens/setup_screen.cpp
        src/ui/screens/splash_screen.cpp
        src/ui/theme/theme.cpp
        src/ui/widgets/art.cpp
        src/ui/widgets/episode_row.cpp
        src/ui/widgets/glass.cpp
        src/ui/widgets/icons.cpp
        src/ui/widgets/nav_bar.cpp
        src/ui/widgets/pill_button.cpp
        src/ui/widgets/pin_pad.cpp
        src/ui/widgets/profile_avatar.cpp
        src/ui/widgets/top_bar.cpp
        src/ui/widgets/type_chips.cpp)
target_link_libraries(csweb-tv PRIVATE csweb_core csweb_player wnk_app wnk_media_ui)
target_compile_definitions(csweb-tv PRIVATE CSWEB_VERSION="${CSWEB_VERSION}")
# Host runs find the runtime in the repository (runtime/make_dist.py); the TV uses the packaged one.
if (NOT WNK_WEBOS)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE _csweb_arch OUTPUT_STRIP_TRAILING_WHITESPACE)
    target_compile_definitions(csweb-tv PRIVATE CSWEB_HOST_RUNTIME_DIR="${_csweb_root}/runtime/dist/runtime-${_csweb_arch}")
endif ()

set(_csweb_fonts "${PROJECT_SOURCE_DIR}/assets/fonts")
set(_csweb_icons "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons")
set(_csweb_brand "${CMAKE_CURRENT_SOURCE_DIR}/assets/brand")  # tools/make_brand.py
add_custom_command(TARGET csweb-tv POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_csweb_fonts}" "$<TARGET_FILE_DIR:csweb-tv>/fonts"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_csweb_icons}" "$<TARGET_FILE_DIR:csweb-tv>/icons"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_csweb_brand}" "$<TARGET_FILE_DIR:csweb-tv>/brand"
        VERBATIM)

set(_csweb_runtimes)
if (WNK_WEBOS AND EXISTS "${_csweb_root}/runtime/dist/runtime-aarch64/lib/ld-musl-aarch64.so.1")
    list(APPEND _csweb_runtimes "${_csweb_root}/runtime/dist/runtime-aarch64")
elseif (WNK_WEBOS)
    message(WARNING "CSWeb TV: runtime/dist/runtime-aarch64 missing; the package will not run plugins")
endif ()

wnk_add_webos_package(
        TARGET csweb-tv
        APPID ${CSWEB_APP_ID}
        TITLE "${CSWEB_TITLE}"
        VERSION ${CSWEB_VERSION}
        VENDOR "cloudstream-webos"
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/icon.png"
        LARGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/largeIcon.png"
        ICON_COLOR "#0B0D12"
        EXPECTED_NEEDED "${CMAKE_CURRENT_SOURCE_DIR}/packaging/expected-needed.txt"
        EXTRA_DIRS "${_csweb_fonts}" "${_csweb_icons}" "${_csweb_brand}"
        REQUIRED_PERMISSIONS database.operation
        BUNDLED_LIBS ${WNK_FFMPEG_RUNTIME_LIBS}
        PRIVATE_RUNTIMES ${_csweb_runtimes})

if (WNK_BUILD_TESTS)
    # Host tool: a stream URL through the player's source (tools/media_probe.cpp).
    add_executable(csweb-media-probe tools/media_probe.cpp)
    target_link_libraries(csweb-media-probe PRIVATE csweb_player)
    add_executable(csweb_test_glass tests/test_glass.cpp src/ui/widgets/glass.cpp src/ui/theme/theme.cpp)
    target_include_directories(csweb_test_glass PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit" "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(csweb_test_glass PRIVATE wnk_app)
    add_test(NAME csweb.glass COMMAND csweb_test_glass)
    foreach (t link_priority merged_source subtitle_file)
        add_executable(csweb_test_${t} tests/test_${t}.cpp)
        target_include_directories(csweb_test_${t} PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit")
        target_link_libraries(csweb_test_${t} PRIVATE csweb_player)
        add_test(NAME csweb.${t} COMMAND csweb_test_${t})
    endforeach ()
endif ()
