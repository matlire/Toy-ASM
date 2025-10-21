#ifndef INSTRUCTION_HANDLERS_H
#define INSTRUCTION_HANDLERS_H

#include <stddef.h>

#include "../../dumper/dump.h"
#include "../executor_types.h"
#include <time.h>
#include <errno.h>

#include "../../../libs/instruction_set/instruction_set.h"

#define DECL_HANDLER(symbol, name, argc, opcode) \
    err_t exec_##symbol(cpu_t * const, const cell64_t * const, const size_t);
INSTRUCTION_LIST(DECL_HANDLER)
#undef DECL_HANDLER

#define clear() printf("\033[3J\033[H\033[2J")

static err_t exec_pop_operands(cpu_t* cpu, cell64_t* lhs, cell64_t* rhs)
{
    if (!cpu || !lhs || !rhs) return ERR_BAD_ARG;

    cell64_t rhs_val = { 0 };
    err_t rc         = STACK_POP(cpu->code_stack, rhs_val);
    if (rc != OK) return rc;

    cell64_t lhs_val = { 0 };
    rc               = STACK_POP(cpu->code_stack, lhs_val);
    if (rc != OK) return rc;

    *lhs = lhs_val;
    *rhs = rhs_val;

    return OK;
}

#endif
