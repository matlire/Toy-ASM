#include <math.h>

#include "instruction_handlers.h"

typedef int (*jump_cmp_t) (long lhs, long rhs);

static err_t pop_operands(cpu_t* cpu, long* lhs, long* rhs)
{
    if (!cpu || !lhs || !rhs) return ERR_BAD_ARG;

    long rhs_val = 0;
    err_t rc     = STACK_POP(cpu->stack, rhs_val);
    if (rc != OK) return rc;

    long lhs_val = 0;
    rc           = STACK_POP(cpu->stack, lhs_val);
    if (rc != OK) return rc;

    *lhs = lhs_val;
    *rhs = rhs_val;

    return OK;
}

#define DEFINE_COMPARATOR(fn, cmp, op) \
    static int cmp(long lhs, long rhs) { return lhs op rhs; }
CONDITIONAL_JUMP_LIST(DEFINE_COMPARATOR)
#undef DEFINE_COMPARATOR

static err_t conditional_jump(cpu_t*      cpu,
                              const long* args,
                              size_t      arg_count,
                              jump_cmp_t  cmp)
{
    if (!cpu  || !cmp)          return ERR_BAD_ARG;
    if (!args || arg_count < 1) return ERR_BAD_ARG;

    long lhs = 0;
    long rhs = 0;

    err_t rc = pop_operands(cpu, &lhs, &rhs);
    if (rc != OK) return rc;

    if (cmp(lhs, rhs))
        return exec_jmp(cpu, args, arg_count);

    return OK;
}

static int ensure_register_index(size_t* out_index, const long* args, size_t arg_count)
{
    if (!out_index || !args || arg_count == 0) return 0;
    long idx = args[0];
    if (idx < 0 || (size_t)idx >= CPU_REGISTER_COUNT) return 0;
    *out_index = (size_t)idx;
    return 1;
}

err_t exec_hlt(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    if (!cpu) return ERR_BAD_ARG;

    cpu->pc = cpu->code_size;
    return OK;
}

err_t exec_push(cpu_t* cpu, const long* args, size_t arg_count)
{
    if (arg_count < 1 || !args) return ERR_BAD_ARG;
    return STACK_PUSH(cpu->stack, args[0]);
}

err_t exec_pop(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long discarded = 0;
    return STACK_POP(cpu->stack, discarded);
}

err_t exec_out(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    err_t rc   = STACK_POP(cpu->stack, value);
    if (rc == OK) printf("%ld\n", value);
    return rc;
}

err_t exec_add(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 + arg2;
    return STACK_PUSH(cpu->stack, res);
}

err_t exec_sub(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 - arg2;
    return STACK_PUSH(cpu->stack, res);
}

err_t exec_mul(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 * arg2;
    return STACK_PUSH(cpu->stack, res);
}

err_t exec_div(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    if (arg2 == 0) return ERR_BAD_ARG;

    long res = arg1 / arg2;
    return STACK_PUSH(cpu->stack, res);
}

err_t exec_qroot(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = (long)sqrt(arg1);
    return STACK_PUSH(cpu->stack, res);
}

err_t exec_sq(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->stack, arg1);
    if (rc != OK) return rc;

    long res = (long)(arg1 * arg1);
    return STACK_PUSH(cpu->stack, res);
}

err_t exec_pushr(cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long value = cpu->x[reg_index].value.value;
    return STACK_PUSH(cpu->stack, value);
}

err_t exec_popr(cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long value = 0;
    err_t rc   = STACK_POP(cpu->stack, value);
    if (rc != OK) return rc;

    cpu->x[reg_index].value.value = (int)value;

    return OK;
}

err_t exec_in(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    printf("Waiting for input: ");
    if (scanf("%ld", &value) != 1) return ERR_BAD_ARG;

    return STACK_PUSH(cpu->stack, value);
}

err_t exec_topout(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    err_t rc   = stack_top(cpu->stack, &value);
    if (rc != OK) return rc;

    printf("%ld\n", value);
    return OK;
}

err_t exec_jmp(cpu_t* cpu, const long* args, size_t arg_count)
{
    if (!cpu || !args || arg_count < 1) return ERR_BAD_ARG;

    cpu->pc = args[0];

    return OK;
}

#define DEFINE_COND_JUMP(fn, cmp, op)                         \
    err_t fn(cpu_t* cpu, const long* args, size_t arg_count)  \
    {                                                         \
        return conditional_jump(cpu, args, arg_count, cmp);   \
    }

CONDITIONAL_JUMP_LIST(DEFINE_COND_JUMP)
#undef DEFINE_COND_JUMP
