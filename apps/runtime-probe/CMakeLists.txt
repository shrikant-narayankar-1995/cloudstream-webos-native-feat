# SPDX-License-Identifier: GPL-3.0-or-later
# Runtime feasibility probe for the CloudStream webOS project. Built inside the kit's tree through
# WNK_EXTRA_APP_DIRS (docs/kit-patches/0001-extra-app-dirs.md).
add_executable(wnk-csprobe main.cpp child_probe.cpp a64_elf.cpp)
target_link_libraries(wnk-csprobe PRIVATE wnk_app)
target_include_directories(wnk-csprobe SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party")

set(WNK_FONT_DIR "${PROJECT_SOURCE_DIR}/assets/fonts")
add_custom_command(TARGET wnk-csprobe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${WNK_FONT_DIR}" "$<TARGET_FILE_DIR:wnk-csprobe>/fonts"
        VERBATIM)

wnk_add_webos_package(
        TARGET wnk-csprobe
        APPID org.csweb.runtimeprobe
        TITLE "CS runtime probe"
        VERSION 0.1.0
        VENDOR "cloudstream-webos"
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
        LARGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/largeIcon.png"
        ICON_COLOR "#7c2d12"
        EXPECTED_NEEDED "${CMAKE_CURRENT_SOURCE_DIR}/expected-needed.txt"
        EXTRA_DIRS "${WNK_FONT_DIR}")

if (WNK_BUILD_TESTS)
    # The child probe on the host: exec mappings, entropy and threads must work on a normal Linux.
    add_test(NAME csweb.probe.child_host
            COMMAND wnk-csprobe --child-probe "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" nonet)
    set_tests_properties(csweb.probe.child_host PROPERTIES
            PASS_REGULAR_EXPRESSION "child\\.jit_wx=ok.*child\\.file_exec_map_tmp=ok.*child\\.threads=64/64.*child\\.done=1"
            FAIL_REGULAR_EXPRESSION "jit_wx=FAIL|file_exec_map_tmp=FAIL")
endif ()
