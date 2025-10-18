#include "instruction_handlers.h"

#include <math.h>

#include <inttypes.h>

static inline int64_t  g_ci64(cell64_t c) { return c.i64; }
static inline uint64_t g_cu64(cell64_t c) { return c.u64; }
static inline double   g_cf64(cell64_t c) { return c.f64; }

static inline cell64_t s_ci64(int64_t v)  { cell64_t c; c.i64=v; return c; }
static inline cell64_t s_cu64(uint64_t v) { cell64_t c; c.u64=v; return c; }
static inline cell64_t s_cf64(double v)   { cell64_t c; c.f64=v; return c; }

static int ensure_ir_index(size_t* out_index, const cell64_t* args, size_t argc)
{
    if (!out_index || !args || argc == 0) return 0;
    size_t idx = (size_t)g_ci64(args[0]);
    if (idx >= CPU_IR_COUNT) return 0;
    *out_index = idx;
    return 1;
}

static int ensure_fr_index(size_t* out_index, const cell64_t* args, size_t argc)
{
    if (!out_index || !args || argc == 0) return 0;
    size_t idx = (size_t)g_cf64(args[0]);
    if (idx >= CPU_FR_COUNT) return 0;
    *out_index = (size_t)idx;
    return 1;
}

err_t exec_NOP(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)cpu; (void)args; (void)argc;
    return OK;
}

err_t exec_HLT(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    if (!cpu) return ERR_BAD_ARG;

    cpu->pc = cpu->code_size;
    return OK;
}

err_t exec_PUSH(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    if (argc < 1 || !args) return ERR_BAD_ARG;
    return stack_push(cpu->code_stack, &args[0]);
}

err_t exec_FPUSH(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    if (argc < 1 || !args) return ERR_BAD_ARG;
    return stack_push(cpu->code_stack, &args[0]);
}

err_t exec_POP(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cell64_t discarded = { 0 };
    return stack_pop(cpu->code_stack, &discarded);
}

err_t exec_FPOP(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cell64_t discarded = { 0 };
    return stack_pop(cpu->code_stack, &discarded);
}

err_t exec_OUT(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cell64_t value = { 0 };
    err_t rc       = stack_pop(cpu->code_stack, &value);
    if (rc == OK) printf("%" PRId64 "\n", g_ci64(value));
    return rc;
}

err_t exec_FOUT(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cell64_t value = { 0 };
    err_t rc       = stack_pop(cpu->code_stack, &value);
    if (rc == OK) printf("%lf\n", g_cf64(value));
    return rc;
}

#define DEF_UNOP_I64(NAME, EXPR)                                              \
err_t exec_##NAME(cpu_t* cpu, const cell64_t* args, const size_t argc) {      \
    (void)args; (void)argc;                                                   \
    if (!cpu) return ERR_BAD_ARG;                                             \
    cell64_t value = { 0 };                                                   \
    err_t rc = stack_pop(cpu->code_stack, &value);                            \
    if (rc != OK) return rc;                                                  \
    cell64_t out = { 0 };                                                     \
    out.i64 = (EXPR);                                                         \
    return stack_push(cpu->code_stack, &out);                                 \
}

#define DEF_UNOP_F64(NAME, EXPR)                                              \
err_t exec_##NAME(cpu_t* cpu, const cell64_t* args, const size_t argc) {      \
    (void)args; (void)argc;                                                   \
    if (!cpu) return ERR_BAD_ARG;                                             \
    cell64_t value = { 0 };                                                   \
    err_t rc = stack_pop(cpu->code_stack, &value);                            \
    if (rc != OK) return rc;                                                  \
    cell64_t out = { 0 };                                                     \
    out.f64 = (EXPR);                                                         \
    return stack_push(cpu->code_stack, &out);                                 \
}

#define DEF_BINOP_I64(NAME, OP, DIV0)                                         \
err_t exec_##NAME(cpu_t* cpu, const cell64_t* args, const size_t argc) {      \
    (void)args; (void)argc;                                                   \
    if (!cpu) return ERR_BAD_ARG;                                             \
    cell64_t rhs = { 0 }, lhs = { 0 };                                        \
    if (exec_pop_operands(cpu, &lhs, &rhs) != OK) return ERR_CORRUPT;         \
    if (DIV0 && rhs.i64 == 0) return ERR_BAD_ARG;                             \
    cell64_t out = { 0 };                                                     \
    out.i64 = lhs.i64 OP rhs.i64;                                             \
    return stack_push(cpu->code_stack, &out);                                 \
}

