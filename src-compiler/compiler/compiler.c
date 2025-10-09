#include "compiler.h"

err_t load_op_data (operational_data_t * const op_data,
                     const char * const IN_FILE, const char * const OUT_FILE)
{
    FILE* in_file  = load_file(IN_FILE,  "r");
    if (!CHECK(ERROR, in_file != NULL, "CAN'T OPEN INPUT FILE!"))
        { printf("CAN'T OPEN INPUT FILE!\n"); return ERR_BAD_ARG; }

    if (!CHECK(ERROR, clean_file(OUT_FILE) != 0, "FAILED TO PREPARE OUTPUT FILE"))
    {
        fclose(in_file);
        printf("CAN'T PREPARE OUTPUT FILE!\n");
        return ERR_BAD_ARG;
    }

    FILE* out_file = load_file(OUT_FILE, "w+b");
    if (!CHECK(ERROR, out_file != NULL, "CAN'T OPEN OUTPUT FILE!"))
    {
        fclose(in_file);
        printf("CAN'T OPEN OUTPUT FILE!\n");
        return ERR_BAD_ARG;
    }

    ssize_t file_size = get_file_size_stat(IN_FILE);
    if (!CHECK(ERROR, file_size > 0, "INPUT FILE IS EMPTY OR INACCESSIBLE"))
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
    if (!CHECK(ERROR, cursor != NULL && *cursor != NULL && op_data != NULL, 
              "parse_loop: some data not provided")) return 0;

    char* p = *cursor;
    
    size_t total_written = 0;
    
    while (*p)
    {
        while (isspace(*p)) p++;
        if (*p == '\0') break;

        char* line_start = p;
        while (*p != '\0' && *p != '\n' && *p != '\r') p++;
        char saved = *p;
        *p         = '\0';

        char* trimmed = line_start;
        while (*trimmed && isspace(*trimmed)) trimmed++;

        if (*trimmed == '\0' || *trimmed == ';')
            { *p = saved; if (*p != '\0') p++; continue; }

        unsigned char encoded[MAX_LINE_LEN] = { 0 };
        size_t  encoded_len           = parse_line(trimmed, encoded);

        if (encoded_len == 0)
            { *p = saved; log_printf(ERROR, "parse_file: failed to parse line '%s'", trimmed); return 0; }

        size_t written = fwrite(encoded, 1, encoded_len, op_data->out_file);

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
    if (!CHECK(ERROR, op_data != NULL && op_data->in_file != NULL && 
               op_data->out_file != NULL && op_data->buffer_size != 0, "parse_file: some data is missing"))
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

    char* cursor = op_data->buffer;

    instruction_set_version_t   version = instruction_set_version();
    instruction_binary_header_t header  = { 0 };

    memcpy(header.magic, INSTRUCTION_BINARY_MAGIC, INSTRUCTION_BINARY_MAGIC_LEN);
    header.version_major = (unsigned char)version.major;
    header.version_minor = (unsigned char)version.minor;

    size_t header_written = fwrite(&header, 1, sizeof(header), op_data->out_file);
    if (header_written != sizeof(header))
    {
        log_printf(ERROR, "parse_file: failed to write binary header");
        free(op_data->buffer);
        op_data->buffer = NULL;
        return 0;
    }

    size_t body_written  = parse_loop(op_data, &cursor);

    if (body_written == 0) return 0;

    if (body_written > UINT32_MAX)
    {
        log_printf(ERROR, "parse_file: body exceeds maximum encodable size (%zu bytes)", body_written);
        return 0;
    }

    header.code_size = (uint32_t)body_written;

    if (fseek(op_data->out_file, 0L, SEEK_SET) != 0)
    {
        log_printf(ERROR, "parse_file: failed to seek for header rewrite");
        return 0;
    }

    size_t rewrite = fwrite(&header, 1, sizeof(header), op_data->out_file);
    if (rewrite != sizeof(header))
    {
        log_printf(ERROR, "parse_file: failed to update binary header");
        return 0;
    }

    if (fflush(op_data->out_file) != 0)
    {
        log_printf(ERROR, "parse_file: failed to flush output stream");
        return 0;
    }

    return header_written + body_written;
}

size_t parse_line(const char* in, unsigned char * const out)
{
    if(!CHECK(ERROR, in != NULL && out != NULL, 
              "parse_line: some data not provided")) return 0;

    const char* cursor = in;

    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if    (*cursor == '\0' || *cursor == ';') return 0;

    char   instruction[MAX_INSTRUCTION_LEN] = { 0 };
    size_t instruction_len                  = 0;

    while (*cursor && !isspace((unsigned char)*cursor) && instruction_len < MAX_INSTRUCTION_LEN - 1)
        instruction[instruction_len++] = *cursor++;

    instruction[instruction_len] = '\0';

    if (instruction_len == MAX_INSTRUCTION_LEN - 1 && *cursor && !isspace((unsigned char)*cursor))
        { log_printf(ERROR, "parse_line: instruction token too long"); return 0; }

    instruction_set ins = map_instruction(instruction);

    if (ins == UNDEF)
        { log_printf(ERROR, "parse_line: unknown instruction '%s'", instruction); return 0; }

    const instruction_t* meta = instruction_get(ins);
    if (!meta)
        { log_printf(ERROR, "parse_line: instruction metadata missing for '%s'", instruction); return 0; }

    while (*cursor && isspace((unsigned char)*cursor)) cursor++;

    size_t total = 0;
    out[total++] = (unsigned char)meta->id;
    out[total++] = (unsigned char)meta->expected_args;

    for (size_t arg_idx = 0; arg_idx < meta->expected_args; ++arg_idx)
    {
        char* endptr = NULL;
        long  val    = 0;

        if ((*cursor == 'x' || *cursor == 'X') && isdigit((unsigned char)cursor[1]))
        {
            const char* digits = cursor + 1;
            val = strtol(digits, &endptr, 10);
            if ((const char*)endptr == digits)
                { log_printf(ERROR, "parse_line: invalid register for '%s'", instruction); return 0; }
        } else
        {
            val = strtol(cursor, &endptr, 10);
            if (cursor == endptr)
                { log_printf(ERROR, "parse_line: invalid argument for '%s'", instruction); return 0; }
        }

        int32_t stored = (int32_t)val;
        memcpy(out + total, &stored, sizeof(stored));
        total += sizeof(stored);

        cursor = endptr;

        while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    }

    if (*cursor && *cursor != ';')
    {
        log_printf(ERROR, "parse_line: unexpected token '%s'", cursor);
        return 0;
    }

    return total;
}
