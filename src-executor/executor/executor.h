#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "../../libs/instructions_set/instructions_set.h"

#include "../../libs/logging/logging.h"

#include "../../libs/stack/stack.h"
#include "../../libs/io/io.h"

typedef long reg_t;

#define CPU_REGISTER_SIZE (sizeof(reg_t))
#define CPU_REGISTER_COUNT 8

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
    stack_id       stack;
    char*          code;
    size_t         code_size;
    size_t         pc;
    cpu_register_t x[CPU_REGISTER_COUNT];

    instruction_set_version_t binary_version;
} cpu_t;

err_t cpu_init    (cpu_t* cpu);
void  cpu_destroy (cpu_t* cpu);

#define CPU_INIT(var)                          \
    cpu_t var = { 0 };                         \
    err_t cpu_init_rc_##var = cpu_init(&(var))

err_t load_program (operational_data_t * const op_data, cpu_t* cpu);
err_t exec_stream  (cpu_t* cpu);
err_t load_op_data (operational_data_t * const op_data, const char* const IN_FILE);

#endif
