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

# The clean code of the client and the files that only the client has.
# src/client holds the clean code that only the client has, in one folder for each system (image, render, input, menus).
file(GLOB_RECURSE CL_CLIENT_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/client/*.c)
set(CL_ADOPTED ${SV_REWRITTEN_SOURCES} ${CL_CLIENT_SOURCES})

# Import libraries of the DLL files that the game ships (SDL2, OpenAL, Speex). The names of the
# functions are in third_party/syslibs/*.def. The build makes each import library from its list,
# so no DLL file is needed to link.
set(CL_SYSLIB_DIR ${CMAKE_BINARY_DIR}/syslibs)
file(MAKE_DIRECTORY ${CL_SYSLIB_DIR})
set(CL_SYSLIBS)
foreach(syslib SDL2 OpenAL32 libspeex libspeexdsp)
    execute_process(COMMAND ${CMAKE_AR} /nologo /machine:x86 /def:${CMAKE_SOURCE_DIR}/third_party/syslibs/${syslib}.def
        /out:${CL_SYSLIB_DIR}/${syslib}.lib RESULT_VARIABLE syslib_result OUTPUT_QUIET ERROR_QUIET)
    if(NOT syslib_result EQUAL 0)
        message(FATAL_ERROR "Cannot make the import library ${syslib}.lib")
    endif()
    list(APPEND CL_SYSLIBS ${CL_SYSLIB_DIR}/${syslib}.lib)
endforeach()

# The native client: the same sources, one real C object for each datum (src/client/storage.c),
# no image of subrosa.exe and no file of instrumentation/.
add_library(client_native OBJECT EXCLUDE_FROM_ALL ${CL_ADOPTED})
target_include_directories(client_native PRIVATE src/include src/platform src/client src/client/native third_party/glee ${SV_REWRITTEN_INCLUDES})
target_compile_options(client_native PRIVATE /arch:IA32 /W3 /utf-8)
target_compile_definitions(client_native PRIVATE _CRT_SECURE_NO_WARNINGS SR_CLIENT_NATIVE)
set_property(SOURCE src/net/server.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_native>:addvoice=server_voice_slot>")
set_property(SOURCE src/game/game_setup.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_native>:setupgame=server_setupgame>")
set_property(SOURCE src/game/options.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_native>:loadsaveoptions=server_loadsaveoptions>")
set_property(TARGET client_native PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The native client executable: it holds no image of subrosa.exe and no file of instrumentation/.
add_executable(subvulpes-client-native EXCLUDE_FROM_ALL src/standalone/client_native_main.c
    src/platform/x87_math_client.c third_party/glee/GLee.c $<TARGET_OBJECTS:client_native>)
target_include_directories(subvulpes-client-native PRIVATE src/include src/platform src/client src/client/native third_party/glee)
target_compile_options(subvulpes-client-native PRIVATE /arch:IA32 /W3 /utf-8)
target_compile_definitions(subvulpes-client-native PRIVATE _CRT_SECURE_NO_WARNINGS SR_CLIENT_NATIVE)
set_source_files_properties(third_party/glee/GLee.c PROPERTIES COMPILE_OPTIONS "/W0")
target_link_libraries(subvulpes-client-native PRIVATE opengl32 user32 gdi32 ws2_32 shell32 ${CL_SYSLIBS})
target_link_options(subvulpes-client-native PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_target_properties(subvulpes-client-native PROPERTIES OUTPUT_NAME opensubrosa24_client_native)
set_property(TARGET subvulpes-client-native PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The same executable with the automatic input and frame checks of the smoke test.
add_executable(subvulpes-client-native-smoke EXCLUDE_FROM_ALL src/standalone/client_native_main.c
    src/platform/x87_math_client.c src/standalone/client_smoke.c third_party/glee/GLee.c $<TARGET_OBJECTS:client_native>)
target_include_directories(subvulpes-client-native-smoke PRIVATE src/include src/platform src/client src/client/native src/standalone third_party/glee)
target_compile_options(subvulpes-client-native-smoke PRIVATE /arch:IA32 /W3 /utf-8)
target_compile_definitions(subvulpes-client-native-smoke PRIVATE _CRT_SECURE_NO_WARNINGS SR_CLIENT_NATIVE SR_SMOKE)
target_link_libraries(subvulpes-client-native-smoke PRIVATE opengl32 user32 gdi32 ws2_32 shell32 ${CL_SYSLIBS})
target_link_options(subvulpes-client-native-smoke PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
set_target_properties(subvulpes-client-native-smoke PROPERTIES OUTPUT_NAME opensubrosa24_client_native_smoke)
set_property(TARGET subvulpes-client-native-smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The native client with a guard after each datum (SR_STORAGE_GUARDS). A write past the end of a datum
# prints its name and where the main thread is. The smoke variant drives the same scenarios.
add_library(client_native_guard OBJECT EXCLUDE_FROM_ALL ${CL_ADOPTED})
target_include_directories(client_native_guard PRIVATE src/include src/platform src/client src/client/native third_party/glee ${SV_REWRITTEN_INCLUDES})
target_compile_options(client_native_guard PRIVATE /arch:IA32 /W3 /utf-8)
target_compile_definitions(client_native_guard PRIVATE _CRT_SECURE_NO_WARNINGS SR_CLIENT_NATIVE SR_STORAGE_GUARDS)
set_property(SOURCE src/net/server.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_native_guard>:addvoice=server_voice_slot>")
set_property(SOURCE src/game/game_setup.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_native_guard>:setupgame=server_setupgame>")
set_property(SOURCE src/game/options.c APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,client_native_guard>:loadsaveoptions=server_loadsaveoptions>")
set_property(TARGET client_native_guard PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
foreach(variant "" "-smoke")
    set(sources src/standalone/client_native_main.c
        src/platform/x87_math_client.c third_party/glee/GLee.c $<TARGET_OBJECTS:client_native_guard>)
    set(definitions _CRT_SECURE_NO_WARNINGS SR_CLIENT_NATIVE SR_STORAGE_GUARDS)
    if(variant STREQUAL "-smoke")
        list(APPEND sources src/standalone/client_smoke.c)
        list(APPEND definitions SR_SMOKE)
    endif()
    set(target subvulpes-client-native-guard${variant})
    add_executable(${target} EXCLUDE_FROM_ALL ${sources})
    target_include_directories(${target} PRIVATE src/include src/platform src/client src/client/native src/standalone third_party/glee)
    target_compile_options(${target} PRIVATE /arch:IA32 /W3 /utf-8)
    target_compile_definitions(${target} PRIVATE ${definitions})
    target_link_libraries(${target} PRIVATE opengl32 user32 gdi32 ws2_32 shell32 ${CL_SYSLIBS})
    target_link_options(${target} PRIVATE /LARGEADDRESSAWARE /MAP /DEBUG)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME opensubrosa24_client_native_guard${variant})
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endforeach()
set_source_files_properties(third_party/glee/GLee.c PROPERTIES COMPILE_OPTIONS "/W0")

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

# The verification tools (image of the original, harness, recorder, hook libraries).
# They are outside the normal build: no target above depends on them.
if(SV_BUILD_INSTRUMENTATION)
    include(instrumentation/targets.cmake)
endif()
