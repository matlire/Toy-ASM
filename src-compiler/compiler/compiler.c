#include "compiler.h"

err_t load_op_data (operational_data_t * const op_data,
                     const char * const IN_FILE, const char * const OUT_FILE)
{
    FILE* in_file  = load_file(IN_FILE,  "r");
    if(!CHECK(ERROR, in_file != NULL, "CAN'T OPEN INPUT FILE!"))
        { printf("CAN'T OPEN INPUT FILE!\n"); return 1; }

    if(!CHECK(ERROR, clean_file(OUT_FILE) != 0, "FAILED TO PREPARE OUTPUT FILE"))
    {
        fclose(in_file);
        printf("CAN'T PREPARE OUTPUT FILE!\n");
        return ERR_BAD_ARG;
    }

    FILE* out_file = load_file(OUT_FILE, "a");
    if(!CHECK(ERROR, out_file != NULL, "CAN'T OPEN OUTPUT FILE!"))
    {
        fclose(in_file);
        printf("CAN'T OPEN OUTPUT FILE!\n");
        return ERR_BAD_ARG;
    }

    ssize_t file_size = get_file_size_stat(IN_FILE);
    if(!CHECK(ERROR, file_size > 0, "INPUT FILE IS EMPTY OR INACCESSIBLE"))
    {
        fclose(in_file);
        fclose(out_file);
        printf("INPUT FILE ERROR!\n");
        return ERR_BAD_ARG;
    }

    op_data->in_file     = in_file;
    op_data->out_file    = out_file;
    op_data->buffer_size = file_size;

    return OK;
}

static size_t parse_loop(const operational_data_t * const op_data, char** cursor)
{
    if(!CHECK(ERROR, cursor != NULL || *cursor != NULL || op_data != NULL, 
              "parse_loop: some data not provided")) return 0;

    char* p = *cursor;
    
    size_t total_written = 0;
    
    while (*p)
    {
        while (*p == '\n' || *p == '\r' || *p == ' ') p++;
        if (*p == '\0') break;

        char* line_start = p;
        while (*p != '\0' && *p != '\n' && *p != '\r') p++;
        char saved = *p;
        *p         = '\0';

        char* trimmed = line_start;
        while (*trimmed && isspace(*trimmed)) trimmed++;

        if (*trimmed == '\0' || *trimmed == ';')
            { *p = saved; if (*p != '\0') p++; continue; }

        char   encoded[MAX_LINE_LEN] = { 0 };
        size_t encoded_len = parse_line(trimmed, encoded);

        if (encoded_len == 0)
            { *p = saved; log_printf(ERROR, "parse_file: failed to parse line '%s'", trimmed); return 0; }

        size_t written = fwrite(encoded, sizeof(char), encoded_len, op_data->out_file);

        if (written != encoded_len)
            { *p = saved; log_printf(ERROR, "parse_file: failed to write encoded instruction"); return 0; }

        total_written += written;

        *p = saved;

        if (*p != '\0') p++;
    }

    *cursor = p;

    return total_written;
}

size_t parse_file(operational_data_t * const op_data)
{
    if (!CHECK(ERROR, op_data != NULL || op_data->in_file != NULL || 
               op_data->out_file != NULL || op_data->buffer_size != 0, "parse_file: some data is missing"))
            return 0;

    op_data->buffer = (char*)calloc(op_data->buffer_size + 1, sizeof(char));
    if (!op_data->buffer)
        { log_printf(ERROR, "parse_file: failed to alloc %zu bytes", op_data->buffer_size + 1); return 0; }

    size_t read_bytes = fread(op_data->buffer, sizeof(char), op_data->buffer_size, op_data->in_file);
    if (read_bytes == 0)
    {
        if (ferror(op_data->in_file)) log_printf(ERROR, "parse_file: fread failed");
        free(op_data->buffer);
        op_data->buffer = NULL;
        return 0;
    }

    op_data->buffer[read_bytes] = '\0';

    char*  cursor = op_data->buffer;

    return (parse_loop(op_data, &cursor));
}

size_t parse_line(const char* in, char * const out)
{
    if(!CHECK(ERROR, in != NULL || out != NULL, 
              "parse_line: some data not provided")) return 0;

    const char* cursor = in;

    while (*cursor && isspace(*cursor)) cursor++;
    if (*cursor == '\0' || *cursor == ';') return 0;

    char instruction[MAX_INSTRUCTION_LEN] = { 0 };
    size_t instruction_len = 0;

    while (*cursor && !isspace(*cursor) && instruction_len < MAX_INSTRUCTION_LEN - 1)
        instruction[instruction_len++] = *cursor++;

    instruction[instruction_len] = '\0';

    if (instruction_len == MAX_INSTRUCTION_LEN - 1 && *cursor && !isspace(*cursor))
        { log_printf(ERROR, "parse_line: instruction token too long"); return 0; }

    instruction_set ins = map_instruction(instruction);

    if (ins == UNDEF)
        { log_printf(ERROR, "parse_line: unknown instruction '%s'", instruction); return 0; }

    while (*cursor && isspace(*cursor)) cursor++;

    int written = snprintf(out, MAX_LINE_LEN, "%04d", (int)ins);
    if (written < 0 || written >= MAX_LINE_LEN) return 0;

    size_t total = (size_t)written;

    if (expect_arg(ins))
    {
        char* endptr = NULL;
        long  val    = strtol(cursor, &endptr, 10);

        if (cursor == endptr)
        {
            log_printf(ERROR, "parse_line: invalid argument for '%s'", instruction);
            return 0;
        }

        int arg_written = snprintf(out + total, 64 - total, "%ld", val);
        if (arg_written < 0 || arg_written >= (int)(64 - total)) return 0;

        total += (size_t)arg_written;
        cursor = endptr;

        while (*cursor && isspace(*cursor)) cursor++;
    }

    if (*cursor && *cursor != ';')
    {
        log_printf(ERROR, "parse_line: unexpected token '%s'", cursor);
        return 0;
    }

    out[total++] = '|';
    out[total]   = '\0';

    return total;
}
