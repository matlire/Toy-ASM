#include "executor.h"

#include <stdio.h>

#include "../dumper/dump.h"

DEFINE_STACK_PRINTER_SIMPLE(long, "%ld")

static const instruction_handler_t i_handlers[INSTRUCTION_TABLE_CAPACITY] = {
#define HANDLER_ROW(symbol, name, argc, opcode) [symbol] = &exec_##symbol,
    INSTRUCTION_LIST(HANDLER_ROW)
#undef HANDLER_ROW
};

static inline void stack_assign_cell64_t(void* dst, const void* src, size_t size)
{
    (void)size;
    *(cell64_t*)dst = *(const cell64_t*)src;
}

static inline void hex_bytes_append(char* dst, size_t dstsz,
                                    const void* p, size_t n)
{
    size_t len = dst && dstsz ? strnlen(dst, dstsz) : 0;
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n && len < dstsz; ++i)
    {
        int w = snprintf(dst + len, dstsz - len, "%02X%s",
                         b[i], (i + 1 < n ? " " : ""));
        if (w < 0) break;
        len += (size_t)w;
        if (len >= dstsz) { len = dstsz - 1; break; }
    }
}

static inline int fprint_cell_payloads(FILE* out, const uint64_t u, const int64_t i)
{
    double d;
    memcpy(&d, &u, sizeof(d));

    return fprintf(out,
                   "[i=%" PRId64 " u=%" PRIu64 " f=%.17g hex=%016" PRIX64 "]",
                   (i64_t)i,
                   (u64_t)u,
                   d,
                   (u64_t)u);
}

static int print_cell64_t(FILE* __OUT__, const void* __VP)
{
    const cell64_t* c = (const cell64_t*)__VP;
    return fprint_cell_payloads(__OUT__, c->u64, c->i64);
}

static int sprint_cell64_t(char* __dst, size_t __dstsz, const void* __VP)
{
    const cell64_t* c = (const cell64_t*)__VP;

    size_t len = (__dst && __dstsz) ? strnlen(__dst, __dstsz) : 0;
    if (len >= __dstsz) return 0;

    double d;
    memcpy(&d, &c->u64, sizeof(d));
    int w = snprintf(__dst + len, __dstsz - len,
                     "[i=%" PRId64 " u=%" PRIu64 " f=%.17g hex=%016" PRIX64 "]",
                     (i64_t)c->i64,
                     (u64_t)c->u64,
                     d,
                     (u64_t)c->u64);
    if (w < 0) return 0;

    len = strnlen(__dst, __dstsz);
    if (len + 1 < __dstsz) {
        __dst[len++] = ' ';
        __dst[len]   = '\0';
        hex_bytes_append(__dst, __dstsz, &c->u64, sizeof(c->u64));
    }

    return w;
}
err_t cpu_init(cpu_t* cpu)
{
    if (!CHECK(ERROR, cpu != NULL, "cpu_init: cpu pointer is NULL"))
        return ERR_BAD_ARG;

    memset(cpu, 0, sizeof(*cpu));
    cpu->code_stack           = (size_t)-1;
    cpu->ret_stack            = (size_t)-1;
    cpu->binary_version       = instruction_set_version();
    cpu->code                 = 0;
    cpu->code_size            = 0;
    cpu->pc                   = 0;

    static char ir_names[CPU_IR_COUNT][8] = { { 0 } };
    static char fr_names[CPU_FR_COUNT][8] = { { 0 } };

    for (size_t i = 0; i < CPU_IR_COUNT; i++)
    {
        snprintf(ir_names[i], sizeof(ir_names[i]), "x%zu", i);
        cpu->x[i].name        = ir_names[i];
        cpu->x[i].value.value = 0;
    }

    for (size_t i = 0; i < CPU_FR_COUNT; i++)
    {
        snprintf(fr_names[i], sizeof(fr_names[i]), "fx%zu", i);
        cpu->fx[i].name        = fr_names[i];
        cpu->fx[i].value.value = 0.0;
    }

    for (size_t i = 0; i < RAM_SIZE; i++)
        cpu->ram[i] = (cell64_t){ .u64 = 0 };

    for (size_t i = 0; i < VRAM_SIZE; i++)
        cpu->vram[i] = ' ';

    element_info_t ei = ELEMENT_INFO_INIT(cell64_t);
    ei.copy_fn        = stack_assign_cell64_t;
    err_t rc = stack_ctor(&cpu->code_stack, ei, print_cell64_t, sprint_cell64_t, 
                          STACK_INFO_INIT(code_stack));

    if (!CHECK(ERROR, rc == OK,
               "cpu_init: stack ctor failed rc=%d", rc))
        return rc;

    rc = stack_ctor(&cpu->ret_stack, ei, print_cell64_t, sprint_cell64_t, 
                    STACK_INFO_INIT(ret_stack));
    if (!CHECK(ERROR, rc == OK,
               "cpu_init: stack ctor failed rc=%d", rc))
        return rc;

    return OK;
}

