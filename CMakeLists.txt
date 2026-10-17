cmake_minimum_required(VERSION 3.20)
project(SubVulpes C)

# Hook DLL for subrosadedicated024d.exe (Windows x86, MSVC).
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "The hook DLL needs MSVC x86 (32-bit). Use tools/build_hook.bat.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Functions rewritten by hand (Phase 6). tools/lift/lift.py finds each one by its real name
# and lifts it no more. Run cmake again after a new file. src/common and src/net hold clean
# code. src/game still holds files in the lifted style (no warnings until they are clean).
file(GLOB SV_CLEAN_SOURCES CONFIGURE_DEPENDS src/common/*.c src/net/*.c)
file(GLOB SV_GAME_SOURCES CONFIGURE_DEPENDS src/game/*.c)
set(SV_REWRITTEN_SOURCES ${SV_CLEAN_SOURCES} ${SV_GAME_SOURCES})
set(SV_REWRITTEN_INCLUDES src/common src/net src/game)

add_library(sv_wsock32 SHARED
    hook/proxy.c
    hook/netguard.c
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
    src/server/accounts.c
    src/server/events.c
    src/server/players.c
    src/server/main_loop.c
    ${SV_REWRITTEN_SOURCES}
)
set_target_properties(sv_wsock32 PROPERTIES OUTPUT_NAME wsock32)
target_include_directories(sv_wsock32 PRIVATE src/include src/lifted hook ${SV_REWRITTEN_INCLUDES})
# Lifted Ghidra code: x87 math like the original (/arch:IA32), no warnings (decompiler style).
set_source_files_properties(src/lifted/lifted_024d.c PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013")
# Code rewritten by hand from the lifted C (Phase 6): x87 math like the original. The warnings
# come back on when a file has no lifted style left.
set_source_files_properties(${SV_GAME_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013;/Z7")
# Clean code: x87 math like the original, with the warnings of each target.
set_source_files_properties(${SV_CLEAN_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:IA32;/Z7")
set_source_files_properties(hook/hooks_lifted.c hook/hooks_record.c src/lifted/x87_intrinsics.c src/lifted/lifted_table.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")
target_compile_options(sv_wsock32 PRIVATE /W4 /WX /utf-8)
target_compile_definitions(sv_wsock32 PRIVATE _CRT_SECURE_NO_WARNINGS)
# Static CRT: the DLL does not depend on a redistributable.
set_property(TARGET sv_wsock32 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Map file (build/hook/wsock32.map): turns a crash offset "WSOCK32.dll+0x..." into a function name.
target_link_options(sv_wsock32 PRIVATE /MAP)

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
    src/tests/harness_ui.c src/platform/plat_test.c src/server/accounts.c src/lifted/lifted_024d.c src/lifted/lifted_table.c src/lifted/x87_intrinsics.c ${SV_REWRITTEN_SOURCES})
target_include_directories(harness PRIVATE src/include src/lifted src/platform hook ${SV_REWRITTEN_INCLUDES})
target_compile_options(harness PRIVATE /W4 /WX /utf-8)
target_compile_definitions(harness PRIVATE _CRT_SECURE_NO_WARNINGS SV_MAX_PAGES=49152)
target_link_options(harness PRIVATE /LARGEADDRESSAWARE /MAP)
set_property(TARGET harness PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# The harness stays out of the way of the 024d test copy (0x20000000..0x45f3b000, tools/re/fixed_base.py).
target_link_options(harness PRIVATE /BASE:0x50000000 /DYNAMICBASE:NO /FIXED:NO)

# Phase 5.2: the CRT of the game (MSVCR100.dll) against the new CRT (ucrtbase.dll).
add_executable(crt_diff src/tests/crt_diff.c)
target_compile_options(crt_diff PRIVATE /W4 /utf-8)
target_compile_definitions(crt_diff PRIVATE _CRT_SECURE_NO_WARNINGS)

# Input checks of the proxy (hook/netguard.c): packets made by tools/net/srpacket.py.
add_executable(test_netguard src/tests/test_netguard.c hook/netguard.c)
target_include_directories(test_netguard PRIVATE hook)
target_compile_options(test_netguard PRIVATE /W4 /WX /utf-8)

# Phase 5: the dedicated server without the 024d (src/standalone/main.c). The same lifted and
# hand-written C as the hook DLL, with SR_STANDALONE: game calls go to the C functions.
add_executable(subvulpes-server src/standalone/main.c src/standalone/fn_table.c src/standalone/image_024d.c
    src/lifted/lifted_024d.c src/lifted/lifted_table.c src/lifted/x87_intrinsics.c ${SV_REWRITTEN_SOURCES}
    src/server/accounts.c src/server/events.c src/server/players.c src/server/main_loop.c
    src/platform/plat_imports.c)
target_include_directories(subvulpes-server PRIVATE src/include src/lifted src/standalone src/platform hook ${SV_REWRITTEN_INCLUDES})
target_compile_options(subvulpes-server PRIVATE /W4 /WX /utf-8)
target_compile_definitions(subvulpes-server PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_link_options(subvulpes-server PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_property(TARGET subvulpes-server PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The harness recording build of the standalone server (src/standalone/record_shim.c). The lifted
# code is compiled again with /Gh (_penter at each function), in its own object library: the
# other targets keep the normal code.
add_library(game_record OBJECT src/lifted/lifted_024d.c)
target_include_directories(game_record PRIVATE src/include src/lifted src/standalone ${SV_REWRITTEN_INCLUDES})
target_compile_options(game_record PRIVATE /Gh)
target_compile_definitions(game_record PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
set_property(TARGET game_record PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_executable(subvulpes-server-record src/standalone/main.c src/standalone/fn_table.c src/standalone/image_024d.c
    $<TARGET_OBJECTS:game_record> src/lifted/lifted_table.c src/lifted/x87_intrinsics.c ${SV_REWRITTEN_SOURCES}
    src/server/accounts.c src/server/events.c src/server/players.c src/server/main_loop.c
    src/platform/plat_imports.c src/standalone/record_shim.c hook/record.c hook/record_imports.c)
target_include_directories(subvulpes-server-record PRIVATE src/include src/lifted src/standalone src/platform hook ${SV_REWRITTEN_INCLUDES})
target_compile_options(subvulpes-server-record PRIVATE /W4 /WX /utf-8)
target_compile_definitions(subvulpes-server-record PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE SR_RECORD)
target_link_options(subvulpes-server-record PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG /INCREMENTAL:NO)
set_property(TARGET subvulpes-server-record PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
