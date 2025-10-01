#include <stdlib.h>

#include "../libs/logging/logging.h"
#include "../libs/io/io.h"

#include "compiler/compiler.h"

const char* IN_FILE  = NULL;
const char* OUT_FILE = NULL;

void on_terminate();

int main(const int argc, char* const argv[])
{
    atexit(on_terminate);
    init_logging("log.log", DEBUG);
 
    size_t res = parse_arguments(argc, argv, &IN_FILE, &OUT_FILE);
    if(!CHECK(ERROR, res == 2 && IN_FILE && OUT_FILE, "FILES NOT PROVIDED!"))
        { printf("FILES NOT PROVIDED!\n"); return 1; }

    FILE* in_file  = load_file(IN_FILE,  "r");
    if(!CHECK(ERROR, in_file != NULL, "CAN'T OPEN INPUT FILE!"))
        { printf("CAN'T OPEN INPUT FILE!\n"); return 1; }

    if(!CHECK(ERROR, clean_file(OUT_FILE) != 0, "FAILED TO PREPARE OUTPUT FILE"))
    {
        fclose(in_file);
        printf("CAN'T PREPARE OUTPUT FILE!\n");
        return 1;
    }

    FILE* out_file = load_file(OUT_FILE, "a");
    if(!CHECK(ERROR, out_file != NULL, "CAN'T OPEN OUTPUT FILE!"))
    {
        fclose(in_file);
        printf("CAN'T OPEN OUTPUT FILE!\n");
        return 1;
    }

    ssize_t file_size = get_file_size_stat(IN_FILE);
    if(!CHECK(ERROR, file_size > 0, "INPUT FILE IS EMPTY OR INACCESSIBLE"))
    {
        fclose(in_file);
        fclose(out_file);
        printf("INPUT FILE ERROR!\n");
        return 1;
    }

    operational_data_t op_data =
    {
        .in_file      = in_file,
        .out_file     = out_file,
        .buffer       = NULL,
        .buffer_size  = (size_t)file_size,
    };

    size_t parsed_bytes = parse_file(&op_data);

    free(op_data.buffer);

    fclose(in_file);
    fclose(out_file);

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
