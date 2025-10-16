#include "instruction_handlers.h"

#include <math.h>

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
    return STACK_PUSH(cpu->code_stack, args[0]);
}

err_t exec_pop(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long discarded = 0;
    return STACK_POP(cpu->code_stack, discarded);
}

err_t exec_out(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    err_t rc   = STACK_POP(cpu->code_stack, value);
    if (rc == OK) printf("%ld\n", value);
    return rc;
}

err_t exec_add(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 + arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_sub(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 - arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_mul(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 * arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_div(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    if (arg2 == 0) return ERR_BAD_ARG;

    long res = arg1 / arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_qroot(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->code_stack, arg1);
    if (rc != OK) return rc;

    long res = (long)sqrt(arg1);
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_sq(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->code_stack, arg1);
    if (rc != OK) return rc;

    long res = (long)(arg1 * arg1);
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_pushr(cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long value = cpu->x[reg_index].value.value;
    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_popr(cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long value = 0;
    err_t rc   = STACK_POP(cpu->code_stack, value);
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

    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_topout(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    long value = 0;
    err_t rc   = stack_top(cpu->code_stack, &value);
    if (rc != OK) return rc;

    printf("%ld\n", value);
    return OK;
}

err_t exec_jmp(cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)arg_count;

    cpu->pc = args[0];

    return OK;
}

err_t exec_call  (cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)arg_count;

    err_t rc = STACK_PUSH(cpu->ret_stack, cpu->pc);
    cpu->pc  = args[0];

    return rc;
}

err_t exec_ret   (cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;
    
    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->ret_stack, arg1);
    if (rc != OK) return rc;

    cpu->pc = arg1;

    return rc;

}

err_t exec_pushm (cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long addr = cpu->x[reg_index].value.value;
    if (addr >= RAM_SIZE) return ERR_BAD_ARG;
    long value = 0;
    err_t rc = STACK_POP(cpu->code_stack, value);
    cpu->ram[addr] = value;
    return rc;
}

err_t exec_popm  (cpu_t* cpu, const long* args, size_t arg_count)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, arg_count)) return ERR_BAD_ARG;

    long addr = cpu->x[reg_index].value.value;
    if (addr >= RAM_SIZE) return ERR_BAD_ARG;
    long value = cpu->ram[addr];
    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_dump  (cpu_t* cpu, const long* args, size_t arg_count)
{
    (void)args;
    (void)arg_count;

    cpu_dump_state(cpu, DEBUG);

    return OK;
}
