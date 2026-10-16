cmake_minimum_required(VERSION 3.16)
project(light-git VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization and warnings
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
    # Release optimization for minimal footprint and maximum speed
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /GL /Gy /Gw")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -ffunction-sections -fdata-sections")
    if(APPLE)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto -Wl,-dead_strip")
    else()
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto -Wl,--gc-sections")
    endif()
endif()

include_directories(include)

# Core library sources
set(LIB_SOURCES
    src/core/types.cpp
    src/core/crypto.cpp
    src/core/repository.cpp
    src/storage/object_store.cpp
    src/storage/tree.cpp
    src/storage/commit.cpp
    src/storage/references.cpp
    src/storage/index.cpp
    src/filesystem/filesystem.cpp
    src/filesystem/ignore.cpp
    src/cli/terminal.cpp
    src/cli/config.cpp
    src/cli/diff.cpp
    src/cli/cli.cpp
    src/commands/cmd_init.cpp
    src/commands/cmd_status.cpp
    src/commands/cmd_add.cpp
    src/commands/cmd_commit.cpp
    src/commands/cmd_log.cpp
    src/commands/cmd_diff.cpp
    src/commands/cmd_branch.cpp
    src/commands/cmd_switch.cpp
    src/commands/cmd_restore.cpp
    src/commands/cmd_reset.cpp
    src/commands/cmd_show.cpp
    src/commands/cmd_rm.cpp
    src/commands/cmd_mv.cpp
    src/commands/cmd_check_ignore.cpp
    src/commands/cmd_config.cpp
    src/commands/cmd_gc.cpp
    src/commands/cmd_fsck.cpp
    src/commands/cmd_tag.cpp
    src/commands/cmd_stash.cpp
    src/commands/cmd_merge.cpp
    src/commands/cmd_cherry_pick.cpp
    src/commands/cmd_export_git.cpp
    src/commands/cmd_remote.cpp
    src/commands/cmd_fetch.cpp
    src/commands/cmd_push.cpp
    src/commands/cmd_pull.cpp
    src/commands/cmd_clone.cpp
    src/commands/cmd_bundle.cpp
    src/commands/cmd_worktree.cpp
    src/commands/cmd_registry.cpp
)

add_library(lgit_lib STATIC ${LIB_SOURCES})
target_include_directories(lgit_lib PUBLIC include src)

# Main executable: lgit
add_executable(lgit src/main.cpp)
target_link_libraries(lgit PRIVATE lgit_lib)

# Testing
enable_testing()

add_executable(unit_tests tests/unit/test_unit_all.cpp)
target_link_libraries(unit_tests PRIVATE lgit_lib)
target_include_directories(unit_tests PRIVATE include src tests)
add_test(NAME UnitTests COMMAND unit_tests)

add_executable(integration_tests tests/integration/test_integration_all.cpp)
target_link_libraries(integration_tests PRIVATE lgit_lib)
target_include_directories(integration_tests PRIVATE include src tests)
add_test(NAME IntegrationTests COMMAND integration_tests)

# Installation targets
install(TARGETS lgit RUNTIME DESTINATION bin)

# CPack configuration
set(CPACK_PACKAGE_NAME "light-git")
set(CPACK_PACKAGE_VENDOR "light-git contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ultra-lightweight, modern C++17 version control system")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
