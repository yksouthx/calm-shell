#include "cli/commands/shell_cmd.h"

#include "shell/repl.h"

int shell_cmd_run(void) {
    return repl_run();
}
