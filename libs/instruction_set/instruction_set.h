#ifndef INSTRUCTIONS_SET_H
#define INSTRUCTIONS_SET_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_INSTRUCTION_LEN  16
#define MAX_LINE_LEN         64
#define MAX_INSTRUCTION_ARGS 4

#define INSTRUCTION_SET_VERSION_MAJOR 0U
#define INSTRUCTION_SET_VERSION_MINOR 5U

#define INSTRUCTION_BINARY_MAGIC_LEN  4U

typedef struct
{
    unsigned char magic[INSTRUCTION_BINARY_MAGIC_LEN];
    unsigned char version_major;
    unsigned char version_minor;
    size_t        code_size;
} instruction_binary_header_t;

#define INSTRUCTION_BINARY_HEADER_SIZE (sizeof(instruction_binary_header_t))

extern const unsigned char INSTRUCTION_BINARY_MAGIC[INSTRUCTION_BINARY_MAGIC_LEN];

typedef struct
{
    unsigned int major;
    unsigned int minor;
} instruction_set_version_t;

#define INSTRUCTION_LIST(X)        \
    X(NOP,    "NOP",    0,   0)    \
                                   \
    X(HLT,    "HLT",    0,   1)    \
    X(PUSH,   "PUSH",   1,   2)    \
    X(POP,    "POP",    0,   3)    \
    X(OUT,    "OUT",    0,   4)    \
    X(TOPOUT, "TOPOUT", 0,   5)    \
                                   \
    X(IN,     "IN",     0,   6)    \
    X(ADD,    "ADD",    0,  10)    \
    X(SUB,    "SUB",    0,  11)    \
    X(MUL,    "MUL",    0,  12)    \
    X(DIV,    "DIV",    0,  13)    \
    X(QROOT,  "QROOT",  0,  14)    \
    X(SQ,     "SQ",     0,  15)    \
                                   \
    X(JMP,    "JMP",    1,  16)    \
    X(JB,     "JB",     1,  17)    \
    X(JBE,    "JBE",    1,  18)    \
    X(JA,     "JA",     1,  19)    \
    X(JAE,    "JAE",    1,  20)    \
    X(JE,     "JE",     1,  21)    \
    X(JNE,    "JNE",    1,  22)    \
    X(PUSHR,  "PUSHR",  1,  33)    \
                                   \
    X(POPR,   "POPR",   1,  34) 

typedef enum
{
#define INSTRUCTION_ENUM(symbol, name, args, opcode) symbol = (opcode),
    INSTRUCTION_LIST(INSTRUCTION_ENUM)
#undef INSTRUCTION_ENUM
    INSTRUCTION_TABLE_CAPACITY,
    UNDEF = 666
} instruction_set;

typedef struct
{
    const char*     name;
    instruction_set id;
    size_t          expected_args;
} instruction_t;

const instruction_t* instruction_get        (instruction_set id);
const instruction_t* instruction_table      (void);
size_t               instruction_table_size (void);

instruction_set      map_instruction        (const char* str);
size_t               expect_arg             (const instruction_set instruction);

instruction_set_version_t  instruction_set_version     (void);
unsigned int               instruction_set_version_code(void);

#endif
