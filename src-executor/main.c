#include <stdlib.h>

#include "../libs/logging/logging.h"
#include "executor/executor.h"
#include "../libs/io/io.h"

const char* IN_FILE  = NULL;

void on_terminate();

int main(const int argc, char* const argv[])
{
    atexit(on_terminate);
    init_logging("log.log", DEBUG);
 
    size_t res = parse_arguments(argc, argv, &IN_FILE, NULL);
    if(!CHECK(ERROR, res == 1 && IN_FILE != NULL, "FILE NOT PROVIDED!"))
        { printf("FILE NOT PROVIDED!\n"); return 1; }

    FILE* in_file  = load_file(IN_FILE,  "r");
    if(!CHECK(ERROR, in_file != NULL, "CAN'T OPEN FILE!"))
        { printf("CAN'T OPEN FILE!\n"); return 1; }

    ssize_t file_size = get_file_size_stat(IN_FILE);
    if(!CHECK(ERROR, file_size > 0, "INPUT FILE IS EMPTY OR INACCESSIBLE"))
    {
        fclose(in_file);
        printf("INPUT FILE ERROR!\n");
        return 1;
    }

    operational_data_t op_data = {
        .in_file     = in_file,
        .out_file    = NULL,
        .buffer      = NULL,
        .buffer_size = (size_t)file_size,
    };
    err_t rc = exec_stream(&op_data);
    if (rc != OK) { printf("EXEC ERROR: %s\n", err_str(rc)); }
    
    free(op_data.buffer);
    fclose(in_file);

    return (rc == OK) ? 0 : 1;
}

void on_terminate()
{
    free((char*)IN_FILE);

    close_log_file();
}

