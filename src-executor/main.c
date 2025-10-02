#include <stdlib.h>

#include "../libs/logging/logging.h"
#include "executor/executor.h"
#include "../libs/io/io.h"

/*
TODO:
1) out not pops but tops
2) more universal commands execution
*/

const char* IN_FILE  = NULL;

void on_terminate();

int main(const int argc, char* const argv[])
{
    atexit(on_terminate);
    init_logging("log.log", DEBUG);
 
    size_t res = parse_arguments(argc, argv, &IN_FILE, NULL);
    if(!CHECK(ERROR, res == 1 && IN_FILE != NULL, "FILE NOT PROVIDED!"))
        { printf("FILE NOT PROVIDED!\n"); return 1; }
    
    operational_data_t op_data = { 0 };

    err_t rc = load_op_data(&op_data, IN_FILE);
    if (rc != OK) return 1;

    rc = exec_stream(&op_data);
    if (rc != OK) { printf("EXEC ERROR: %s\n", err_str(rc)); }
    
    free(op_data.buffer);
    fclose(op_data.in_file);

    return (rc == OK) ? 0 : 1;
}

void on_terminate()
{
    free((char*)IN_FILE);

    close_log_file();
}

