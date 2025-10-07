#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "executor_types.h"
#include "instruction_handlers/instruction_handlers.h"

err_t cpu_init    (cpu_t* cpu);
void  cpu_destroy (cpu_t* cpu);

#define CPU_INIT(var)                          \
    cpu_t var = { 0 };                         \
    err_t cpu_init_rc_##var = cpu_init(&(var))

err_t load_program (operational_data_t * const op_data, cpu_t* cpu);
err_t exec_stream  (cpu_t* cpu);
err_t load_op_data (operational_data_t * const op_data, const char* const IN_FILE);

#endif
