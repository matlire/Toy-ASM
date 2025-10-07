#ifndef INSTRUCTION_HANDLERS_H
#define INSTRUCTION_HANDLERS_H

#include <stddef.h>

#include "../executor_types.h"

#define CONDITIONAL_JUMP_LIST(X) \
    X(exec_jb,  cmp_jb,  <)      \
    X(exec_jbe, cmp_jbe, <=)     \
    X(exec_ja,  cmp_ja,  >)      \
    X(exec_jae, cmp_jae, >=)     \
    X(exec_je,  cmp_je,  ==)     \
    X(exec_jne, cmp_jne, !=)     \

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

#define DECLARE_COND_JUMP(fn, cmp, op) err_t fn(cpu_t* cpu, const long* args, size_t arg_count);
CONDITIONAL_JUMP_LIST(DECLARE_COND_JUMP)
#undef DECLARE_COND_JUMP

#endif
