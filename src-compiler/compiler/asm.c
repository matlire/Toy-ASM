#include "compiler.h"

#include "../dumper/dump.h"

#include <stdlib.h>

static label_parse_status_t label_parse_token(const char*    token,
                                              label_token_t* out)
{
    if (!CHECK(ERROR, out != NULL, 
               "label_parse_token: out is NULL"))
        return LABEL_PARSE_ERR_INVALID;
    
    if (!token || token[0] != ':') return LABEL_PARSE_ERR_BAD_PREFIX;

    const char* name_start = token + 1;
    const char* cursor     = name_start;

    if (*cursor == '\0') return LABEL_PARSE_ERR_EMPTY;

    while (*cursor && (isalnum((unsigned char)*cursor) || *cursor == '_')) cursor++;

    if (cursor == name_start) return LABEL_PARSE_ERR_INVALID;

    out->name   = name_start;
    out->length = (size_t)(cursor - name_start);

    return LABEL_PARSE_OK;
}

static err_t asm_ensure_capacity(asm_t* as)
{
    if (as->label_count < as->label_capacity) return OK;

    size_t old_capacity = as->label_capacity;
    size_t new_capacity = (old_capacity == 0) ? ASM_INITIAL_LABEL_CAPACITY
                                              : old_capacity * 2;

    asm_label_t* resized = (asm_label_t*)realloc(as->labels, new_capacity * sizeof(*resized));
    if (!CHECK(ERROR, resized != NULL,
               "asm_ensure_capacity: realloc failed for %zu labels", new_capacity))
        return ERR_ALLOC;

    as->labels         = resized;
    as->label_capacity = new_capacity;

    size_t newly_added = new_capacity - old_capacity;
    if (newly_added > 0)
        memset(as->labels + old_capacity, 0, newly_added * sizeof(*as->labels));
    return OK;
}

static asm_label_t* asm_find_label(asm_t* as, const char* name, size_t name_len)
{
    if (!CHECK(ERROR, as != NULL && name != NULL,
               "asm_find_label: invalid arguments"))
        return NULL;

    for (size_t i = 0; i < as->label_count; ++i)
    {
        if (as->labels[i].name_len == name_len &&
            strncmp(as->labels[i].name, name, name_len) == 0)
            return &as->labels[i];
    }
    return NULL;
}

static err_t asm_add_label(asm_t* as, const char* name, size_t name_len, size_t offset)
{
    if (!CHECK(ERROR, as != NULL && name != NULL && name_len != 0,
               "asm_add_label: invalid arguments"))
        return ERR_BAD_ARG;

    asm_label_t* existing = asm_find_label(as, name, name_len);
    if (existing)
    {
        if (!CHECK(ERROR, existing->offset == offset,
                   "asm_add_label: label '%.*s' redefined",
                   (int)existing->name_len, existing->name))
        {
            printf("ASM_ADD_LABEL: LABEL REDEFINED!\n");
            return ERR_BAD_ARG;
        }
        return OK;
    }

    err_t rc = asm_ensure_capacity(as);
    if (rc != OK) return rc;

    as->labels[as->label_count].name     = name;
    as->labels[as->label_count].name_len = name_len;
    as->labels[as->label_count].offset   = offset;
    as->label_count++;

    return OK;
}

static err_t process_label_definition(asm_t* as,
                                      const char* trimmed,
                                      size_t offset,
                                      int pass)
{
    if (!CHECK(ERROR, trimmed != NULL && trimmed[0] == ':',
               "process_label_definition: missing ':' label prefix"))
        return ERR_BAD_ARG;

    label_token_t        label    = { 0 };
    label_parse_status_t parse_rc = label_parse_token(trimmed, &label);

    if (!CHECK(ERROR, parse_rc == LABEL_PARSE_OK,
               "process_label_definition: label parse failed"))
    {
        if (parse_rc == LABEL_PARSE_ERR_EMPTY)
            printf("PROCESS_LABEL_DEFINITION: EMPTY LABEL!\n");
        else if (parse_rc == LABEL_PARSE_ERR_INVALID)
            printf("PROCESS_LABEL_DEFINITION: INVALID LABEL!\n");
        else
            printf("PROCESS_LABEL_DEFINITION: INVALID LABEL!\n");
        return ERR_BAD_ARG;
    }

    const char* tail = label.name + label.length;

    while (*tail && isspace((unsigned char)*tail)) tail++;
    if (!CHECK(ERROR, !*tail || *tail == ';',
               "process_label_definition: unexpected token '%s'", tail))
    {
        printf("PROCESS_LABEL_DEFINITION: UNEXPECTED TOKEN!\n");
        return ERR_BAD_ARG;
    }

    size_t name_len        = label.length;
    const char* name_start = label.name;

    if (pass == 0)
    {
        return asm_add_label(as, name_start, name_len, offset);
    }
    else
    {
        asm_label_t* found = asm_find_label(as, name_start, name_len);

        if (!CHECK(ERROR, found != NULL,
                   "process_label_definition: label '%.*s' missing from pass one",
                   (int)name_len, name_start))
        {
            printf("PROCESS_LABEL_DEFINITION: LABEL MISSING FROM PASS ONE!\n");
            return ERR_BAD_ARG;
        }
        return OK;
    }
}

