cmake_minimum_required(VERSION 3.20)
project(SubVulpes C)

# Hook DLL for subrosadedicated024d.exe (Windows x86, MSVC).
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "The hook DLL needs MSVC x86 (32-bit). Use tools/build_hook.bat.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(sv_wsock32 SHARED
    hook/proxy.c
    hook/hook.c
    hook/hooks_024d.c
    hook/verify.c
    hook/wsock32.def
    src/server/events.c
    src/server/players.c
)
set_target_properties(sv_wsock32 PROPERTIES OUTPUT_NAME wsock32)
target_include_directories(sv_wsock32 PRIVATE src/include hook)
target_compile_options(sv_wsock32 PRIVATE /W4 /WX /utf-8)
target_compile_definitions(sv_wsock32 PRIVATE _CRT_SECURE_NO_WARNINGS)
# Static CRT: the DLL does not depend on a redistributable.
set_property(TARGET sv_wsock32 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(check_types src/tests/check_types.c)
target_include_directories(check_types PRIVATE src/include)
target_compile_options(check_types PRIVATE /W4 /WX /utf-8)

add_executable(test_verify src/tests/test_verify.c hook/verify.c hook/hook.c)
target_include_directories(test_verify PRIVATE src/include hook)
target_compile_options(test_verify PRIVATE /W4 /WX /utf-8)
target_compile_definitions(test_verify PRIVATE _CRT_SECURE_NO_WARNINGS)
