#ifndef EXECUTOR_TYPES_H
#define EXECUTOR_TYPES_H

#include <stdint.h>
#include <ctype.h>

#include "../../libs/logging/logging.h"

#include "../../libs/stack/stack.h"
#include "../../libs/io/io.h"
#include "../../libs/instruction_set/instruction_set.h"

typedef int64_t reg_t;

#define CPU_REGISTER_SIZE (sizeof(reg_t))
#define CPU_REGISTER_COUNT 8

#define RAM_SIZE  128

#define SCREEN_WIDTH  64
#define SCREEN_HEIGHT 16
#define VRAM_SIZE     SCREEN_WIDTH * SCREEN_HEIGHT

typedef union
{
    reg_t value;
    char  bytes[CPU_REGISTER_SIZE];
} cpu_register_value_t;

typedef struct
{
    const char*          name;
    cpu_register_value_t value;
} cpu_register_t;

typedef struct
{
    stack_id       code_stack;
    stack_id       ret_stack;
    char*          code;
    size_t         code_size;
    size_t         pc;
    cpu_register_t x[CPU_REGISTER_COUNT];

    long           ram[RAM_SIZE];
    char           vram[VRAM_SIZE];

    instruction_set_version_t binary_version;
} cpu_t;

#endif
