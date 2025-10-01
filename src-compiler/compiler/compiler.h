#ifndef COMPILER_H
#define COMPILER_H

#include "../../libs/instructions_set/instructions_set.h"

#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "../../libs/io/io.h"

size_t parse_file (operational_data_t * const op_data);
size_t parse_line (const char* in, char * const out);

#endif
