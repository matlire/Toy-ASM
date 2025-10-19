#include <stdlib.h>

#include "../libs/logging/logging.h"
#include "../libs/io/io.h"

#include "compiler/compiler.h"

const char* IN_FILE  = NULL;
const char* OUT_FILE = NULL;

logging_level level = INFO;

void on_terminate();

int main(const int argc, char* const argv[])
{
    atexit(on_terminate);
    init_logging("log.log", level);

    int exit_code = 0;
 
    size_t res = parse_arguments(argc, argv, &IN_FILE, &OUT_FILE);
    if (!CHECK(ERROR, res == 2 && IN_FILE && OUT_FILE, "main: files not provided"))
        { printf("FILES NOT PROVIDED!\n"); return 1; }
    
    operational_data_t op_data = { 0 };

    /*
        Setup operational data
    */
    err_t rc = load_op_data(&op_data, IN_FILE, OUT_FILE);

    if (rc != OK) return 1;

    /*
       Load asm source file into buffer
    */
    size_t parsed_bytes = parse_file(&op_data);    

    if (!CHECK(ERROR, parsed_bytes > 0, "main: file parsing failed"))
    {
        printf("FILE PARSING FAILED!\n");
        return 1;
    }

    /*
        Init asm
    */
    asm_t assembler   = { 0 };
    err_t asm_init_rc = asm_init(&assembler, op_data.buffer, parsed_bytes);

    if (!CHECK(ERROR, asm_init_rc == OK,
               "main: asm_init failed"))
    {
        printf("ASM INIT FAILED!\n");
        return 1;
    }

    /*
        Generate header
    */
    
    instruction_binary_header_t header = { 0 };
    size_t header_written = gen_write_header(&op_data, &header);

    if (!CHECK(ERROR, header_written != 0,
                   "main: failed to generate & write binary header"))
    {
        printf("HEADER GENERATE & WRITE FAILED!\n");
        exit_code = 1;
        goto cleanup;
    }

    /*
        First asm pass (generate labels)
    */
    size_t first_pass_bytes = asm_first_pass(&assembler, level);

    if (!CHECK(ERROR, first_pass_bytes != SIZE_MAX,
            "main: first pass failed"))
    {
        printf("FIRST PASS FAILED!\n");
        exit_code = 1;
        goto cleanup;
    }

    /*
        Second asm pass (byte code generation)
    */
    size_t body_written = asm_second_pass(&assembler, op_data.out_file, level);
    
    if (!CHECK(ERROR, body_written != SIZE_MAX,
                "main: second pass failed"))
    {
        printf("SECOND PASS FAILED!\n");
        exit_code = 1;
        goto cleanup;
    }

    if (!CHECK(ERROR, body_written <= UINT32_MAX,
               "main: body exceeds maximum encodable size %zu bytes",
               body_written))
    {
        printf("BODY TOO LARGE!\n");
        exit_code = 1;
        goto cleanup;
    }

    /*
        Update header with code size
    */
    
    header_written = update_header(&op_data, &header, body_written);

    if (!CHECK(ERROR, header_written != 0,
               "parse_file: body exceeds maximum encodable size %zu bytes",
               body_written))
    {
        printf("PARSE_FILE: BODY TOO LARGE!\n");
        exit_code = 1;
        goto cleanup;
    }

cleanup:
    asm_destroy(&assembler);
    free(op_data.buffer);
    fclose(op_data.in_file);
    fclose(op_data.out_file);
    close_log_file();
    return exit_code;
}

void on_terminate()
{
    close_log_file();
}