static err_t parse_register_arg(const char* token,
                                long*       out_value,
                                const char** out_end)
{
    if (!token) return ERR_BAD_ARG;

    char* endptr = NULL;
    long  value  = strtol(token + 1, &endptr, 10);

    if (!CHECK(ERROR, token + 1 != endptr,
               "parse_argument: invalid register '%s'", token))
    {
        printf("PARSE_ARGUMENT: INVALID REGISTER!\n");
        return ERR_BAD_ARG;
    }

    if (out_end)  *out_end  = endptr;
    if (out_value) *out_value = value;

    return OK;
}

static err_t parse_label_arg(asm_t* as,
                             const char* token,
                             size_t      iter,
                             long*       out_value,
                             const char** out_end)
{
    label_token_t        label    = { 0 };
    label_parse_status_t parse_rc = label_parse_token(token, &label);

    if (!CHECK(ERROR, parse_rc == LABEL_PARSE_OK,
               "parse_argument: label reference parse failed"))
    {
        if (parse_rc == LABEL_PARSE_ERR_EMPTY)
            printf("PARSE_ARGUMENT: EMPTY LABEL REFERENCE!\n");
        else if (parse_rc == LABEL_PARSE_ERR_INVALID)
            printf("PARSE_ARGUMENT: INVALID LABEL REFERENCE!\n");
        else
            printf("PARSE_ARGUMENT: INVALID LABEL REFERENCE!\n");
        return ERR_BAD_ARG;
    }

    long value = 0;

    if (iter != 0)
    {
        asm_label_t* found = asm_find_label(as, label.name, label.length);

        if (!CHECK(ERROR, found != NULL,
                   "parse_argument: undefined label '%.*s'",
                   (int)label.length, label.name))
        {
            printf("PARSE_ARGUMENT: UNDEFINED LABEL!\n");
            return ERR_BAD_ARG;
        }

        value = (long)found->offset;
    }

    if (out_end)   *out_end   = label.name + label.length;
    if (out_value) *out_value = value;

    return OK;
}

static err_t parse_memory_arg(const char* token,
                              long*       out_value,
                              const char** out_end)
{
    if (!token) return ERR_BAD_ARG;

    const char closing = ']';
    const char* inner  = token + 1;

    while (*inner && isspace((unsigned char)*inner)) inner++;

    if (!CHECK(ERROR, *inner != '\0',
               "parse_argument: unexpected end inside brackets"))
    {
        printf("PARSE_ARGUMENT: UNEXPECTED END INSIDE BRACKETS!\n");
        return ERR_BAD_ARG;
    }

    long value = 0;
    const char* inner_end = NULL;

    if ((*inner == 'x' || *inner == 'X') && isdigit((unsigned char)inner[1]))
    {
        err_t rc = parse_register_arg(inner, &value, &inner_end);
        if (rc != OK) return rc;
    }
    else
    {
        char* endptr = NULL;
        value = strtol(inner, &endptr, 10);

        if (!CHECK(ERROR, inner != endptr,
                   "parse_argument: invalid memory literal '%s'", inner))
        {
            printf("PARSE_ARGUMENT: INVALID MEMORY LITERAL!\n");
            return ERR_BAD_ARG;
        }

        inner_end = endptr;
    }

    while (*inner_end && isspace((unsigned char)*inner_end)) inner_end++;

    if (!CHECK(ERROR, *inner_end == closing,
               "parse_argument: missing closing symbol"))
    {
        printf("PARSE_ARGUMENT: MISSING CLOSING SYMBOL!\n");
        return ERR_BAD_ARG;
    }

    if (out_end)   *out_end   = inner_end + 1;
    if (out_value) *out_value = value;

    return OK;
}

