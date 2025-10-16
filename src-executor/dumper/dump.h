#ifndef EXECUTOR_DUMP_H
#define EXECUTOR_DUMP_H

#include "../executor/executor_types.h"

/*
 * Debugging helpers for the executor. These routines centralise the
 * presentation logic so the runtime can emit rich diagnostics without
 * cluttering the main execution code.
 */

void cpu_dump_registers(const cpu_t* cpu, logging_level level);
void cpu_dump_stack(const cpu_t* cpu, logging_level level);
void cpu_dump_code_window(const cpu_t* cpu,
                          size_t start_pc,
                          size_t max_bytes,
                          logging_level level);
void cpu_dump_state(const cpu_t* cpu, logging_level level);
void cpu_dump_step(const cpu_t* cpu,
                   size_t pc_before,
                   instruction_set opcode,
                   const long* args,
                   size_t arg_count,
                   logging_level level);
void cpu_dump_final_state(const cpu_t* cpu,
                          err_t rc,
                          logging_level level);
void cpu_dump_binary_header(const instruction_binary_header_t* header,
                            logging_level level);

#endif
