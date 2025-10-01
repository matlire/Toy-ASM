#include "executor.h"

DEFINE_STACK_PRINTER_SIMPLE(long, "%ld")

static instruction_set read_instruction_code(char** cursor)
{
    if (!cursor || !*cursor) return UNDEF;

    char* p = *cursor;
    for (size_t i = 0; i < 4; i++)
        if (p[i] == '\0' || !isdigit(p[i])) return UNDEF;

    char opcode_buf[5] = { 0 };
    memcpy(opcode_buf, p, 4);
    long code = strtol(opcode_buf, NULL, 10);
    instruction_set instruction = (instruction_set)code;

    *cursor = p + 4;
    return instruction;
}

static err_t read_optional_argument(char** cursor, long * const value)
{
    if (!cursor || !*cursor || !value) return ERR_BAD_ARG;

    char* p = *cursor;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') return ERR_BAD_ARG;

    char* endptr = NULL;
    long  parsed = strtol(p, &endptr, 10);
    if (endptr == p) return ERR_BAD_ARG;

    *value  = parsed;
    *cursor = endptr;

    return OK;
}

static err_t add_impl(const long lhs, const long rhs, long* out)
{
    if (!out) return ERR_BAD_ARG;
    *out = lhs + rhs;
    return OK;
}

static err_t sub_impl(const long lhs, const long rhs, long* out)
{
    if (!out) return ERR_BAD_ARG;
    *out = lhs - rhs;
    return OK;
}

static err_t mul_impl(const long lhs, const long rhs, long* out)
{
    if (!out) return ERR_BAD_ARG;
    *out = lhs * rhs;
    return OK;
}

static err_t div_impl(const long lhs, const long rhs, long* out)
{
    if (!out) return ERR_BAD_ARG;
    if (rhs == 0) return ERR_BAD_ARG;
    *out = lhs / rhs;
    return OK;
}

static err_t exec_binary_op(const stack_id stack,
                            err_t (*op)(long, long, long*))
{
    long rhs = 0;
    long lhs = 0;

    err_t rc = STACK_POP(stack, rhs);
    if (rc != OK) return rc;

    rc = STACK_POP(stack, lhs);
    if (rc != OK) return rc;

    long result = 0;
    err_t op_rc = op(lhs, rhs, &result);
    if (op_rc != OK) return op_rc;

    return STACK_PUSH(stack, result);
}

static err_t switcher(const stack_id stack, char** cursor, 
                      const instruction_set instruction, const long arg)
{
    err_t exec_rc = OK;

    switch(instruction)
        {
            case PUSH:
                exec_rc = STACK_PUSH(stack, arg);
                break;
            case POP:
            {
                long discarded = 0;
                exec_rc = STACK_POP(stack, discarded);
                break;
            }
            case OUT:
            {
                long value  = 0;
                exec_rc     = STACK_POP(stack, value);
                if (exec_rc == OK) printf("%ld\n", value); 
                break;
            }
            case ADD:
                exec_rc = exec_binary_op(stack, add_impl);
                break;
            case SUB:
                exec_rc = exec_binary_op(stack, sub_impl);
                break;
            case MUL:
                exec_rc = exec_binary_op(stack, mul_impl);
                break;
            case DIV:
            {
                exec_rc = exec_binary_op(stack, div_impl);
                break;
            }
            case HLT:
                exec_rc = OK;
                *cursor = strchr(*cursor, '\0');
                break;
            default:
                exec_rc = ERR_BAD_ARG;
                break;
        }

    return exec_rc;
}

static err_t exec_loop(const stack_id stack, char** cursor)
{
    char* p       = *cursor;
    err_t exec_rc = OK;
    while (*p)
    {
        if (isspace(*p) || *p == '|') { p++; continue; }

        instruction_set instruction = read_instruction_code(&p);
        if (instruction == UNDEF) { exec_rc = ERR_BAD_ARG; break; }

        long arg = 0;
        if (expect_arg(instruction)) 
        {
            exec_rc = read_optional_argument(&p, &arg);
            if (exec_rc != OK) break;
        }

        while (*p && isspace(*p)) p++;

        if (*p != '|') { exec_rc = ERR_BAD_ARG; break; }
        p++;

        exec_rc = switcher(stack, &p, instruction, arg);
        
        if (exec_rc != OK) break;
    }

    *cursor = p;

    return exec_rc;
}

err_t exec_stream(operational_data_t* op_data)
{
    if (!op_data || !op_data->in_file || op_data->buffer_size == 0)
        return ERR_BAD_ARG;

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

    
    STACK_INIT(vm_stack, long);
    if (stack_init_rc_vm_stack != OK)
    {
        free(op_data->buffer);
        op_data->buffer = NULL;
        return stack_init_rc_vm_stack;
    }
    
    err_t exec_rc = OK;
    char* cursor  = op_data->buffer;

    exec_rc = exec_loop(vm_stack, &cursor);

    stack_dtor(vm_stack);

    return exec_rc;
}
