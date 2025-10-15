#include "executor.h"

DEFINE_STACK_PRINTER_SIMPLE(long, "%ld")

err_t cpu_init(cpu_t* cpu)
{
    if (!CHECK(ERROR, cpu != NULL, "cpu_init: cpu pointer is NULL"))
        return ERR_BAD_ARG;

    memset(cpu, 0, sizeof(*cpu));
    cpu->stack          = (size_t)-1;
    cpu->binary_version = instruction_set_version();
    cpu->code           = NULL;
    cpu->code_size      = 0;
    cpu->pc             = 0;

    for (size_t i = 0; i < CPU_REGISTER_COUNT; i++)
    {
        static char register_name[8] = { 0 };
        sprintf(register_name, "x%ld", i);
        cpu->x[i].name        = register_name;
        cpu->x[i].value.value = 0;
    }

    STACK_INIT(cpu_stack, long);
    if (!CHECK(ERROR, stack_init_rc_cpu_stack == OK,
               "cpu_init: stack ctor failed rc=%d", stack_init_rc_cpu_stack))
        return stack_init_rc_cpu_stack;

    cpu->stack = cpu_stack;

    return OK;
}

void cpu_destroy(cpu_t* cpu)
{
    if (!CHECK(ERROR, cpu != NULL, "cpu_destroy: cpu pointer is NULL"))
        return;

    if (cpu->stack != (size_t)-1)
    {
        stack_dtor(cpu->stack);
        cpu->stack = (size_t)-1;
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
                                 size_t* code_size_out)
{
    if (!CHECK(ERROR,
               cursor && *cursor && remaining && binary_version && code_size_out,
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
    size_t code_size = (size_t)header.code_size;

    if (!CHECK(ERROR, code_size <= available,
               "parse_binary_header: code size %zu exceeds available %zu",
               code_size, available))
        return ERR_BAD_ARG;

    *remaining     = code_size;
    *code_size_out = code_size;

    return OK;
}

static err_t switcher(cpu_t* cpu,
                      const instruction_set instruction,
                      const long* args, size_t arg_count)
{ 
    if (!CHECK(ERROR,
               instruction >= 0 && instruction < INSTRUCTION_TABLE_CAPACITY,
               "switcher: invalid instruction id %d", instruction))
        return ERR_BAD_ARG;

    err_t rc = OK;
    switch (instruction)
    {
        case HLT:    rc = exec_hlt   (cpu, args, arg_count); break;
        case PUSH:   rc = exec_push  (cpu, args, arg_count); break;
        case POP:    rc = exec_pop   (cpu, args, arg_count); break;
        case OUT:    rc = exec_out   (cpu, args, arg_count); break;
        
        case ADD:    rc = exec_add   (cpu, args, arg_count); break;
        case SUB:    rc = exec_sub   (cpu, args, arg_count); break;
        case MUL:    rc = exec_mul   (cpu, args, arg_count); break;
        case DIV:    rc = exec_div   (cpu, args, arg_count); break;
        case QROOT:  rc = exec_qroot (cpu, args, arg_count); break;
        case SQ:     rc = exec_sq    (cpu, args, arg_count); break;

        case IN:     rc = exec_in    (cpu, args, arg_count); break;
        case TOPOUT: rc = exec_topout(cpu, args, arg_count); break;
        case PUSHR:  rc = exec_pushr (cpu, args, arg_count); break;
        case POPR:   rc = exec_popr  (cpu, args, arg_count); break;

        case JMP:    rc = exec_jmp   (cpu, args, arg_count); break;
        case JB:     rc = exec_jb    (cpu, args, arg_count); break;
        case JBE:    rc = exec_jbe   (cpu, args, arg_count); break;
        case JA:     rc = exec_ja    (cpu, args, arg_count); break;
        case JAE:    rc = exec_jae   (cpu, args, arg_count); break;
        case JE:     rc = exec_je    (cpu, args, arg_count); break;
        case JNE:    rc = exec_jne   (cpu, args, arg_count); break;

        default: break;
    }

    return rc;
}

static err_t exec_loop(cpu_t* cpu)
{
    if (!CHECK(ERROR, cpu != NULL, "exec_loop: cpu pointer is NULL"))
        return ERR_BAD_ARG;

    if (!CHECK(ERROR, cpu->code != NULL, "exec_loop: code buffer is NULL"))
        return ERR_BAD_ARG;

    err_t exec_rc = OK;
    while (cpu->pc < cpu->code_size)
    {
        if (!CHECK(ERROR, cpu->pc + 2 <= cpu->code_size,
                   "exec_loop: truncated instruction header at pc=%zu", cpu->pc))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        unsigned char opcode      = (unsigned char)cpu->code[cpu->pc++];
        unsigned char encoded_arg = (unsigned char)cpu->code[cpu->pc++];

        log_printf(DEBUG, "exec: pc=%zu opcode=%u args=%u", cpu->pc - 2, opcode, encoded_arg);

        instruction_set instruction = (instruction_set)opcode;
        const instruction_t* meta   = instruction_get(instruction);

        if (!CHECK(ERROR, meta != NULL,
                   "exec_loop: metadata missing for opcode %u", opcode))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        if (!CHECK(ERROR, encoded_arg <= MAX_INSTRUCTION_ARGS,
                   "exec_loop: encoded arg count %u exceeds max", encoded_arg))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        if (!CHECK(ERROR, meta->expected_args == encoded_arg,
                   "exec_loop: arg count mismatch for opcode %u", opcode))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        size_t required = (size_t)encoded_arg * sizeof(int32_t);
        if (!CHECK(ERROR, cpu->pc + required <= cpu->code_size,
                   "exec_loop: truncated arguments for opcode %u", opcode))
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        long args[MAX_INSTRUCTION_ARGS] = { 0 };
        for (size_t arg_idx = 0; arg_idx < encoded_arg; ++arg_idx)
        {
            int32_t stored = 0;
            memcpy(&stored, cpu->code + cpu->pc, sizeof(stored));
            cpu->pc += sizeof(stored);
            args[arg_idx] = (long)stored;
        }

        exec_rc = switcher(cpu, instruction, args, encoded_arg);

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
    instruction_set_version_t binary_version = { 0 };

    size_t code_size = 0;
    err_t header_rc  = parse_binary_header(&cursor, &remaining, &binary_version, &code_size);
    if (!CHECK(ERROR, header_rc == OK, "load_program: binary header parse failed"))
    {
        free(op_data->buffer);
        op_data->buffer = NULL;
        return header_rc;
    }

    cpu->binary_version = binary_version;
    cpu->code           = cursor;
    cpu->code_size      = code_size;
    cpu->pc             = 0;

    return OK;
}

err_t exec_stream(cpu_t* cpu)
{
    return exec_loop(cpu);
}
