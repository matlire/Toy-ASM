#ifndef DUMP_H
#define DUMP_H

#include "../../libs/logging/logging.h"
#include "../compiler/compiler.h"

void asm_dump_label_table(const asm_t* as, logging_level level);

void asm_dump_pass_line(const asm_t*         as,
                        int                  pass,
                        size_t               line_no,
                        size_t               offset_before,
                        const char*          raw_source,
                        const unsigned char* encoded,
                        size_t               encoded_len,
                        logging_level        level);

void asm_dump_pass_summary(const asm_t*  as,
                           int           pass,
                           size_t        total_bytes,
                           logging_level level);

void asm_dump_header(const instruction_binary_header_t* header,
                     logging_level level);

#endif
