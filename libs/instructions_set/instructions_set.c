#include "instructions_set.h"

instruction_set map_instruction(const char* str)
{
    if (!str) return UNDEF;

    if (strcmp(str, "HLT")  == 0)  return HLT;
    if (strcmp(str, "PUSH") == 0)  return PUSH;
    if (strcmp(str, "POP")  == 0)  return POP;
    if (strcmp(str, "OUT")  == 0)  return OUT;
    if (strcmp(str, "ADD")  == 0)  return ADD;
    if (strcmp(str, "SUB")  == 0)  return SUB;
    if (strcmp(str, "MUL")  == 0)  return MUL;
    if (strcmp(str, "DIV")  == 0)  return DIV;
    if (strcmp(str, "QROOT") == 0) return QROOT;

    return UNDEF;
}

int expect_arg(const instruction_set instruction)
{
    if (instruction == PUSH) return 1;
    return 0;
}
