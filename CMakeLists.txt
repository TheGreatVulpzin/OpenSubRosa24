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
    hook/console.c
    hook/hooks_024d.c
    hook/verify.c
    hook/record.c
    hook/capture.c
    hook/imports.c
    hook/hooks_lifted.c
    hook/hooks_record.c
    hook/record_imports.c
    src/lifted/lifted_024d.c
    src/lifted/x87_intrinsics.c
    hook/wsock32.def
    src/server/events.c
    src/server/players.c
    src/server/main_loop.c
)
set_target_properties(sv_wsock32 PROPERTIES OUTPUT_NAME wsock32)
target_include_directories(sv_wsock32 PRIVATE src/include src/lifted hook)
# Lifted Ghidra code: x87 math like the original (/arch:IA32), no warnings (decompiler style).
set_source_files_properties(src/lifted/lifted_024d.c PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013")
set_source_files_properties(hook/hooks_lifted.c hook/hooks_record.c src/lifted/x87_intrinsics.c src/lifted/lifted_table.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")
target_compile_options(sv_wsock32 PRIVATE /W4 /WX /utf-8)
target_compile_definitions(sv_wsock32 PRIVATE _CRT_SECURE_NO_WARNINGS)
# Static CRT: the DLL does not depend on a redistributable.
set_property(TARGET sv_wsock32 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(check_types src/tests/check_types.c)
target_include_directories(check_types PRIVATE src/include)
target_compile_options(check_types PRIVATE /W4 /WX /utf-8)

add_executable(test_verify src/tests/test_verify.c hook/verify.c hook/hook.c hook/console.c hook/record.c)
target_include_directories(test_verify PRIVATE src/include hook)
target_compile_options(test_verify PRIVATE /W4 /WX /utf-8)
target_compile_definitions(test_verify PRIVATE _CRT_SECURE_NO_WARNINGS)

# Offline differential test: loads the 024d as a library, fills .data from a snapshot and
# compares each lifted function with the original on recorded calls (src/tests/harness.c).
add_executable(harness src/tests/harness.c hook/verify.c hook/hook.c hook/console.c hook/record.c hook/imports.c
    src/tests/harness_ui.c src/platform/plat_test.c src/lifted/lifted_024d.c src/lifted/lifted_table.c src/lifted/x87_intrinsics.c)
target_include_directories(harness PRIVATE src/include src/lifted src/platform hook)
target_compile_options(harness PRIVATE /W4 /WX /utf-8)
target_compile_definitions(harness PRIVATE _CRT_SECURE_NO_WARNINGS SV_MAX_PAGES=49152)
target_link_options(harness PRIVATE /LARGEADDRESSAWARE)
set_property(TARGET harness PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# The harness stays out of the way of the 024d test copy (0x20000000..0x45f3b000, tools/re/fixed_base.py).
target_link_options(harness PRIVATE /BASE:0x50000000 /DYNAMICBASE:NO /FIXED:NO)
