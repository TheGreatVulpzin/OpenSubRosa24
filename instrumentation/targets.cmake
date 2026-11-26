# Verification targets. CMakeLists.txt includes this file when SV_BUILD_INSTRUMENTATION is ON.
# Paths are relative to the root folder of the repository.

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

# Proxy wsock32.dll for subrosa.exe (client). Its own output folder: the file name is the same.
add_library(cl_wsock32 SHARED instrumentation/client/cl_proxy.c instrumentation/client/wsock32.def)
set_target_properties(cl_wsock32 PROPERTIES OUTPUT_NAME wsock32
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client
    PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client)
target_include_directories(cl_wsock32 PRIVATE instrumentation/hook)
target_compile_options(cl_wsock32 PRIVATE /W4 /WX /utf-8)
target_compile_definitions(cl_wsock32 PRIVATE _CRT_SECURE_NO_WARNINGS)
# The call counter needs the table of instrumentation/client/gen_count_table.py.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/lift/client_win/cl_count_table.c)
    target_sources(cl_wsock32 PRIVATE instrumentation/client/cl_count.c build/lift/client_win/cl_count_table.c
        instrumentation/hook/hook.c)
    target_include_directories(cl_wsock32 PRIVATE src/include instrumentation/runtime)
    target_compile_definitions(cl_wsock32 PRIVATE CL_COUNT SR_TEST_MEMORY)
    set_source_files_properties(build/lift/client_win/cl_count_table.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")
endif()
# Clean files of the server that the client uses in place of lifted C (analysis/client_win/adopted.txt).
# The storage adapter of the client puts each sr_data_<name> at its address in the client image.
set(CL_ADOPTED_OBJECTS)
add_library(client_adopted OBJECT ${CL_ADOPTED} instrumentation/client/runtime/storage_unplaced.c instrumentation/client/runtime/server_stubs.c instrumentation/client/runtime/cl_fn.c)
target_include_directories(client_adopted PRIVATE instrumentation/client/runtime src/include src/lifted src/platform src/client ${SV_REWRITTEN_INCLUDES})
target_compile_options(client_adopted PRIVATE /arch:IA32 /W3 /utf-8)
target_compile_definitions(client_adopted PRIVATE _CRT_SECURE_NO_WARNINGS SR_TEST_MEMORY SR_CLIENT_IMAGE)
set_property(SOURCE src/net/server.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_adopted>:addvoice=server_voice_slot>")
set_property(SOURCE src/game/game_setup.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_adopted>:setupgame=server_setupgame>")
set_property(SOURCE src/game/options.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_adopted>:loadsaveoptions=server_loadsaveoptions>")
set_property(TARGET client_adopted PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CL_ADOPTED_OBJECTS $<TARGET_OBJECTS:client_adopted>)
# Lifted client code with the hook tables of the lifter, the verifier and the recorder. The hook
# tables are generated under build/ by instrumentation/client/pipeline.sh and are not in the
# repository, so a fresh checkout skips the client targets.
if(EXISTS ${CMAKE_SOURCE_DIR}/src/lifted/client/lifted_client.c
   AND EXISTS ${CMAKE_SOURCE_DIR}/build/lift/client_win/hooks_lifted.c
   AND EXISTS ${CMAKE_SOURCE_DIR}/build/lift/client_win/hooks_record.c)
    target_sources(cl_wsock32 PRIVATE instrumentation/client/cl_hooks.c
        src/lifted/client/lifted_client.c build/lift/client_win/hooks_lifted.c build/lift/client_win/hooks_record.c
        instrumentation/hook/verify.c instrumentation/hook/record.c instrumentation/hook/record_imports.c
        instrumentation/hook/imports.c instrumentation/runtime/profile.c src/platform/x87_math_client.c ${CL_ADOPTED_OBJECTS})
    target_include_directories(cl_wsock32 PRIVATE src/lifted src/platform)
    target_compile_definitions(cl_wsock32 PRIVATE CL_HOOKS)
    # The clean code calls the system libraries by name: the thunks send each call to the import of the
    # target (build/lift/client_win/system_thunks.c, made by instrumentation/client/gen_system_thunks.py).
    target_sources(cl_wsock32 PRIVATE build/lift/client_win/system_thunks.c)
    target_link_libraries(cl_wsock32 PRIVATE ws2_32)
    set_source_files_properties(build/lift/client_win/system_thunks.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")
    set_source_files_properties(src/lifted/client/lifted_client.c PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013")
    set_source_files_properties(build/lift/client_win/hooks_lifted.c build/lift/client_win/hooks_record.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")

    # Offline harness for the client: the same program as the server harness, with the client sources.
    add_executable(harness_client instrumentation/tests/harness.c instrumentation/client/check_image_io.c instrumentation/hook/verify.c instrumentation/hook/hook.c
        instrumentation/hook/record.c instrumentation/hook/imports.c instrumentation/hook/console.c instrumentation/tests/harness_ui.c
        instrumentation/platform/plat_test.c src/lifted/client/lifted_client.c src/lifted/client/lifted_table.c
        src/platform/x87_math_client.c ${CL_ADOPTED_OBJECTS})
    target_include_directories(harness_client PRIVATE src/include src/lifted src/platform src/client instrumentation/platform
        instrumentation/hook instrumentation/runtime build/lift/client_win)
    target_compile_options(harness_client PRIVATE /W4 /WX /utf-8)
    target_compile_definitions(harness_client PRIVATE _CRT_SECURE_NO_WARNINGS SV_MAX_PAGES=114688 SR_TEST_MEMORY SR_CLIENT_IMAGE CL_HARNESS SV_FULL_EVERY_DEFAULT=1)
    target_link_options(harness_client PRIVATE /LARGEADDRESSAWARE /MAP /BASE:0x50000000 /DYNAMICBASE:NO /FIXED:NO)
    target_sources(harness_client PRIVATE build/lift/client_win/system_thunks.c)
    target_link_libraries(harness_client PRIVATE ws2_32)
    set_property(TARGET harness_client PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_source_files_properties(src/lifted/client/lifted_client.c PROPERTIES COMPILE_OPTIONS "/arch:IA32;/W0;/WX-;/we4013")
    set_source_files_properties(src/lifted/client/lifted_table.c PROPERTIES COMPILE_OPTIONS "/W0;/WX-")

    # Standalone client, level 1: lifted C in its own image region (Phase 7.5). The image tables come
    # from instrumentation/lift/gen_image.py --client.
    if(EXISTS ${CMAKE_SOURCE_DIR}/build/client_image/image_client.c)
        add_library(client_lifted OBJECT EXCLUDE_FROM_ALL src/lifted/client/lifted_client.c)
        target_include_directories(client_lifted PRIVATE src/include src/lifted src/platform build/lift/client_win)
        target_compile_options(client_lifted PRIVATE /arch:IA32 /W0 /utf-8)
        target_compile_definitions(client_lifted PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
        set_property(TARGET client_lifted PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_executable(subvulpes-client EXCLUDE_FROM_ALL src/standalone/client_main.c src/platform/client_crt.c
            src/standalone/client_smoke.c instrumentation/client/gl_trace.c
            build/client_image/image_client.c build/client_image/client_gl.c
            build/client_image/client_gl_glee.c third_party/glee/GLee.c
            $<TARGET_OBJECTS:client_lifted> src/lifted/client/lifted_table.c src/platform/x87_math_client.c ${CL_ADOPTED_OBJECTS})
        target_include_directories(subvulpes-client PRIVATE src/include src/lifted src/standalone src/platform src/client third_party/glee)
        set_source_files_properties(third_party/glee/GLee.c PROPERTIES COMPILE_OPTIONS "/W0")
        target_link_libraries(subvulpes-client PRIVATE opengl32)
        target_compile_options(subvulpes-client PRIVATE /W4 /utf-8)
        target_compile_definitions(subvulpes-client PRIVATE _CRT_SECURE_NO_WARNINGS SR_STANDALONE)
        target_link_options(subvulpes-client PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
        set_target_properties(subvulpes-client PROPERTIES OUTPUT_NAME opensubrosa24_client)
        set_property(TARGET subvulpes-client PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_source_files_properties(src/lifted/client/lifted_table.c PROPERTIES COMPILE_OPTIONS "/W0")
        set_source_files_properties(build/client_image/image_client.c build/client_image/client_gl.c PROPERTIES COMPILE_OPTIONS "/W0")
    endif()
endif()
set_property(TARGET cl_wsock32 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_options(cl_wsock32 PRIVATE /MAP)

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

# The recording object library instruments game C without changing the normal target.
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
target_sources(test_storage_native PRIVATE ${SV_STORAGE_SOURCES})
target_compile_definitions(test_storage_image PRIVATE SR_TEST_MEMORY)