static err_t parse_char_literal_arg(const char* token,
                                    long*       out_value,
                                    const char** out_end)
{
    if (!token) return ERR_BAD_ARG;

    const char* inner = token + 1;
    if (!*inner)
    {
        printf("PARSE_ARGUMENT: INVALID SYMBOL!\n");
        return ERR_BAD_ARG;
    }

    long value = (long)(unsigned char)*inner;

    if (!CHECK(ERROR, value < 255 && value >= 0,
               "parse_argument: invalid symbol '%s'", inner))
    {
        printf("PARSE_ARGUMENT: INVALID SYMBOL!\n");
        return ERR_BAD_ARG;
    }

    const char* inner_end = inner + 1;
    while (*inner_end && isspace((unsigned char)*inner_end)) inner_end++;

    if (!CHECK(ERROR, *inner_end == '\'',
               "parse_argument: missing closing symbol"))
    {
        printf("PARSE_ARGUMENT: MISSING CLOSING SYMBOL!\n");
        return ERR_BAD_ARG;
    }

    if (out_end)   *out_end   = inner_end + 1;
    if (out_value) *out_value = value;

    return OK;
}

static err_t parse_numeric_literal_arg(const char* token,
                                       long*       out_value,
                                       const char** out_end)
{
    if (!token) return ERR_BAD_ARG;

    char* endptr = NULL;
    long  value  = strtol(token, &endptr, 10);

    if (!CHECK(ERROR, token != endptr,
               "parse_argument: invalid literal '%s'", token))
    {
        printf("PARSE_ARGUMENT: INVALID LITERAL!\n");
        return ERR_BAD_ARG;
    }

    if (out_end)   *out_end   = endptr;
    if (out_value) *out_value = value;

    return OK;
}

static err_t parse_argument(asm_t* as,
                            const char** cursor,
                            size_t iter,
                            long* out_value)
{
    const char* token = *cursor;
    while (*token && isspace((unsigned char)*token)) token++;

    if (!CHECK(ERROR, *token != '\0', "parse_argument: unexpected end of line"))
        return ERR_BAD_ARG;

    long value = 0;
    const char* endptr = NULL;

    err_t rc = ERR_BAD_ARG;

    switch (*token)
    {
        case 'x':
        case 'X':
            rc = parse_register_arg(token, &value, &endptr);
            break;

        case ':':
            rc = parse_label_arg(as, token, iter, &value, &endptr);
            break;

        case '[':
            rc = parse_memory_arg(token, &value, &endptr);
            break;

        case '\'':
            rc = parse_char_literal_arg(token, &value, &endptr);
            break;

        default:
            rc = parse_numeric_literal_arg(token, &value, &endptr);
            break;
    }

    if (rc != OK) return rc;

    *cursor = endptr;
    if (out_value) *out_value = value;

    return OK;
}

static err_t encode_instruction(asm_t* as,
                                const char* line,
                                size_t iter,
                                unsigned char* buffer,
                                size_t* out_size)
{
    if (!CHECK(ERROR, line != NULL && buffer != NULL && out_size != NULL,
               "encode_instruction: invalid arguments"))
        return ERR_BAD_ARG;

    const char* cursor = line;
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if (!*cursor || *cursor == ';') { *out_size = 0; return OK; }

    char mnemonic[MAX_INSTRUCTION_LEN] = { 0 };
    size_t mn_len = 0;
    while (cursor[mn_len] && !isspace((unsigned char)cursor[mn_len]) &&
           cursor[mn_len] != '[' && mn_len < MAX_INSTRUCTION_LEN - 1)
    {
        mnemonic[mn_len] = cursor[mn_len];
        mn_len++;
    }
    cursor += mn_len;
    mnemonic[mn_len] = '\0';

    if (!CHECK(ERROR,
               !(mn_len == MAX_INSTRUCTION_LEN - 1 &&
                 *cursor && !isspace((unsigned char)*cursor)),
               "encode_instruction: mnemonic too long"))
    {
        printf("ENCODE_INSTRUCTION: MNEMONIC TOO LONG!\n");
        return ERR_BAD_ARG;
    }

    instruction_set opcode = map_instruction(mnemonic);

    if (!CHECK(ERROR, opcode != UNDEF,
               "encode_instruction: unknown instruction '%s'", mnemonic))
    {
        printf("ENCODE_INSTRUCTION: UNKNOWN INSTRUCTION!\n");
        return ERR_BAD_ARG;
    }

    const instruction_t* meta = instruction_get(opcode);

    if (!CHECK(ERROR, meta != NULL,
               "encode_instruction: metadata missing for '%s'", mnemonic))
    {
        printf("ENCODE_INSTRUCTION: METADATA MISSING!\n");
        return ERR_BAD_ARG;
    }

    while (*cursor && isspace((unsigned char)*cursor)) cursor++;

    size_t total    = 0;
    buffer[total++] = (unsigned char)meta->id;
    buffer[total++] = (unsigned char)meta->expected_args;

    for (size_t arg_idx = 0; arg_idx < meta->expected_args; ++arg_idx)
    {
        long value = 0;
        err_t rc   = parse_argument(as, &cursor, iter, &value);

        if (!CHECK(ERROR, rc == OK, "encode_instruction: failed to parse argument"))
            return rc;

        int32_t stored = (int32_t)value;
        memcpy(buffer + total, &stored, sizeof(stored));
        total += sizeof(stored);

        while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    }

    if (!CHECK(ERROR, !*cursor || *cursor == ';',
               "encode_instruction: unexpected token '%s'", cursor))
    {
        printf("ENCODE_INSTRUCTION: UNEXPECTED TOKEN!\n");
        return ERR_BAD_ARG;
    }

    *out_size = total;
    return OK;
}