#define DEF_BINOP_F64(NAME, OP, DIV0)                                         \
err_t exec_##NAME(cpu_t* cpu, const cell64_t* args, const size_t argc) {      \
    (void)args; (void)argc;                                                   \
    if (!cpu) return ERR_BAD_ARG;                                             \
    cell64_t rhs = { 0 }, lhs = { 0 };                                        \
    if (exec_pop_operands(cpu, &lhs, &rhs) != OK) return ERR_CORRUPT;         \
    if (DIV0 && rhs.f64 == 0) return ERR_BAD_ARG;                             \
    cell64_t out = { 0 };                                                     \
    out.f64 = lhs.f64 OP rhs.f64;                                             \
    return stack_push(cpu->code_stack, &out);                                 \
}

DEF_BINOP_I64(ADD, +, 0);
DEF_BINOP_I64(SUB, -, 0);
DEF_BINOP_I64(MUL, *, 0);
DEF_BINOP_I64(DIV, /, 1);
DEF_UNOP_I64(SQRT, sqrt(value.i64));
DEF_UNOP_I64(SQ,   value.i64 * value.i64);

DEF_BINOP_F64(FADD, +, 0);
DEF_BINOP_F64(FSUB, -, 0);
DEF_BINOP_F64(FMUL, *, 0);
DEF_BINOP_F64(FDIV, /, 1);
DEF_UNOP_F64(FSQRT, sqrt(value.f64));
DEF_UNOP_F64(FSQ,   value.f64 * value.f64);

DEF_UNOP_F64(FLOOR, floor(value.f64));
DEF_UNOP_F64(CEIL,  ceil(value.f64));
DEF_UNOP_F64(ROUND, round(value.f64));
DEF_UNOP_F64(ITOF,  (f64_t)(value.i64));
DEF_UNOP_I64(FTOI,  (i64_t)(floor(value.f64)));

err_t exec_PUSHR(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    cell64_t value = s_ci64(cpu->x[reg_index].value.value);
    return stack_push(cpu->code_stack, &value);
}

err_t exec_FPUSHR(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    cell64_t value = s_cf64(cpu->fx[reg_index].value.value);
    return stack_push(cpu->code_stack, &value);
}

err_t exec_POPR(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    cell64_t value = { 0 };
    err_t rc       = stack_pop(cpu->code_stack, &value);
    if (rc != OK) return rc;

    cpu->x[reg_index].value.value = g_ci64(value);

    return OK;
}

err_t exec_FPOPR(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    cell64_t value = { 0 };
    err_t rc       = stack_pop(cpu->code_stack, &value);
    if (rc != OK) return rc;

    cpu->fx[reg_index].value.value = g_cf64(value);

    return OK;
}

err_t exec_IN(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    i64_t value = 0;
    printf("Waiting for i64 input: ");
    if (scanf("%" PRId64, &value) != 1) return ERR_BAD_ARG;

    cell64_t v = s_ci64(value);
    return stack_push(cpu->code_stack, &v);
}

err_t exec_FIN(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    f64_t value = 0;
    printf("Waiting for f64 input: ");
    if (scanf("%lf", &value) != 1) return ERR_BAD_ARG;

    cell64_t v = s_cf64(value);
    return stack_push(cpu->code_stack, &v);
}

err_t exec_TOPOUT(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cell64_t value = { 0 };
    err_t rc       = stack_top(cpu->code_stack, &value);
    if (rc != OK) return rc;

    printf("%" PRId64 "\n", g_ci64(value));
    return OK;
}

err_t exec_FTOPOUT(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cell64_t value = { 0 };
    err_t rc       = stack_top(cpu->code_stack, &value);
    if (rc != OK) return rc;

    printf("%lf\n", g_cf64(value));
    return OK;
}

err_t exec_JMP(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)argc;

    cpu->pc = (size_t)g_ci64(args[0]);

    return OK;
}

