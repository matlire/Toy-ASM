#ifndef EXECUTOR_DUMP_H
#define EXECUTOR_DUMP_H

#include "../executor/executor_types.h"

#define DUMP_CODE_WINDOW_SIZE 64

void cpu_dump_registers     (const cpu_t * const cpu, logging_level level);
void cpu_dump_stack         (const cpu_t * const cpu, logging_level level);

void cpu_dump_code_window   (const cpu_t * const cpu,
                             size_t              start_pc,
                             size_t              max_bytes,
                             logging_level       level);

void cpu_dump_state         (const cpu_t * const cpu, logging_level level);

void cpu_dump_step          (const cpu_t * const cpu,
                             size_t              pc_before,
                             instruction_set     opcode,
                             const long*         args,
                             size_t              arg_count,
                             logging_level       level);

void cpu_dump_final_state   (const cpu_t * const cpu,
                             err_t               rc,
                             logging_level       level);

void cpu_dump_ram           (const cpu_t * const cpu, 
                             logging_level       level);

void cpu_dump_vram          (const cpu_t * const cpu, 
                             logging_level       level);

void cpu_dump_binary_header (const instruction_binary_header_t *   const header,
                             logging_level                               level);
#endif
