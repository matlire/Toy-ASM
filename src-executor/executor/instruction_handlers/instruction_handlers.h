#ifndef INSTRUCTION_HANDLERS_H
#define INSTRUCTION_HANDLERS_H

#include <stddef.h>

#include "../../dumper/dump.h"
#include "../executor_types.h"

typedef err_t (*instruction_executor_fn)(cpu_t* cpu,
                const long* args, size_t arg_count);

err_t exec_hlt   (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_push  (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_pop   (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_out   (cpu_t* cpu, const long* args, size_t arg_count);

err_t exec_add   (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_sub   (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_mul   (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_div   (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_qroot (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_sq    (cpu_t* cpu, const long* args, size_t arg_count);

err_t exec_pushr (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_popr  (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_in    (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_topout(cpu_t* cpu, const long* args, size_t arg_count);

err_t exec_jmp   (cpu_t* cpu, const long* args, size_t arg_count);

err_t exec_call  (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_ret   (cpu_t* cpu, const long* args, size_t arg_count);

err_t exec_pushm (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_popm  (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_pushvm(cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_popvm (cpu_t* cpu, const long* args, size_t arg_count);

err_t exec_dump  (cpu_t* cpu, const long* args, size_t arg_count);
err_t exec_draw  (cpu_t* cpu, const long* args, size_t arg_count);

#define clear() printf("\033[H\033[J")

static err_t exec_pop_operands(cpu_t* cpu, long* lhs, long* rhs)
{
    if (!cpu || !lhs || !rhs) return ERR_BAD_ARG;

    long rhs_val = 0;
    err_t rc     = STACK_POP(cpu->code_stack, rhs_val);
    if (rc != OK) return rc;

    long lhs_val = 0;
    rc           = STACK_POP(cpu->code_stack, lhs_val);
    if (rc != OK) return rc;

    *lhs = lhs_val;
    *rhs = rhs_val;

    return OK;
}

#define DEFINE_COND_JUMP_FUNC(name, op)                                      \
    static err_t exec_##name(cpu_t* cpu, const long* args, size_t arg_count) \
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
            return exec_jmp((cpu), (args), (arg_count));                     \
                                                                             \
        return OK;                                                           \
    }                                                                        \

DEFINE_COND_JUMP_FUNC(jb,  <);
DEFINE_COND_JUMP_FUNC(jbe, <=);
DEFINE_COND_JUMP_FUNC(ja,  >);
DEFINE_COND_JUMP_FUNC(jae, >=);
DEFINE_COND_JUMP_FUNC(je,  ==);
DEFINE_COND_JUMP_FUNC(jne, !=);

#endif
