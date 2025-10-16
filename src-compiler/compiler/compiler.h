#ifndef COMPILER_H
#define COMPILER_H

#include "../../libs/logging/logging.h"
#include "../../libs/stack/stack.h"
#include "../../libs/instruction_set/instruction_set.h"

#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "../../libs/io/io.h"

#define begin do {
#define end   } while (0)

#define ASM_INITIAL_LABEL_CAPACITY 4

typedef struct
{
    const char* name;
    size_t      name_len;
    size_t      offset;
} asm_label_t;

typedef enum
{
    LABEL_PARSE_OK             = 0,
    LABEL_PARSE_ERR_BAD_PREFIX = 1,
    LABEL_PARSE_ERR_EMPTY      = 2,
    LABEL_PARSE_ERR_INVALID    = 3,
} label_parse_status_t;

typedef struct
{
    const char* name;
    size_t      length;
} label_token_t;

typedef struct
{
    char*  source;
    size_t source_size;

    asm_label_t* labels;
    size_t       label_count;
    size_t       label_capacity;
} asm_t;

err_t  load_op_data    (operational_data_t * const op_data,
                        const char * const IN_FILE, const char * const OUT_FILE);
size_t parse_file      (operational_data_t * const op_data);
size_t gen_write_header(operational_data_t * const op_data, instruction_binary_header_t* header);
size_t asm_first_pass  (asm_t* as);
size_t asm_second_pass (asm_t* as, FILE* out);
size_t update_header   (operational_data_t * const op_data,
                        instruction_binary_header_t* header, const size_t body_written);

err_t  asm_init       (asm_t* as, char* source, size_t source_size);
void   asm_destroy    (asm_t* as);

#endif
