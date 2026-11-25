#include "app.h"
#include "cli.h"

#include <stdio.h>

int main(int argc, char **argv) {
    CLI_ARGS args;
    int status;
    if (!cli_parse(argc, argv, &args)) {
        cli_usage(argv[0]);
        cli_destroy(&args);
        return 2;
    }
    status = app_run(&args);
    cli_destroy(&args);
    return status;
}
