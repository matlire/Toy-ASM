#include "dump.h"

#include <stdio.h>
#include <string.h>

#define CODE_DUMP_BYTES_PER_LINE 16

static const char* safe_instruction_name(instruction_set opcode)
{
    const instruction_t* meta = instruction_get(opcode);
    return meta && meta->name ? meta->name : "<unknown>";
}

void cpu_dump_registers(const cpu_t* cpu, logging_level level)
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
               "+------+----------------+----------------------------------+");
    log_printf(level,
               "| idx  | name           | value (dec / hex)                |");
    log_printf(level,
               "+------+----------------+----------------------------------+");

    for (size_t i = 0; i < CPU_REGISTER_COUNT; ++i)
    {
        const cpu_register_t* reg = &cpu->x[i];
        long value = reg->value.value;
        log_printf(level,
                   "| %4zu | %-14s | %11ld / 0x%016lx |",
                   i,
                   reg->name ? reg->name : "x?",
                   value,
                   (unsigned long)value);
    }

    log_printf(level,
               "+------+----------------+----------------------------------+");
}

static void dump_single_stack(logging_level level,
                              const char* label,
                              stack_id stack)
{
    if (stack == (size_t)-1)
    {
        log_printf(level, "%s: <uninitialised>", label);
        return;
    }

    log_printf(level, "%s:", label);
    STACK_DUMP(level, stack, OK, label);
}

void cpu_dump_stack(const cpu_t* cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_stack: cpu pointer is NULL");
        return;
    }

    dump_single_stack(level, "Code stack", cpu->code_stack);
    dump_single_stack(level, "Return stack", cpu->ret_stack);
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

        char hex_buf[CODE_DUMP_BYTES_PER_LINE * 3 + 1] = { 0 };
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

void cpu_dump_code_window(const cpu_t* cpu,
                          size_t start_pc,
                          size_t max_bytes,
                          logging_level level)
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

void cpu_dump_state(const cpu_t* cpu, logging_level level)
{
    if (!cpu)
    {
        log_printf(level, "cpu_dump_state: cpu pointer is NULL");
        return;
    }

    log_printf(level, "=== CPU STATE ===");
    cpu_dump_registers(cpu, level);
    cpu_dump_stack(cpu, level);
    cpu_dump_code_window(cpu, cpu->pc, DUMP_CODE_WINDOW_SIZE, level);
    log_printf(level, "=================");
}

void cpu_dump_step(const cpu_t* cpu,
                   size_t pc_before,
                   instruction_set opcode,
                   const long* args,
                   size_t arg_count,
                   logging_level level)
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
        long value = args ? args[i] : 0;
        log_printf(level,
                   "       arg[%zu] = %ld (0x%08lx)",
                   i,
                   value,
                   (unsigned long)value);
    }

    cpu_dump_registers(cpu, level);
}

void cpu_dump_final_state(const cpu_t* cpu,
                          err_t rc,
                          logging_level level)
{
    const char* status = err_str(rc);
    log_printf(level,
               "Execution finished with rc=%d (%s)",
               rc,
               status ? status : "unknown");
    cpu_dump_state(cpu, level);
}

void cpu_dump_binary_header(const instruction_binary_header_t* header,
                            logging_level level)
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
