#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <ctype.h>
#include <string.h>
#include <math.h>

#include "../../libs/instructions_set/instructions_set.h"

#include "../../libs/logging/logging.h"

#include "../../libs/stack/stack.h"
#include "../../libs/io/io.h"

err_t exec_stream  (operational_data_t * const op_data);
err_t load_op_data (operational_data_t * const op_data, const char* const IN_FILE);

#endif