void cpu_destroy(cpu_t* cpu)
{
    if (!CHECK(ERROR, cpu != NULL, "cpu_destroy: cpu pointer is NULL"))
        return;

    if (cpu->code_stack != (size_t)-1)
    {
        stack_dtor(cpu->code_stack);
        cpu->code_stack = (size_t)-1;
    }

    if (cpu->ret_stack != (size_t)-1)
    {
        stack_dtor(cpu->ret_stack);
        cpu->ret_stack = (size_t)-1;
    }

    cpu->code      = NULL;
    cpu->code_size = 0;
    cpu->pc        = 0;
}

err_t load_op_data (operational_data_t * const op_data, const char* const IN_FILE)
{
    FILE* in_file  = load_file(IN_FILE,  "r");
    if(!CHECK(ERROR, in_file != NULL, "CAN'T OPEN FILE!"))
        { printf("CAN'T OPEN FILE!\n"); return ERR_BAD_ARG; }

    ssize_t file_size = get_file_size_stat(IN_FILE);
    if(!CHECK(ERROR, file_size > 0, "INPUT FILE IS EMPTY OR INACCESSIBLE"))
    {
        fclose(in_file);
        printf("INPUT FILE ERROR!\n");
        return ERR_BAD_ARG;
    }

    op_data->in_file     = in_file;
    op_data->buffer_size = file_size;

    return OK;
}

static err_t parse_binary_header(char** cursor,
                                 size_t* remaining,
                                 instruction_set_version_t* binary_version,
                                 size_t* code_stack_size_out)
{
    if (!CHECK(ERROR,
               cursor && *cursor && remaining && binary_version && code_stack_size_out,
               "parse_binary_header: invalid arguments"))
        return ERR_BAD_ARG;

    if (!CHECK(ERROR, *remaining >= INSTRUCTION_BINARY_HEADER_SIZE,
               "parse_binary_header: insufficient bytes for header"))
        return ERR_BAD_ARG;

    instruction_binary_header_t header = { 0 };
    memcpy(&header, *cursor, sizeof(header));

    if (!CHECK(ERROR,
               memcmp(header.magic, INSTRUCTION_BINARY_MAGIC, INSTRUCTION_BINARY_MAGIC_LEN) == 0,
               "parse_binary_header: invalid magic"))
        return ERR_BAD_ARG;

    instruction_set_version_t runtime = instruction_set_version();

    if (!CHECK(ERROR,
                header.version_major <  runtime.major ||
               (header.version_major == runtime.major &&
                header.version_minor <= runtime.minor),
               "parse_binary_header: binary version %u.%u newer than runtime %u.%u",
                header.version_major, header.version_minor,
                runtime.major, runtime.minor))
        return ERR_BAD_ARG;

    binary_version->major = header.version_major;
    binary_version->minor = header.version_minor;

    *cursor    += INSTRUCTION_BINARY_HEADER_SIZE;
    *remaining -= INSTRUCTION_BINARY_HEADER_SIZE;

    size_t available = *remaining;
    size_t code_stack_size = (size_t)header.code_size;

    if (!CHECK(ERROR, code_stack_size <= available,
               "parse_binary_header: code_stack size %zu exceeds available %zu",
               code_stack_size, available))
        return ERR_BAD_ARG;

    *remaining     = code_stack_size;
    *code_stack_size_out = code_stack_size;

    return OK;
}

