#include "dump.h"

#include "../executor/executor_types.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define CODE_DUMP_BYTES_PER_LINE 16

static inline void log_cell(logging_level level, const char* label, const cell64_t c)
{
    double d = 0.0;
    memcpy(&d, &c.u64, sizeof(d));
    log_printf(level, "%s[i=%" PRId64 " u=%" PRIu64 " f=%.17f hex=%016" PRIX64 "]",
               (label ? label : ""),
               (i64_t)c.i64, (u64_t)c.u64, d, (u64_t)c.u64);
}

static const char* safe_instruction_name(instruction_set opcode)
{
    const instruction_t* meta = instruction_get(opcode);
    return meta && meta->name ? meta->name : "<unknown>";
}

void cpu_dump_registers(const cpu_t * const cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_registers: cpu pointer is NULL");
        return;
    }

    log_printf(level,
               "Registers (pc=0x%04zx size=%zu version=%u.%u):",
               cpu->pc,
               cpu->code_size,
               cpu->binary_version.major,
               cpu->binary_version.minor);

    log_printf(level,
               "+------------------ INTEGER REGISTERS ------------------+");
    log_printf(level,
               "| idx  | name           | value (dec / hex)                |");
    log_printf(level,
               "+------+----------------+----------------------------------+");

    for (size_t i = 0; i < CPU_IR_COUNT; ++i)
    {
        const cpu_ir_t* reg = &cpu->x[i];
        char row_hdr[64] = { 0 };
        snprintf(row_hdr, sizeof(row_hdr), "| %4zu | %-8s | ", i, reg->name ? reg->name : "x?");
        log_printf(level, "%s", row_hdr);
        log_cell(level, "    ", (cell64_t){ .i64 = reg->value.value });
    }

    log_printf(level, "+-------------------------------------------------------+");

    log_printf(level, "+------------------- FLOAT REGISTERS -------------------+");
    log_printf(level, "| idx  | name     | value                                |");
    log_printf(level, "+------+----------+--------------------------------------+");
    
    for (size_t i = 0; i < CPU_FR_COUNT; ++i)
    {
        const cpu_fr_t* reg = &cpu->fx[i];
        char row_hdr[64] = { 0 };
        snprintf(row_hdr, sizeof(row_hdr), "| %4zu | %-8s | ", i, reg->name ? reg->name : "fx?");
        log_printf(level, "%s", row_hdr);

        cell64_t c; c.f64 = reg->value.value;
        log_cell(level, "    ", c);
    }

    log_printf(level,
               "+------+----------------+----------------------------------+");
}

static void dump_single_stack(logging_level level,
                              const char*   label,
                              stack_id      stack)
{
    if (stack == (size_t)-1)
    {
        log_printf(level, "%s: <uninitialised>", label);
        return;
    }

    log_printf(level, "%s:", label);
    STACK_DUMP(level, stack, OK, label);
}

void cpu_dump_stack(const cpu_t * const cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_stack: cpu pointer is NULL");
        return;
    }

    dump_single_stack(level, "Code stack", cpu->code_stack);
    dump_single_stack(level, "Return stack", cpu->ret_stack);
}

void cpu_dump_ram(const cpu_t * const cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_ram: cpu pointer is NULL");
        return;
    }

    log_printf(level, "RAM dump (cells=%zu):", (size_t)RAM_SIZE);

    const size_t step = 4;
    for (size_t base = 0; base < RAM_SIZE; base += step)
    {
        log_printf(level, "  [0x%04zx..0x%04zx]", base,
                   ((base + step) > RAM_SIZE ? RAM_SIZE : base + step) - 1);

        for (size_t off = 0; off < step && (base + off) < RAM_SIZE; ++off)
        {
            char lbl[32] = { 0 };
            snprintf(lbl, sizeof(lbl), "    ram[%zu]=", base + off);
            log_cell(level, lbl, cpu->ram[base + off]);
        }
    }
}

void cpu_dump_vram(const cpu_t * const cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_vram: cpu pointer is NULL");
        return;
    }

    log_printf(level, "VRAM dump (size=%zu):", (size_t)VRAM_SIZE);

    const int step = 32;

    for (size_t base = 0; base < VRAM_SIZE; base += step)
    {
        char   line[RAM_SIZE] = { 0 };
        size_t len            = (size_t)snprintf(line, sizeof(line), "  0x%04zx:", base);

        for (size_t offset = 0; offset < step && (base + offset) < VRAM_SIZE; ++offset)
        {
            if (len < sizeof(line))
            {
                len += (size_t)snprintf(line + len,
                                        sizeof(line) - len,
                                        " %c",
                                        cpu->vram[base + offset]);
            }
        }

        log_printf(level, "%s", line);
    }
}

