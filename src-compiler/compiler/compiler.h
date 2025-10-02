#ifndef COMPILER_H
#define COMPILER_H

#include "../../libs/logging/logging.h"
#include "../../libs/stack/stack.h"
#include "../../libs/instructions_set/instructions_set.h"

#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "../../libs/io/io.h"

size_t parse_file   (operational_data_t * const op_data);
size_t parse_line   (const char* in, char * const out);
err_t  load_op_data (operational_data_t * const op_data,
                     const char * const IN_FILE, const char * const OUT_FILE);

#endif
