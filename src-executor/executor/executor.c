#include "executor.h"

DEFINE_STACK_PRINTER_SIMPLE(long, "%ld")

err_t cpu_init(cpu_t* cpu)
{
    if (!cpu) return ERR_BAD_ARG;

    memset(cpu, 0, sizeof(*cpu));
    cpu->stack          = (size_t)-1;
    cpu->binary_version = instruction_set_version();
    cpu->code           = NULL;
    cpu->code_size      = 0;
    cpu->pc             = 0;

    static const char* REGISTER_NAMES[CPU_REGISTER_COUNT] =
    {
        "x0", "x1", "x2", "x3",
        "x4", "x5", "x6", "x7"
    };

    for (size_t i = 0; i < CPU_REGISTER_COUNT; i++)
    {
        cpu->x[i].name        = REGISTER_NAMES[i];
        cpu->x[i].value.value = 0;
    }

    STACK_INIT(cpu_stack, long);
    if (stack_init_rc_cpu_stack != OK)
        return stack_init_rc_cpu_stack;

    cpu->stack = cpu_stack;

    return OK;
}

void cpu_destroy(cpu_t* cpu)
{
    if (!cpu) return;

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
                                 instruction_set_version_t* binary_version)
{
    if (!cursor || !*cursor || !remaining || !binary_version) return ERR_BAD_ARG;

    if (*remaining < INSTRUCTION_BINARY_HEADER_SIZE) return ERR_BAD_ARG;

    const uint8_t* bytes = (const uint8_t*)(*cursor);

    if (memcmp(bytes, INSTRUCTION_BINARY_MAGIC, INSTRUCTION_BINARY_MAGIC_LEN) != 0)
        return ERR_BAD_ARG;

    unsigned int major = bytes[INSTRUCTION_BINARY_MAGIC_LEN];
    unsigned int minor = bytes[INSTRUCTION_BINARY_MAGIC_LEN + 1];

    instruction_set_version_t runtime = instruction_set_version();

    if (major >  runtime.major) return ERR_BAD_ARG;
    if (major == runtime.major && 
        minor >  runtime.minor) return ERR_BAD_ARG;

    binary_version->major = major;
    binary_version->minor = minor;

    *cursor    += INSTRUCTION_BINARY_HEADER_SIZE;
    *remaining -= INSTRUCTION_BINARY_HEADER_SIZE;

    return OK;
}

typedef err_t (*instruction_executor_fn)(cpu_t* cpu,
                                         const long* args, size_t arg_count);