static size_t process_source(asm_t* as, int pass, FILE* out)
{
    if (!CHECK(ERROR, as != NULL && as->source != NULL,
               "process_source: invalid assembler state"))
    {
        printf("PROCESS_SOURCE: INVALID ASSEMBLER STATE!\n");
        return SIZE_MAX;
    }

    char* cursor   = as->source;
    size_t offset  = 0;
    size_t line_no = 1;

    while (*cursor)
    {
        while (isspace((unsigned char)*cursor)) cursor++;
        if (*cursor == '\0') break;

        char* line_start = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r') cursor++;
        char saved          = *cursor;
        *cursor             = '\0';
        size_t current_line = line_no++;

        char* trimmed = line_start;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

        if (*trimmed == '\0' || *trimmed == ';')
        {
            *cursor = saved;
            if (*cursor) cursor++;
            continue;
        }

        if (*trimmed == ':')
        {
            if (!CHECK(ERROR,
                       process_label_definition(as, trimmed, offset, pass) == OK,
                       "process_source: failed to process label"))
            {
                *cursor = saved;
                return SIZE_MAX;
            }
            *cursor = saved;
            if (*cursor) cursor++;
            continue;
        }

        unsigned char encoded[MAX_LINE_LEN] = { 0 };
        size_t encoded_len = 0;

        if (!CHECK(ERROR, encode_instruction(as, trimmed, pass, encoded, &encoded_len) == OK,
                   "process_source: failed to encode instruction"))
        {
            *cursor = saved;
            return SIZE_MAX;
        }

        if (encoded_len > 0)
        {
            asm_dump_pass_line(as,
                               pass,
                               current_line,
                               offset,
                               trimmed,
                               encoded,
                               encoded_len,
                               DEBUG);
        }

        offset += encoded_len;

        if (pass == 1 && encoded_len > 0 && out)
        {
            size_t written = fwrite(encoded, 1, encoded_len, out);

            if (!CHECK(ERROR, written == encoded_len,
                       "process_source: failed to write encoded instruction"))
            {
                printf("PROCESS_SOURCE: WRITE FAILED!\n");
                *cursor = saved;
                return SIZE_MAX;
            }
        }

        *cursor = saved;
        if (*cursor) cursor++;
    }

    return offset;
}

err_t asm_init(asm_t* as, char* source, size_t source_size)
{
    if (!CHECK(ERROR, as != NULL && source != NULL,
               "asm_init: invalid arguments"))
    {
        printf("ASM_INIT: INVALID ARGUMENTS!\n");
        return ERR_BAD_ARG;
    }

    as->source      = source;
    as->source_size = source_size;

    return OK;
}

void asm_destroy(asm_t* as)
{
    if (!CHECK(ERROR, as != NULL, "asm_destroy: assembler pointer is NULL"))
        return;

    free(as->labels);

    memset(as, 0, sizeof(*as));
}

size_t asm_first_pass(asm_t* as)
{
    size_t result = process_source(as, 0, NULL);
    if (result == SIZE_MAX)
    {
        log_printf(ERROR, "asm_first_pass: failed to parse source");
        printf("ASM_FIRST_PASS: FAILED TO PARSE SOURCE!\n");
    }
    else
    {
        asm_dump_pass_summary(as, 0, result, DEBUG);
    }
    return result;
}

size_t asm_second_pass(asm_t* as, FILE* out)
{
    size_t result = process_source(as, 1, out);
    if (result == SIZE_MAX)
    {
        log_printf(ERROR, "asm_second_pass: failed to emit program");
        printf("ASM_SECOND_PASS: FAILED TO EMIT PROGRAM!\n");
    }
    else
    {
        asm_dump_pass_summary(as, 1, result, DEBUG);
    }
    return result;
}
