#include <stdlib.h>

#include "../libs/logging/logging.h"
#include "../libs/io/io.h"

#include "compiler/compiler.h"

const char* IN_FILE  = NULL;
const char* OUT_FILE = NULL;

void on_terminate();
// 5-3 division
// Signature - store in bin version
int main(const int argc, char* const argv[])
{
    atexit(on_terminate);
    init_logging("log.log", DEBUG);
 
    size_t res = parse_arguments(argc, argv, &IN_FILE, &OUT_FILE);
    if(!CHECK(ERROR, res == 2 && IN_FILE && OUT_FILE, "FILES NOT PROVIDED!"))
        { printf("FILES NOT PROVIDED!\n"); return 1; }

    
    operational_data_t op_data = { 0 };

    err_t rc = load_op_data(&op_data, IN_FILE, OUT_FILE);
    if (rc != OK) return 1;

    size_t parsed_bytes = parse_file(&op_data);

    free(op_data.buffer);

    fclose(op_data.in_file);
    fclose(op_data.out_file);

    if(!CHECK(ERROR, parsed_bytes > 0, "FILE PARSING FAILED"))
    {
        printf("FILE PARSING FAILED!\n");
        return 1;
    }

    return 0;
}

void on_terminate()
{
    free((char*)IN_FILE);
    free((char*)OUT_FILE);

    close_log_file();
}
