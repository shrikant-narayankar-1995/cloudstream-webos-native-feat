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
        src/core/child_process.cpp
        src/core/sha256.cpp
        src/core/zip_check.cpp
        src/plugins/plugin_store.cpp
        src/plugins/repository.cpp
        src/runtime/runtime_client.cpp
        src/runtime/runtime_launcher.cpp
        src/runtime/runtime_types.cpp)
target_include_directories(csweb_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(csweb_core SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party")
target_link_libraries(csweb_core PUBLIC wnk_storage wnk_logging Threads::Threads)

# ---- tests (host) -----------------------------------------------------------------------------------
if (WNK_BUILD_TESTS)
    foreach (t sha256 zip_check repository runtime_types plugin_store runtime_client)
        add_executable(csweb_test_${t} tests/test_${t}.cpp)
        target_include_directories(csweb_test_${t} PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit")
        target_compile_definitions(csweb_test_${t} PRIVATE CSWEB_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        target_link_libraries(csweb_test_${t} PRIVATE csweb_core)
        add_test(NAME csweb.${t} COMMAND csweb_test_${t})
    endforeach ()
endif ()
