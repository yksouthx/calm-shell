cmake_minimum_required(VERSION 3.16)
project(calm-shell C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(CALM_WERROR "Treat warnings as errors" OFF)
option(CALM_BUILD_TESTS "Build the test suite" ON)

# -D_POSIX_C_SOURCE / _DEFAULT_SOURCE: the codebase uses POSIX.1-2008
# APIs (getcwd, realpath(path, NULL), strtok_r, ...) plus a couple of
# glibc-portable BSD-ish conveniences (dirent d_name traversal); this
# keeps glibc's feature-test macros from hiding them under -std=c11's
# strict mode while staying off nonstandard GNU-only extensions.
add_compile_definitions(_POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)

add_library(calm_core STATIC
    src/util/xalloc.c
    src/util/strbuf.c
    src/util/strutil.c
    src/util/strlist.c
    src/util/fsutil.c
    src/config/calmconf.c
    src/config/paths.c
    src/config/scaffold.c
    src/theme/theme.c
    src/theme/appearance.c
    src/theme/color_scheme.c
    src/theme/fastfetch_sync.c
    src/theme/sync.c
    src/term/emulator.c
    src/term/emulator_sync.c
    src/term/raw_terminal.c
    src/term/edit_buffer.c
    src/term/known_commands.c
    src/term/highlight.c
    src/term/completion.c
    src/term/history.c
    src/term/prompt.c
    src/term/line_editor.c
    src/exec/process.c
    src/exec/git_status.c
    src/shell/alias_table.c
    src/shell/dir_stack.c
    src/shell/builtins.c
    src/shell/execute.c
    src/shell/repl.c
    src/cli/cli.c
    src/cli/commands/shell_cmd.c
    src/cli/commands/theme_cmd.c
    src/cli/commands/sync_cmd.c
    src/cli/commands/plugin_cmd.c
    src/cli/commands/config_cmd.c
    src/cli/commands/doctor_cmd.c
)
target_include_directories(calm_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

set(CALM_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith
    -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes
)
if(CALM_WERROR)
    list(APPEND CALM_WARNING_FLAGS -Werror)
endif()
target_compile_options(calm_core PRIVATE ${CALM_WARNING_FLAGS})

add_executable(calm src/main.c)
target_link_libraries(calm PRIVATE calm_core)
target_compile_options(calm PRIVATE ${CALM_WARNING_FLAGS})

include(GNUInstallDirs)
install(TARGETS calm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(CALM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
