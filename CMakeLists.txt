cmake_minimum_required(VERSION 3.20)
project(SubVulpes C)

# Hook DLL for subrosadedicated024d.exe (Windows x86, MSVC).
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "The server and verification tools need MSVC x86 (32-bit). Use tools/build_server.bat or tools/build_hook.bat.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(SV_BUILD_INSTRUMENTATION "Build differential verification and recording tools" ON)

# The normal server and verification targets share the game C sources.
# Verification targets select their memory adapter through a target definition.
file(GLOB SV_CLEAN_SOURCES CONFIGURE_DEPENDS src/common/*.c src/net/*.c src/game/*.c)
# The operating system calls of the clean code, through the imports of the game.
list(APPEND SV_CLEAN_SOURCES src/platform/os_calls.c)
set(SV_GAME_SOURCES)
foreach(f ai_car)
    list(APPEND SV_GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/game/${f}.c)
endforeach()
list(REMOVE_ITEM SV_CLEAN_SOURCES ${SV_GAME_SOURCES})
set(SV_REWRITTEN_SOURCES ${SV_CLEAN_SOURCES} ${SV_GAME_SOURCES})
set(SV_REWRITTEN_INCLUDES src/common src/net src/game)

# Storage ownership changes must rebuild every consumer.
set(SV_STORAGE_HEADERS)
foreach(header sr_types.h sr_runtime.h sr_storage.h sr_storage_types.h sr_limits.h)
    list(APPEND SV_STORAGE_HEADERS "${CMAKE_SOURCE_DIR}/src/include/${header}")
endforeach()
set_property(SOURCE ${SV_REWRITTEN_SOURCES} src/server/accounts.c src/server/events.c
    src/server/players.c src/server/main_loop.c src/server/storage.c
    src/standalone/main.c src/platform/x87_math.c APPEND PROPERTY OBJECT_DEPENDS "${SV_STORAGE_HEADERS}")

set_source_files_properties(${SV_GAME_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013;/Z7")
set_source_files_properties(${SV_CLEAN_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:IA32;/Z7")

if(SV_BUILD_INSTRUMENTATION)

add_library(sv_wsock32 SHARED
    instrumentation/hook/proxy.c
    instrumentation/hook/netguard.c
    instrumentation/hook/hook.c
    instrumentation/hook/console.c
    instrumentation/hook/hooks_024d.c
    instrumentation/hook/verify.c
    instrumentation/hook/record.c
    instrumentation/hook/capture.c
    instrumentation/hook/imports.c
    instrumentation/hook/hooks_lifted.c
    instrumentation/hook/hooks_record.c
    instrumentation/hook/record_imports.c
    instrumentation/runtime/profile.c
    src/lifted/lifted_024d.c
    src/platform/x87_math.c
    instrumentation/hook/wsock32.def
    src/server/accounts.c
    src/server/events.c
    src/server/players.c
    src/server/main_loop.c
    ${SV_REWRITTEN_SOURCES}
)
set_target_properties(sv_wsock32 PROPERTIES OUTPUT_NAME wsock32)
target_include_directories(sv_wsock32 PRIVATE src/include src/lifted instrumentation/hook instrumentation/runtime ${SV_REWRITTEN_INCLUDES})
# Lifted Ghidra code: x87 math like the original (/arch:IA32), no warnings (decompiler style).
set_source_files_properties(src/lifted/lifted_024d.c PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013")
set_source_files_properties(instrumentation/hook/hooks_lifted.c instrumentation/hook/hooks_record.c src/lifted/lifted_table.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")
target_compile_options(sv_wsock32 PRIVATE /W4 /WX /utf-8)
target_compile_definitions(sv_wsock32 PRIVATE _CRT_SECURE_NO_WARNINGS SR_TEST_MEMORY)
# Static CRT: the DLL does not depend on a redistributable.
set_property(TARGET sv_wsock32 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Map file (build/hook/wsock32.map): turns a crash offset "WSOCK32.dll+0x..." into a function name.
target_link_options(sv_wsock32 PRIVATE /MAP)

add_executable(check_types instrumentation/tests/check_types.c)
target_include_directories(check_types PRIVATE src/include)
target_compile_options(check_types PRIVATE /W4 /WX /utf-8)

add_executable(test_verify instrumentation/tests/test_verify.c instrumentation/hook/verify.c instrumentation/hook/hook.c instrumentation/hook/console.c instrumentation/hook/record.c)
target_include_directories(test_verify PRIVATE src/include instrumentation/hook instrumentation/runtime)
target_compile_options(test_verify PRIVATE /W4 /WX /utf-8)
target_compile_definitions(test_verify PRIVATE _CRT_SECURE_NO_WARNINGS SR_TEST_MEMORY)

# Offline differential test: loads the 024d as a library, fills .data from a snapshot and
# compares each lifted function with the original on recorded calls (instrumentation/tests/harness.c).
add_executable(harness instrumentation/tests/harness.c instrumentation/hook/verify.c instrumentation/hook/hook.c instrumentation/hook/console.c instrumentation/hook/record.c instrumentation/hook/imports.c
    instrumentation/tests/harness_ui.c instrumentation/platform/plat_test.c src/server/accounts.c src/server/events.c src/server/players.c src/lifted/lifted_024d.c src/lifted/lifted_table.c src/platform/x87_math.c ${SV_REWRITTEN_SOURCES})
target_include_directories(harness PRIVATE src/include src/lifted src/platform instrumentation/platform instrumentation/hook instrumentation/runtime ${SV_REWRITTEN_INCLUDES})
target_compile_options(harness PRIVATE /W4 /WX /utf-8)
target_compile_definitions(harness PRIVATE _CRT_SECURE_NO_WARNINGS SV_MAX_PAGES=49152 SR_TEST_MEMORY)
target_link_options(harness PRIVATE /LARGEADDRESSAWARE /MAP)
set_property(TARGET harness PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# The harness stays out of the way of the 024d test copy (0x20000000..0x45f3b000, tools/re/fixed_base.py).
target_link_options(harness PRIVATE /BASE:0x50000000 /DYNAMICBASE:NO /FIXED:NO)

# Phase 5.2: the CRT of the game (MSVCR100.dll) against the new CRT (ucrtbase.dll).
add_executable(crt_diff instrumentation/tests/crt_diff.c)
target_compile_options(crt_diff PRIVATE /W4 /utf-8)
target_compile_definitions(crt_diff PRIVATE _CRT_SECURE_NO_WARNINGS)

# Input checks of the proxy (instrumentation/hook/netguard.c): packets made by tools/net/srpacket.py.
add_executable(test_netguard instrumentation/tests/test_netguard.c instrumentation/hook/netguard.c)
target_include_directories(test_netguard PRIVATE instrumentation/hook)
target_compile_options(test_netguard PRIVATE /W4 /WX /utf-8)
endif()

# The normal executable has its own storage and no verification sources or include paths.
add_executable(subvulpes-server src/standalone/main.c src/server/storage.c src/platform/x87_math.c ${SV_REWRITTEN_SOURCES}
    src/server/accounts.c src/server/events.c src/server/players.c src/server/main_loop.c
    src/platform/plat_imports.c)
target_include_directories(subvulpes-server PRIVATE src/include src/standalone src/platform ${SV_REWRITTEN_INCLUDES})
target_compile_options(subvulpes-server PRIVATE /W4 /WX /utf-8 /arch:IA32)
target_compile_definitions(subvulpes-server PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
target_link_options(subvulpes-server PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_target_properties(subvulpes-server PROPERTIES OUTPUT_NAME opensubrosa24_server)
set_property(TARGET subvulpes-server PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The recording object library instruments game C without changing the normal target.
if(SV_BUILD_INSTRUMENTATION)
add_library(game_record OBJECT src/lifted/lifted_024d.c ${SV_REWRITTEN_SOURCES})
target_include_directories(game_record PRIVATE src/include src/lifted src/platform instrumentation/runtime ${SV_REWRITTEN_INCLUDES})
target_compile_options(game_record PRIVATE /Gh /W4 /WX /utf-8)
target_compile_definitions(game_record PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE SR_TEST_MEMORY)
set_property(TARGET game_record PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_executable(subvulpes-server-record src/standalone/main.c instrumentation/standalone/memory.c instrumentation/standalone/image_024d.c instrumentation/standalone/fn_table.c
    $<TARGET_OBJECTS:game_record> src/lifted/lifted_table.c src/platform/x87_math.c
    src/server/accounts.c src/server/events.c src/server/players.c src/server/main_loop.c
    src/platform/plat_imports.c instrumentation/standalone/record_shim.c instrumentation/hook/record.c instrumentation/hook/record_imports.c)
target_include_directories(subvulpes-server-record PRIVATE src/include src/lifted src/standalone src/platform instrumentation/standalone instrumentation/hook instrumentation/runtime ${SV_REWRITTEN_INCLUDES})
target_compile_options(subvulpes-server-record PRIVATE /W4 /WX /utf-8)
target_compile_definitions(subvulpes-server-record PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE SR_RECORD SR_TEST_MEMORY)
target_link_options(subvulpes-server-record PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG /INCREMENTAL:NO)
set_target_properties(subvulpes-server-record PROPERTIES OUTPUT_NAME opensubrosa24_server_record)
set_property(TARGET subvulpes-server-record PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Identical deterministic cases run with owned storage and the recording memory adapter.
foreach(mode native image)
    add_executable(test_storage_${mode} instrumentation/tests/test_storage.c
        instrumentation/standalone/memory.c instrumentation/standalone/image_024d.c
        src/platform/x87_math.c src/platform/plat_imports.c
        src/server/accounts.c src/server/events.c src/server/players.c ${SV_REWRITTEN_SOURCES})
    target_include_directories(test_storage_${mode} PRIVATE src/include src/standalone src/platform
        instrumentation/runtime instrumentation/standalone ${SV_REWRITTEN_INCLUDES})
    target_compile_definitions(test_storage_${mode} PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
    target_compile_options(test_storage_${mode} PRIVATE /W4 /WX /utf-8 /arch:IA32)
    target_link_options(test_storage_${mode} PRIVATE /LARGEADDRESSAWARE)
    set_property(TARGET test_storage_${mode} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endforeach()
target_sources(test_storage_native PRIVATE src/server/storage.c)
target_compile_definitions(test_storage_image PRIVATE SR_TEST_MEMORY)
endif()
