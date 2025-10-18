#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "executor_types.h"
#include "instruction_handlers/instruction_handlers.h"

#include "../../libs/instruction_set/instruction_set.h"

typedef err_t (*instruction_handler_t)(cpu_t * const cpu, const cell64_t * const args,
                                       const size_t argc);

err_t cpu_init    (cpu_t* cpu);
void  cpu_destroy (cpu_t* cpu);

err_t load_program (operational_data_t * const op_data, cpu_t* cpu);
err_t exec_stream  (cpu_t* cpu, logging_level level);
err_t load_op_data (operational_data_t * const op_data, const char* const IN_FILE);

#endif
