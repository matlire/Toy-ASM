#include "dump.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ASM_DUMP_MAX_LINES 256
#define ASM_DUMP_MAX_TEXT  (MAX_LINE_LEN + 1)

typedef struct
{
    size_t         line_no;
    size_t         offset;
    char           assembly[ASM_DUMP_MAX_TEXT];
    unsigned char  bytecode[MAX_LINE_LEN];
    size_t         bytecode_len;
} asm_dump_line_t;

typedef struct
{
    bool            capturing;
    asm_dump_line_t lines[ASM_DUMP_MAX_LINES];
    size_t          line_count;
} asm_pass_dump_t;

static asm_pass_dump_t g_pass_dumps[2] = { 0 };

static void asm_dump_reset_pass(int pass)
{
    if (pass < 0 || pass >= (int)(sizeof(g_pass_dumps) / sizeof(g_pass_dumps[0])))
        return;

    g_pass_dumps[pass].capturing  = false;
    g_pass_dumps[pass].line_count = 0;
    memset(g_pass_dumps[pass].lines, 0, sizeof(g_pass_dumps[pass].lines));
}

static const char* pass_title(int pass)
{
    switch (pass)
    {
        case 0: return "First pass";
        case 1: return "Second pass";
        default: return "Pass";
    }
}

static void format_bytecode(const unsigned char* data,
                            size_t size,
                            char*  out,
                            size_t out_size)
{
    if (!out || out_size == 0)
        return;

    if (!data || size == 0)
    {
        out[0] = '\0';
        return;
    }

    (void)snprintf(out, out_size, "%02X %02X", (unsigned)data[0], (unsigned)data[1]);
}

static void format_args(const unsigned char* data,
                        size_t size,
                        char*  out,
                        size_t out_size)
{
    if (!out || out_size == 0)
        return;

    out[0] = '\0';

    if (!data || size <= 2)
        return;

    unsigned char expected      = data[1];
    const unsigned char* cursor = data + 2;
    size_t remaining            = size - 2;
    size_t written              = 0;

    for (unsigned int idx = 0; idx < expected && remaining >= CPU_CELL_SIZE; ++idx)
    {
        cell64_t c = { 0 };
        memcpy(&c, cursor, CPU_CELL_SIZE);
        cursor    += CPU_CELL_SIZE;
        remaining -= CPU_CELL_SIZE;

        double d = 0.0; memcpy(&d, &c.u64, sizeof(d));

        int rc = snprintf(out + written,
                          (written < out_size) ? out_size - written : 0,
                          "%s0x%016" PRIX64 "/%" PRId64 "/%.6g",
                          (idx == 0) ? "" : " ",
                          (uint64_t)c.u64, (int64_t)c.i64, d);
        if (rc < 0) break;

        written += (size_t)rc;
        if (written >= out_size) {
            out[out_size - 1] = '\0';
            break;
        }
    }}

static void format_assembly(const char* src,
                            char*  dst,
                            size_t dst_size)
{
    if (!dst || dst_size == 0)
        return;

    dst[0] = '\0';
    if (!src)
        return;

    while (*src && isspace((unsigned char)*src)) src++;

    size_t to_copy = strnlen(src, dst_size - 1);
    memcpy(dst, src, to_copy);
    dst[to_copy] = '\0';
}

static void table_border(logging_level level)
{
    log_printf(level,
               "+-------+----------------------+------------+----------------------+");
}

static void table_header(logging_level level)
{
    table_border(level);
    log_printf(level,
               "|  PC   | ASSEMBLY             | BYTECODE   | ARGS                          |");
    table_border(level);
}
static void table_footer(logging_level level)
{
    table_border(level);
}

void asm_dump_pass_line(const asm_t*         as,
                        int                  pass,
                        size_t               line_no,
                        size_t               offset_before,
                        const char*          raw_source,
                        const unsigned char* encoded,
                        size_t               encoded_len,
                        logging_level        level)
{
    (void)as;

    if (pass < 0 || pass >= (int)(sizeof(g_pass_dumps) / sizeof(g_pass_dumps[0])))
        return;

    asm_pass_dump_t* dump = &g_pass_dumps[pass];

    if (!dump->capturing)
    {
        asm_dump_reset_pass(pass);
        dump->capturing = true;
    }

    if (dump->line_count >= ASM_DUMP_MAX_LINES)
    {
        log_printf(level,
                   "[asm-dump] pass %d exceeded max tracked lines (%d)",
                   pass,
                   ASM_DUMP_MAX_LINES);
        return;
    }

    asm_dump_line_t* line = &dump->lines[dump->line_count++];

    line->line_no = line_no;
    line->offset  = offset_before;
    line->bytecode_len = (encoded_len <= MAX_LINE_LEN) ? encoded_len : MAX_LINE_LEN;

    if (encoded && encoded_len)
        memcpy(line->bytecode, encoded, line->bytecode_len);
    else
        memset(line->bytecode, 0, sizeof(line->bytecode));

    format_assembly(raw_source, line->assembly, sizeof(line->assembly));
}

void asm_dump_label_table(const asm_t* as, logging_level level)
{
    if (!as)
    {
        log_printf(level, "Labels: assembler pointer is NULL");
        return;
    }

    log_printf(level, "Labels:");

    if (as->label_count == 0)
    {
        log_printf(level, "  <none>");
        return;
    }

    for (size_t i = 0; i < as->label_count; ++i)
    {
        const asm_label_t* label = &as->labels[i];
        log_printf(level,
                   "  [%2zu] %-16.*s -> 0x%04zx",
                   i,
                   (int)label->name_len,
                   label->name ? label->name : "<null>",
                   label->offset);
    }
}

void asm_dump_pass_summary(const asm_t* as,
                           int pass,
                           size_t total_bytes,
                           logging_level level)
{
    if (pass < 0 || pass >= (int)(sizeof(g_pass_dumps) / sizeof(g_pass_dumps[0])))
        return;

    asm_pass_dump_t* dump = &g_pass_dumps[pass];

    log_printf(level, "%s:", pass_title(pass));
    table_header(level);

    for (size_t i = 0; i < dump->line_count; ++i)
    {
        const asm_dump_line_t* line = &dump->lines[i];

        char bytecode[32] = { 0 };
        char args[64]     = { 0 };

        format_bytecode(line->bytecode, line->bytecode_len, bytecode, sizeof(bytecode));
        format_args(line->bytecode, line->bytecode_len, args, sizeof(args));

        log_printf(level,
                   "|[%4zu]| %-20.20s | %-10.10s | %-20.20s |",
                   line->offset,
                   line->assembly,
                   bytecode,
                   args[0] ? args : "");
    }

    table_footer(level);
    log_printf(level, "Total emitted bytes: %zu", total_bytes);

    if (pass == 0)
        asm_dump_label_table(as, level);

    asm_dump_reset_pass(pass);
}

void asm_dump_header(const instruction_binary_header_t* header,
                     logging_level level)
{
    if (!header)
    {
        log_printf(level, "asm_dump_header: header pointer is NULL");
        return;
    }

    char magic[INSTRUCTION_BINARY_MAGIC_LEN + 1] = { 0 };
    memcpy(magic, header->magic, INSTRUCTION_BINARY_MAGIC_LEN);

    log_printf(level,
               "Binary header: magic=\"%s\" version=%u.%u code_size=%zu",
               magic,
               (unsigned int)header->version_major,
               (unsigned int)header->version_minor,
               (size_t)header->code_size);
}
