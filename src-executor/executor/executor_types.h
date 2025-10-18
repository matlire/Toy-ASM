#ifndef EXECUTOR_TYPES_H
#define EXECUTOR_TYPES_H

#include <stdint.h>
#include <ctype.h>

#include "../../libs/logging/logging.h"

#include "../../libs/stack/stack.h"
#include "../../libs/io/io.h"
#include "../../libs/instruction_set/instruction_set.h"

#define CPU_IR_COUNT 16
#define CPU_FR_COUNT 8

#define RAM_SIZE      128

#define SCREEN_WIDTH  256
#define SCREEN_HEIGHT 64
#define VRAM_SIZE     (SCREEN_WIDTH * SCREEN_HEIGHT)

// Integer register
typedef union
{
    i64_t value;
    char  bytes[CPU_CELL_SIZE];
} cpu_ir_value_t;

typedef struct
{
    const char*    name;
    cpu_ir_value_t value;
} cpu_ir_t;

// Float register
typedef union
{
    f64_t value;
    char  bytes[CPU_CELL_SIZE];
} cpu_fr_value_t;

typedef struct
{
    const char*    name;
    cpu_fr_value_t value;
} cpu_fr_t;

// CPU
typedef struct
{
    stack_id code_stack;
    stack_id ret_stack;
    char*    code;
    size_t   code_size;
    size_t   pc;

    cpu_ir_t x[CPU_IR_COUNT];
    cpu_fr_t fx[CPU_FR_COUNT];

    cell64_t ram[RAM_SIZE];
    char     vram[VRAM_SIZE];

    instruction_set_version_t binary_version;
} cpu_t;

#endif
