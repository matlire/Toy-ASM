#ifndef INSTRUCTIONS_SET_H
#define INSTRUCTIONS_SET_H

#include <string.h>

#define MAX_INSTRUCTION_LEN 16
#define MAX_LINE_LEN        64

typedef enum
{
    HLT   = 0,
    PUSH  = 1,
    POP   = 2,
    OUT   = 3,
    ADD   = 10,
    SUB   = 11,
    MUL   = 12,
    DIV   = 13,
    QROOT = 14,
    UNDEF = 666
} instruction_set;

instruction_set map_instruction (const char* str);
int expect_arg                  (const instruction_set instruction);

#endif
