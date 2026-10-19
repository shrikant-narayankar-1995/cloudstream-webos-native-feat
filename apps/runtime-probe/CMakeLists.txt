# SPDX-License-Identifier: GPL-3.0-or-later
# Runtime feasibility probe for the CloudStream webOS project. Built inside the kit's tree through
# WNK_EXTRA_APP_DIRS (the kit's cmake/ExtraApps.cmake).
add_executable(wnk-csprobe main.cpp child_probe.cpp a64_elf.cpp)
target_link_libraries(wnk-csprobe PRIVATE wnk_app)
target_include_directories(wnk-csprobe SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party")

set(WNK_FONT_DIR "${PROJECT_SOURCE_DIR}/assets/fonts")
add_custom_command(TARGET wnk-csprobe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${WNK_FONT_DIR}" "$<TARGET_FILE_DIR:wnk-csprobe>/fonts"
        VERBATIM)

# The plugin runtime (runtime/make_dist.py) and the test plugins (fetch-testplugins.sh) are optional:
# without them the probe runs its platform checks only.
get_filename_component(_csweb_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(_csweb_runtimes)
set(_csweb_extra_dirs)
if (WNK_WEBOS AND EXISTS "${_csweb_root}/runtime/dist/runtime-aarch64/lib/ld-musl-aarch64.so.1")
    list(APPEND _csweb_runtimes "${_csweb_root}/runtime/dist/runtime-aarch64")
endif ()
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/testplugins/tests.json")
    list(APPEND _csweb_extra_dirs "${CMAKE_CURRENT_SOURCE_DIR}/testplugins")
endif ()

wnk_add_webos_package(
        TARGET wnk-csprobe
        APPID org.csweb.runtimeprobe
        TITLE "CS runtime probe"
        VERSION 0.5.0
        VENDOR "cloudstream-webos"
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
        LARGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/largeIcon.png"
        ICON_COLOR "#7c2d12"
        EXPECTED_NEEDED "${CMAKE_CURRENT_SOURCE_DIR}/expected-needed.txt"
        EXTRA_DIRS "${WNK_FONT_DIR}" ${_csweb_extra_dirs}
        PRIVATE_RUNTIMES ${_csweb_runtimes})

if (WNK_BUILD_TESTS)
    # The child probe on the host: exec mappings, entropy and threads must work on a normal Linux.
    add_test(NAME csweb.probe.child_host
            COMMAND wnk-csprobe --child-probe "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" nonet)
    set_tests_properties(csweb.probe.child_host PROPERTIES
            PASS_REGULAR_EXPRESSION "child\\.jit_wx=ok.*child\\.file_exec_map_tmp=ok.*child\\.threads=64/64.*child\\.done=1"
            FAIL_REGULAR_EXPRESSION "jit_wx=FAIL|file_exec_map_tmp=FAIL")
endif ()
