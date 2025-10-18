#include "instruction_handlers.h"

#include <math.h>

static int ensure_register_index(size_t* out_index, const long* args, size_t argc)
{
    if (!out_index || !args || argc == 0) return 0;
    long idx = args[0];
    if (idx < 0 || (size_t)idx >= CPU_REGISTER_COUNT) return 0;
    *out_index = (size_t)idx;
    return 1;
}

err_t exec_NOP(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)cpu; (void)args; (void)argc;
    return OK;
}

err_t exec_HLT(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    if (!cpu) return ERR_BAD_ARG;

    cpu->pc = cpu->code_size;
    return OK;
}

err_t exec_PUSH(cpu_t * const cpu, const long * const args, const size_t argc)
{
    if (argc < 1 || !args) return ERR_BAD_ARG;
    return STACK_PUSH(cpu->code_stack, args[0]);
}

err_t exec_POP(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long discarded = 0;
    return STACK_POP(cpu->code_stack, discarded);
}

err_t exec_OUT(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long value = 0;
    err_t rc   = STACK_POP(cpu->code_stack, value);
    if (rc == OK) printf("%ld\n", value);
    return rc;
}

err_t exec_ADD(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 + arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_SUB(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 - arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_MUL(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    long res = arg1 * arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_DIV(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long arg1 = 0;
    long arg2 = 0;

    err_t rc = exec_pop_operands(cpu, &arg1, &arg2);
    if (rc != OK) return rc;

    if (arg2 == 0) return ERR_BAD_ARG;

    long res = arg1 / arg2;
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_QROOT(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->code_stack, arg1);
    if (rc != OK) return rc;

    long res = (long)sqrt(arg1);
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_SQ(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->code_stack, arg1);
    if (rc != OK) return rc;

    long res = (long)(arg1 * arg1);
    return STACK_PUSH(cpu->code_stack, res);
}

err_t exec_PUSHR(cpu_t * const cpu, const long * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    long value = cpu->x[reg_index].value.value;
    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_POPR(cpu_t * const cpu, const long * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    long value = 0;
    err_t rc   = STACK_POP(cpu->code_stack, value);
    if (rc != OK) return rc;

    cpu->x[reg_index].value.value = (int)value;

    return OK;
}

err_t exec_IN(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long value = 0;
    printf("Waiting for input: ");
    if (scanf("%ld", &value) != 1) return ERR_BAD_ARG;

    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_TOPOUT(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    long value = 0;
    err_t rc   = stack_top(cpu->code_stack, &value);
    if (rc != OK) return rc;

    printf("%ld\n", value);
    return OK;
}

err_t exec_JMP(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)argc;

    cpu->pc = args[0];

    return OK;
}

err_t exec_CALL(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)argc;

    err_t rc = STACK_PUSH(cpu->ret_stack, cpu->pc);
    cpu->pc  = args[0];

    return rc;
}

err_t exec_RET(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;
    
    long arg1 = 0;
    err_t rc  = STACK_POP(cpu->ret_stack, arg1);
    if (rc != OK) return rc;

    cpu->pc = arg1;

    return rc;

}

err_t exec_PUSHM(cpu_t * const cpu, const long * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    long addr  = cpu->x[reg_index].value.value;
    if (addr  >= RAM_SIZE) return ERR_BAD_ARG;
    long value = 0;
    err_t rc   = STACK_POP(cpu->code_stack, value);
    cpu->ram[addr] = value;
    return rc;
}

err_t exec_POPM(cpu_t * const cpu, const long * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    long addr  = cpu->x[reg_index].value.value;
    if (addr  >= RAM_SIZE) return ERR_BAD_ARG;
    long value = cpu->ram[addr];
    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_PUSHVM(cpu_t * const cpu, const long * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    long addr = cpu->x[reg_index].value.value;
    if (addr >= VRAM_SIZE) return ERR_BAD_ARG;
    long value = cpu->vram[addr];
    return STACK_PUSH(cpu->code_stack, value);
}

err_t exec_POPVM(cpu_t * const cpu, const long * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_register_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    long addr  = cpu->x[reg_index].value.value;
    if (addr  >= VRAM_SIZE) return ERR_BAD_ARG;
    long value = 0;
    err_t rc   = STACK_POP(cpu->code_stack, value);
    cpu->vram[addr] = value;
    return rc;
}

err_t exec_DRAW(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    clear();

    for (size_t row = 0; row < SCREEN_HEIGHT; ++row)
    {
        char   line[SCREEN_WIDTH + 1] = { 0 };
        size_t len                    = 0;

        for (size_t col = 0; col < SCREEN_WIDTH; ++col)
        {
            const size_t idx = row * SCREEN_WIDTH + col;
            const char    ch = cpu->vram[idx];

            if (len + 2 >= sizeof(line)) break;

            line[len++] = ch;
        }

        if (len > 0)
            line[len - 1] = '\0';
        else
            line[0] = '\0';

        printf("%s\n", line);
    }

    return OK;
}

err_t exec_CLEANVM(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    for (size_t i = 0; i < VRAM_SIZE; i++)
    {
        cpu->vram[i] = ' ';
    }

    return OK;
}

err_t exec_DUMP(cpu_t * const cpu, const long * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cpu_dump_state(cpu, INFO);

    return OK;
}

#define DEFINE_COND_JUMP_FUNC(name, op)                                      \
    err_t exec_##name(cpu_t* cpu, const long* args, size_t arg_count)        \
    {                                                                        \
        if (!(cpu)) return ERR_BAD_ARG;                                      \
        if (!(args) || (arg_count) < 1) return ERR_BAD_ARG;                  \
                                                                             \
        long lhs = 0;                                                        \
        long rhs = 0;                                                        \
                                                                             \
        err_t rc = exec_pop_operands((cpu), &lhs, &rhs);                     \
        if (rc != OK) return rc;                                             \
                                                                             \
        if (lhs op rhs)                                                      \
            return exec_JMP((cpu), (args), (arg_count));                     \
                                                                             \
        return OK;                                                           \
    }                                                                        \

DEFINE_COND_JUMP_FUNC(JB,  <);
DEFINE_COND_JUMP_FUNC(JBE, <=);
DEFINE_COND_JUMP_FUNC(JA,  >);
DEFINE_COND_JUMP_FUNC(JAE, >=);
DEFINE_COND_JUMP_FUNC(JE,  ==);
DEFINE_COND_JUMP_FUNC(JNE, !=);