static void dump_code_bytes(const unsigned char* code,
                            size_t code_size,
                            size_t start_pc,
                            size_t max_bytes,
                            logging_level level)
{
    if (!code || code_size == 0)
    {
        log_printf(level, "Bytecode: <none>");
        return;
    }

    if (start_pc >= code_size)
    {
        log_printf(level,
                   "Bytecode: start pc 0x%04zx beyond code size %zu",
                   start_pc,
                   code_size);
        return;
    }

    size_t remaining = code_size - start_pc;
    size_t to_dump   = (max_bytes == 0 || max_bytes > remaining) ? remaining : max_bytes;

    log_printf(level,
               "Bytecode window @0x%04zx (showing %zu / %zu bytes):",
               start_pc,
               to_dump,
               code_size);

    const unsigned char* cursor = code + start_pc;
    size_t offset = start_pc;

    while (to_dump > 0)
    {
        size_t line_bytes = (to_dump > CODE_DUMP_BYTES_PER_LINE) ? CODE_DUMP_BYTES_PER_LINE : to_dump;

        char   hex_buf[CODE_DUMP_BYTES_PER_LINE * 3 + 1] = { 0 };
        size_t hex_len = 0;

        for (size_t i = 0; i < line_bytes; ++i)
        {
            hex_len += (size_t)snprintf(hex_buf + hex_len,
                                        sizeof(hex_buf) - hex_len,
                                        "%02X ",
                                        cursor[i]);
            if (hex_len >= sizeof(hex_buf))
            {
                hex_buf[sizeof(hex_buf) - 1] = '\0';
                break;
            }
        }

        log_printf(level, "  0x%04zx: %s", offset, hex_buf);

        cursor  += line_bytes;
        offset  += line_bytes;
        to_dump -= line_bytes;
    }
}

void cpu_dump_code_window(const cpu_t * const cpu,
                          size_t              start_pc,
                          size_t              max_bytes,
                          logging_level       level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_code_window: cpu pointer is NULL");
        return;
    }

    dump_code_bytes((const unsigned char*)cpu->code,
                    cpu->code_size,
                    start_pc,
                    max_bytes,
                    level);
}

void cpu_dump_state(const cpu_t * const cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_state: cpu pointer is NULL");
        return;
    }

    log_printf(level, "=== CPU STATE ===");
    cpu_dump_registers(cpu, level);
    cpu_dump_stack(cpu, level);
    cpu_dump_ram(cpu, level);
    cpu_dump_vram(cpu, level);
    cpu_dump_code_window(cpu, cpu->pc, DUMP_CODE_WINDOW_SIZE, level);
    log_printf(level, "=================");
}

void cpu_dump_step(const cpu_t * const cpu,
                   size_t              pc_before,
                   instruction_set     opcode,
                   const cell64_t*     args,
                   size_t              arg_count,
                   logging_level       level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_step: cpu pointer is NULL");
        return;
    }

    const char* name = safe_instruction_name(opcode);
    log_printf(level,
               "[step] pc=0x%04zx instr=%s (%u) args=%zu",
               pc_before,
               name,
               (unsigned int)opcode,
               arg_count);

    for (size_t i = 0; i < arg_count; ++i)
    {
        char lbl[64] = { 0 };
        snprintf(lbl, sizeof(lbl), "       arg[%zu] = ", i);
        log_cell(level, lbl, args ? args[i] : (cell64_t){ .u64 = 0 });
    }
}

void cpu_dump_final_state(const cpu_t * const cpu,
                          err_t               rc,
                          logging_level       level)
{
    const char* status = err_str(rc);
    log_printf(level,
               "Execution finished with rc=%d (%s)",
               rc,
               status ? status : "unknown");
    cpu_dump_state(cpu, level);
}

void cpu_dump_binary_header(const instruction_binary_header_t * const header,
                            logging_level                         level)
{
    if (!header)
    {
        log_printf(level, "cpu_dump_binary_header: header pointer is NULL");
        return;
    }

    char magic[INSTRUCTION_BINARY_MAGIC_LEN + 1] = { 0 };
    memcpy(magic, header->magic, INSTRUCTION_BINARY_MAGIC_LEN);

    log_printf(level,
               "Loaded header: magic=\"%s\" version=%u.%u code_size=%zu",
               magic,
               (unsigned int)header->version_major,
               (unsigned int)header->version_minor,
               (size_t)header->code_size);
}
