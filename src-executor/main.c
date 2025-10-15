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
    
    /*
        Load operational data
    */
    operational_data_t op_data = { 0 };
    err_t rc = load_op_data(&op_data, IN_FILE);

    if (rc != OK) return 1;

    /*
        Init CPU
    */
    cpu_t cpu = { 0 };
    rc        = cpu_init(&cpu);

    if (!CHECK(ERROR, rc == OK, "main: cpu init failed"))
    {
        printf("CPU INIT FAILED\n");
        return 1;
    }

    /*
        Load programm from bytecode, execute if
    */

    rc = load_program(&op_data, &cpu);

    if (!CHECK(ERROR, rc == OK, "main: failed to load program"))
    {
        printf("LOAD PROGRAM FAILED\n");
        return 1;
    }

    /*
        Exec programm
    */
    rc = exec_stream(&cpu);
    
    if (!CHECK(ERROR, rc == OK, "main: execute program stream failed"))
    {
        printf("EXEC STREAM FAILED\n");
        return 1;
    }

    cpu_destroy(&cpu);
    
    free(op_data.buffer);
    fclose(op_data.in_file);

    return (rc == OK) ? 0 : 1;
}

void on_terminate()
{
    free((char*)IN_FILE);

    close_log_file();
}
