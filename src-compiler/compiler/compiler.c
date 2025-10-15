#include "compiler.h"

err_t load_op_data(operational_data_t * const op_data,
                   const char * const IN_FILE,
                   const char * const OUT_FILE)
{
    if (!op_data || !IN_FILE || !OUT_FILE) return ERR_BAD_ARG;

    err_t rc = ERR_BAD_ARG;

    begin
        FILE* in_file = load_file(IN_FILE, "r");
        if (!CHECK(ERROR, in_file != NULL, "load_op_data: cannot open input file"))
        {
            printf("CAN'T OPEN INPUT FILE!\n");
            break;
        }
        op_data->in_file = in_file;

        if (!CHECK(ERROR, clean_file(OUT_FILE) != 0, "load_op_data: failed to prepare output file"))
        {
            printf("CAN'T PREPARE OUTPUT FILE!\n");
            break;
        }

        FILE* out_file = load_file(OUT_FILE, "w+b");
        if (!CHECK(ERROR, out_file != NULL, "load_op_data: cannot open output file"))
        {
            printf("CAN'T OPEN OUTPUT FILE!\n");
            break;
        }
        op_data->out_file = out_file;

        ssize_t file_size = get_file_size_stat(IN_FILE);
        if (!CHECK(ERROR, file_size > 0, "load_op_data: input file is empty or inaccessible"))
        {
            printf("INPUT FILE ERROR!\n");
            break;
        }

        op_data->buffer_size = (size_t)file_size;
        rc = OK;
    end;

    if (rc != OK)
    {
        if (op_data->in_file)  { fclose(op_data->in_file);  op_data->in_file  = NULL; }
        if (op_data->out_file) { fclose(op_data->out_file); op_data->out_file = NULL; }
    }

    return rc;
}

size_t parse_file(operational_data_t * const op_data)
{
    if (!CHECK(ERROR, op_data != NULL && op_data->in_file != NULL &&
               op_data->out_file != NULL && op_data->buffer_size != 0,
               "parse_file: some data is missing"))
    {
        printf("PARSE_FILE: DATA IS MISSING!\n");
        return 0;
    }

    err_t  rc         = ERR_BAD_ARG;
    size_t read_bytes = 0;

    begin
        op_data->buffer = (char*)calloc(op_data->buffer_size + 1, sizeof(char));
        if (!CHECK(ERROR, op_data->buffer != NULL,
                   "parse_file: failed to alloc %zu bytes",
                   op_data->buffer_size + 1))
        {
            printf("PARSE_FILE: BUFFER ALLOCATION FAILED!\n");
            break;
        }

        read_bytes = fread(op_data->buffer, sizeof(char), op_data->buffer_size, op_data->in_file);
        if (!CHECK(ERROR, read_bytes != 0,
                   "parse_file: failed to read from input stream ferror=%d",
                   ferror(op_data->in_file)))
        {
            printf("PARSE_FILE: INPUT READ FAILED!\n");
            break;
        }

        op_data->buffer[read_bytes] = '\0';
        rc = OK;
    end;

    if (rc != OK) return 0;

    return read_bytes;
}

size_t gen_write_header(operational_data_t * const op_data, instruction_binary_header_t* header)
{
    if (!CHECK(ERROR, op_data != NULL && op_data->in_file != NULL &&
               op_data->out_file != NULL && op_data->buffer_size != 0 &&
               header != NULL,
               "write_header: some data is missing"))
    {
        printf("WRITE_HEADER: DATA IS MISSING!\n");
        return 0;
    }


    instruction_set_version_t   version = instruction_set_version();
    memcpy(header->magic, INSTRUCTION_BINARY_MAGIC, INSTRUCTION_BINARY_MAGIC_LEN);
    header->version_major = (unsigned char)version.major;
    header->version_minor = (unsigned char)version.minor;

    size_t header_written = fwrite(header, 1, sizeof(*header), op_data->out_file);

    return header_written;
}

size_t update_header(operational_data_t * const op_data, 
                     instruction_binary_header_t* header, const size_t body_written)
{
    header->code_size = (uint32_t)body_written;
    size_t rewrite    = 0;
    err_t rc          = ERR_CORRUPT;

    begin
        if (!CHECK(ERROR, fseek(op_data->out_file, 0L, SEEK_SET) == 0,
                    "update_header: failed to seek for header rewrite"))
        {
            printf("UPDATE_HEADER: HEADER SEEK FAILED!\n");
            break;
        }

        rewrite = fwrite(header, 1, sizeof(*header), op_data->out_file);
        if (!CHECK(ERROR, rewrite != 0,
                    "update_header: failed to update binary header"))
        {
            printf("UPDATE_HEADER: HEADER UPDATE FAILED!\n");
            break;
        }

        if (!CHECK(ERROR, fflush(op_data->out_file) == 0,
                    "update_header: failed to flush output stream"))
        {
            printf("UPDATE_HEADER: FLUSH FAILED!\n");
            break;
        }

        rc = OK;
    end;

    if (rc != OK) return 0;

    return rewrite;
}