err_t exec_CALL(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)argc;

    cell64_t retpc = s_ci64((i64_t)cpu->pc);

    err_t rc = stack_push(cpu->ret_stack, &retpc);
    if (rc != OK) return rc;
    
    cpu->pc = (size_t)g_ci64(args[0]);

    return OK;}

err_t exec_RET(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;
    
    cell64_t r = { 0 };
    err_t rc   = stack_pop(cpu->ret_stack, &r);
    if (rc != OK) return rc;
    
    cpu->pc = (size_t)g_ci64(r);

    return OK;
}

err_t exec_PUSHM(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    size_t addr = cpu->x[reg_index].value.value;
    if (addr  >= RAM_SIZE) return ERR_BAD_ARG;
 
    return stack_push(cpu->code_stack, &cpu->ram[addr]); 
}

err_t exec_POPM(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    size_t addr = cpu->x[reg_index].value.value;
    if (addr  >= RAM_SIZE) return ERR_BAD_ARG;
    
    cell64_t value = { 0 };
    err_t rc       = stack_pop(cpu->code_stack, &value);
    if (rc != OK) return ERR_CORRUPT;

    cpu->ram[addr] = value;
    
    return rc;
}

err_t exec_PUSHVM(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    size_t addr = cpu->x[reg_index].value.value;
    if (addr >= VRAM_SIZE) return ERR_BAD_ARG;
    
    cell64_t value = s_ci64((i64_t)(unsigned char)cpu->vram[addr]);
    return stack_push(cpu->code_stack, &value);
}

err_t exec_POPVM(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    size_t reg_index = 0;
    if (!ensure_ir_index(&reg_index, args, argc)) return ERR_BAD_ARG;

    size_t addr = cpu->x[reg_index].value.value;
    if (addr  >= VRAM_SIZE) return ERR_BAD_ARG;

    cell64_t value = {0};
    err_t rc       = stack_pop(cpu->code_stack, &value); 
    if (rc != OK) return ERR_CORRUPT;

    cpu->vram[addr] = (char)(g_ci64(value) & 0xFF);
    
    return rc;
}

err_t exec_DRAW(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    clear();

    for (size_t row = 0; row < SCREEN_HEIGHT; ++row)
    {
        char   line[SCREEN_WIDTH + 1] = { 0 };
        size_t len                    = 0;

        for (size_t col = 0; col < SCREEN_WIDTH; ++col)
        {
            const size_t idx = row * SCREEN_WIDTH + col;
            const char    ch = cpu->vram[idx];

            if (len + 2 >= sizeof(line)) break;

            line[len++] = ch;
        }

        if (len > 0)
            line[len - 1] = '\0';
        else
            line[0] = '\0';

        printf("%s\n", line);
    }

    return OK;
}

err_t exec_CLEANVM(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    for (size_t i = 0; i < VRAM_SIZE; i++)
    {
        cpu->vram[i] = ' ';
    }

    return OK;
}

err_t exec_DUMP(cpu_t * const cpu, const cell64_t * const args, const size_t argc)
{
    (void)args;
    (void)argc;

    cpu_dump_state(cpu, INFO);

    return OK;
}

#define DEFINE_COND_JUMP_FUNC(name, op)                                      \
    err_t exec_##name(cpu_t* cpu, const cell64_t* args, size_t arg_count)    \
    {                                                                        \
        if (!(cpu)) return ERR_BAD_ARG;                                      \
        if (!(args) || (arg_count) < 1) return ERR_BAD_ARG;                  \
                                                                             \
        cell64_t lhs = { 0 };                                                \
        cell64_t rhs = { 0 };                                                \
                                                                             \
        err_t rc = exec_pop_operands((cpu), &lhs, &rhs);                     \
        if (rc != OK) return rc;                                             \
                                                                             \
        if ((g_ci64(lhs)) op (g_ci64(rhs)))                                  \
            return exec_JMP((cpu), (args), (arg_count));                     \
                                                                             \
        return OK;                                                           \
    }                                                                        \

DEFINE_COND_JUMP_FUNC(JB,  <);
DEFINE_COND_JUMP_FUNC(JBE, <=);
DEFINE_COND_JUMP_FUNC(JA,  >);
DEFINE_COND_JUMP_FUNC(JAE, >=);
DEFINE_COND_JUMP_FUNC(JE,  ==);
DEFINE_COND_JUMP_FUNC(JNE, !=);