static err_t exec_hlt   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_push  (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_pop   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_out   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_add   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_sub   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_mul   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_div   (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_qroot (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_sq    (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_pushr (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_popr  (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_in    (cpu_t* cpu, const long* args, size_t arg_count);
static err_t exec_topout(cpu_t* cpu, const long* args, size_t arg_count);

static err_t exec_hlt(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    if (!cpu) return ERR_BAD_ARG;

    cpu->pc = cpu->code_size;
    return OK;
}

static err_t exec_push(cpu_t* cpu, const long* args, size_t arg_count)
{
    if (arg_count < 1 || !args) return ERR_BAD_ARG;
    return STACK_PUSH(cpu->stack, args[0]);
}

static err_t exec_pop(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long discarded = 0;
    return STACK_POP(cpu->stack, discarded);
}

static err_t exec_out(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    err_t rc   = STACK_POP(cpu->stack, value);
    if (rc == OK) printf("%ld\n", value);
    return rc;
}

static err_t exec_add(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = STACK_POP(cpu->stack, arg2);
    if (rc != OK) return rc;

    rc = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = arg1 + arg2;
    return STACK_PUSH(cpu->stack, res);
}

static err_t exec_sub(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = STACK_POP(cpu->stack, arg2);
    if (rc != OK) return rc;

    rc = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = arg1 - arg2;
    return STACK_PUSH(cpu->stack, res);
}

static err_t exec_mul(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = STACK_POP(cpu->stack, arg2);
    if (rc != OK) return rc;

    rc = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = arg1 * arg2;
    return STACK_PUSH(cpu->stack, res);
}

static err_t exec_div(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = STACK_POP(cpu->stack, arg2);
    if (rc != OK) return rc;

    rc = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    if (arg2 == 0) return ERR_BAD_ARG;

    long res = arg1 / arg2;
    return STACK_PUSH(cpu->stack, res);
}

static err_t exec_qroot(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = (long)sqrt(arg1);
    return STACK_PUSH(cpu->stack, res);
}

static err_t exec_sq(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = (long)(arg1 * arg1);
    return STACK_PUSH(cpu->stack, res);
}

static int ensure_register_index(size_t* out_index, const long* args, size_t arg_count)
{
    if (!out_index || !args || arg_count == 0) return 0;
    long idx = args[0];
    if (idx < 0 || (size_t)idx >= CPU_REGISTER_COUNT) return 0;
    *out_index = (size_t)idx;
    return 1;
}

static err_t exec_pushr(cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long value = cpu->x[reg_index].value.value;
    return STACK_PUSH(cpu->stack, value);
}

static err_t exec_popr(cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long value = 0;
    err_t rc   = STACK_POP(cpu->stack, value);
    if (rc != OK) return rc;

    cpu->x[reg_index].value.value = (int)value;

    return OK;
}

static err_t exec_in(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    printf("Waiting for input: ");
    if (scanf("%ld", &value) != 1) return ERR_BAD_ARG;

    return STACK_PUSH(cpu->stack, value);
}

static err_t exec_topout(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    err_t rc   = stack_top(cpu->stack, &value);
    if (rc != OK) return rc;

    printf("%ld\n", value);
    return OK;
}

static err_t switcher(cpu_t* cpu,
                      const instruction_set instruction,
                      const long* args, size_t arg_count)
{
    static const instruction_executor_fn EXECUTORS[INSTRUCTION_TABLE_CAPACITY] =
    {
        [HLT]    = exec_hlt,
        [PUSH]   = exec_push,
        [POP]    = exec_pop,
        [OUT]    = exec_out,
        [ADD]    = exec_add,
        [SUB]    = exec_sub,
        [MUL]    = exec_mul,
        [DIV]    = exec_div,
        [QROOT]  = exec_qroot,
        [SQ]     = exec_sq,
        [PUSHR]  = exec_pushr,
        [POPR]   = exec_popr,
        [IN]     = exec_in,
        [TOPOUT] = exec_topout
    };

    if (instruction < 0 || instruction >= INSTRUCTION_TABLE_CAPACITY) return ERR_BAD_ARG;

    instruction_executor_fn handler = EXECUTORS[instruction];
    if (!handler) return ERR_BAD_ARG;

    return handler(cpu, args, arg_count);
}

static err_t exec_loop(cpu_t* cpu)
{
    if (!cpu || !cpu->code) return ERR_BAD_ARG;

    err_t exec_rc = OK;
    while (cpu->pc < cpu->code_size)
    {
        if (cpu->pc + 2 > cpu->code_size)
        {
            exec_rc = ERR_BAD_ARG;
            break;
        }

        uint8_t opcode      = (uint8_t)cpu->code[cpu->pc++];
        uint8_t encoded_arg = (uint8_t)cpu->code[cpu->pc++];

        instruction_set instruction = (instruction_set)opcode;
        const instruction_t* meta   = instruction_get(instruction);

        if (!meta) { exec_rc = ERR_BAD_ARG; break; }

        if (encoded_arg > MAX_INSTRUCTION_ARGS)
            { exec_rc = ERR_BAD_ARG; break; }

        if (meta->expected_args != encoded_arg)
            { exec_rc = ERR_BAD_ARG; break; }

        size_t required = (size_t)encoded_arg * sizeof(int32_t);
        if (cpu->pc + required > cpu->code_size)
            { exec_rc = ERR_BAD_ARG; break; }

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
    if (!op_data->buffer) return ERR_ALLOC;

    size_t read_bytes = fread(op_data->buffer, 1, op_data->buffer_size, op_data->in_file);
    if (read_bytes == 0)
    {
        if (ferror(op_data->in_file))
        {
            log_printf(ERROR, "exec_stream: failed to read program stream");
        }

        free(op_data->buffer);
        op_data->buffer = NULL;

        return ERR_BAD_ARG;
    }

    op_data->buffer[read_bytes] = '\0';

    char*  cursor    = op_data->buffer;
    size_t remaining = read_bytes;
    instruction_set_version_t binary_version = { 0 };

    err_t header_rc = parse_binary_header(&cursor, &remaining, &binary_version);
    if (header_rc != OK)
    {
        free(op_data->buffer);
        op_data->buffer = NULL;
        return header_rc;
    }

    cpu->binary_version = binary_version;
    cpu->code           = cursor;
    cpu->code_size      = remaining;
    cpu->pc             = 0;

    return OK;
}

err_t exec_stream(cpu_t* cpu)
{
    return exec_loop(cpu);
}