static err_t exec_loop(cpu_t* cpu, logging_level level)
{
    if (!CHECK(ERROR, cpu != NULL, "exec_loop: cpu pointer is NULL"))
        return ERR_BAD_ARG;

    if (!CHECK(ERROR, cpu->code != NULL, "exec_loop: code_stack buffer is NULL"))
        return ERR_BAD_ARG;

    err_t exec_rc = OK;
    while (cpu->pc < cpu->code_size)
    {
        if (!CHECK(ERROR, cpu->pc + 1 <= cpu->code_size,
                   "exec_loop: truncated instruction header at pc=%zu", cpu->pc))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        size_t pc_before = cpu->pc;

        unsigned char   opcode      = (unsigned char)cpu->code[cpu->pc++];
        instruction_set instruction = (instruction_set)opcode;
        const instruction_t* meta   = instruction_get(instruction);

        if (!CHECK(ERROR, meta != NULL,
                   "exec_loop: metadata missing for opcode_stack %u", opcode))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        const size_t argc = meta->expected_args;
        if (!CHECK(ERROR, argc <= MAX_INSTRUCTION_ARGS, "exec_loop: argc %zu exceeds max", argc))
            return ERR_CORRUPT;
        
        size_t required  = argc * CPU_CELL_SIZE;

        if (!CHECK(ERROR, cpu->pc + required <= cpu->code_size,
                   "exec_loop: truncated arguments for opcode_stack %u", opcode))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        cell64_t args[MAX_INSTRUCTION_ARGS] = { 0 };
        for (size_t arg_idx = 0; arg_idx < argc; ++arg_idx)
        {
            memcpy(&args[arg_idx], cpu->code + cpu->pc, CPU_CELL_SIZE);
            cpu->pc += CPU_CELL_SIZE;
        } 

        instruction_handler_t h = i_handlers[instruction];
        if (!h) return ERR_BAD_ARG;
    
        exec_rc = h(cpu, args, argc);

        if (level == DEBUG && (instruction == CALL || instruction == RET))
        {
            cpu_dump_state(cpu, level);
            cpu_dump_step (cpu, pc_before, instruction, args, argc, level);
        }

        if (exec_rc != OK) break;
    }

    return exec_rc;
}

err_t load_program(operational_data_t * const op_data, cpu_t* cpu)
{
    if (!CHECK(ERROR, op_data != NULL && op_data->in_file != NULL && op_data->buffer_size != 0, 
              "load_program: some data not provided")) return ERR_BAD_ARG;

    if (!CHECK(ERROR, cpu != NULL, "load_program: cpu is null")) return ERR_BAD_ARG;

    op_data->buffer = (char*)calloc(op_data->buffer_size + 1, sizeof(char));
    if (!CHECK(ERROR, op_data->buffer != NULL,
               "load_program: failed to alloc %zu bytes", op_data->buffer_size + 1))
        return ERR_ALLOC;

    size_t read_bytes = fread(op_data->buffer, 1, op_data->buffer_size, op_data->in_file);
    if (!CHECK(ERROR, read_bytes != 0,
               "load_program: failed to read program stream ferror=%d",
               ferror(op_data->in_file)))
    {
        free(op_data->buffer);
        op_data->buffer = NULL;

        return ERR_BAD_ARG;
    }

    op_data->buffer[read_bytes] = '\0';

    char*  cursor    = op_data->buffer;
    size_t remaining = read_bytes;
    instruction_binary_header_t header_snapshot = { 0 };
    int header_captured = 0;

    if (remaining >= INSTRUCTION_BINARY_HEADER_SIZE)
    {
        memcpy(&header_snapshot, cursor, INSTRUCTION_BINARY_HEADER_SIZE);
        header_captured = 1;
    }
    instruction_set_version_t binary_version = { 0 };

    size_t code_size = 0;
    err_t header_rc  = parse_binary_header(&cursor, &remaining, &binary_version, &code_size);
    if (!CHECK(ERROR, header_rc == OK, "load_program: binary header parse failed"))
    {
        free(op_data->buffer);
        op_data->buffer = NULL;
        return header_rc;
    }

    if (header_captured)
        cpu_dump_binary_header(&header_snapshot, DEBUG);

    cpu->binary_version = binary_version;
    cpu->code           = cursor;
    cpu->code_size      = code_size;
    cpu->pc             = 0;

    return OK;
}

err_t exec_stream(cpu_t* cpu, logging_level level)
{
    return exec_loop(cpu, level);
}
