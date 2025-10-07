#include "instruction_set.h"

const uint8_t INSTRUCTION_BINARY_MAGIC[INSTRUCTION_BINARY_MAGIC_LEN] =
{
    'T', 'A', 'S', 'M'
};

static const instruction_t INSTRUCTIONS[INSTRUCTION_TABLE_CAPACITY] =
{
#define INSTRUCTION_INIT(symbol, label, args, opcode) \
    [symbol] = { .name = label, .id = symbol, .expected_args = (size_t)(args) },
    INSTRUCTION_LIST(INSTRUCTION_INIT)
#undef INSTRUCTION_INIT
};

static int instruction_id_is_valid(const instruction_set id)
{
    return id >= 0 && id < INSTRUCTION_TABLE_CAPACITY;
}

const instruction_t* instruction_get(const instruction_set id)
{
    if (!instruction_id_is_valid(id)) return NULL;
    const instruction_t* meta = &INSTRUCTIONS[id];
    return (meta->name != NULL) ? meta : NULL;
}

const instruction_t* instruction_table(void)
{
    return INSTRUCTIONS;
}

size_t instruction_table_size(void)
{
    return INSTRUCTION_TABLE_CAPACITY;
}

instruction_set_version_t instruction_set_version(void)
{
    instruction_set_version_t version =
    {
        .major = INSTRUCTION_SET_VERSION_MAJOR,
        .minor = INSTRUCTION_SET_VERSION_MINOR
    };

    return version;
}

unsigned int instruction_set_version_code(void)
{
    return (INSTRUCTION_SET_VERSION_MAJOR << 16) | INSTRUCTION_SET_VERSION_MINOR;
}

instruction_set map_instruction(const char* str)
{
    if (!str) return UNDEF;
#define INSTRUCTION_TRY(symbol, label, args, opcode) \
    if (strcmp(label, str) == 0) return symbol;
    INSTRUCTION_LIST(INSTRUCTION_TRY)
#undef INSTRUCTION_TRY
    return UNDEF;
}

size_t expect_arg(const instruction_set instruction)
{
    const instruction_t* meta = instruction_get(instruction);
    if (!meta) return 0;

    return (size_t)meta->expected_args;
}
