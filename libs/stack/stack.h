#ifdef __DEBUG__
  #define IFDEBUG(...) __VA_ARGS__
#else
  #define IFDEBUG(...)
#endif

#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../logging/logging.h"

#ifdef LOGGING_H
  #define IFLOG(level, fmt, ...)  log_printf((level), (fmt), ##__VA_ARGS__)
#else
  #include <stdio.h>
  #define IFLOG(level, fmt, ...)  printf((fmt), ##__VA_ARGS__)
#endif

#define INITIAL_CAPACITY 4
#define STR_CAT_MAX_SIZE 4096

#define STACK_CANARY ((long long)0xB333DEDDEDAEBALL)

typedef struct
{
    const char* name;
    IFDEBUG(
        const char* func;
        const char* file;
        int   line;
    )
} stack_info_t;

typedef struct
{
    const char* elem_name;

    size_t elem_size;
    size_t elem_align;
    size_t elem_stride;
} element_info_t;

typedef int (*stack_sprint_fn) (char* dst, size_t dstsz, const void* elem);
typedef int (*stack_print_fn)  (FILE* file, const void* elem);

typedef enum 
{
    OK          = 0,
    ERR_BAD_ARG = 1,
    ERR_CORRUPT = 2,
    ERR_ALLOC   = 3,
} err_t;

extern const char * const err_msgs[];
const char* err_str(const err_t e);

typedef size_t stack_id;

err_t stack_ctor(stack_id* stack, element_info_t info,
                 const stack_print_fn printer, const stack_sprint_fn sprinter,
                 const stack_info_t stack_info);
err_t stack_dtor(stack_id st);

err_t stack_push(stack_id stack, const void* elem);
err_t stack_pop (stack_id stack, void* elem);

err_t stack_print(const stack_id stack);

err_t stack_dump (logging_level level, const stack_id stack, err_t code, const char* comment);
#define STACK_DUMP(level, st, code, comment) \
    stack_dump((level), (st), (code), (comment))

err_t stack_verify(const stack_id stack);

#define ELEMENT_INFO_INIT(T)    ((element_info_t) { .elem_name = #T, .elem_size = sizeof(T),     \
                                                    .elem_align = alignof(T), .elem_stride = 0 })

#define STACK_INFO_INIT(m_name) ((stack_info_t) { .name = #m_name,                        \
                                                   IFDEBUG(.func = __PRETTY_FUNCTION__, \
                                                  .file = __FILE__, .line = __LINE__) })

#define STACK_INIT(m_name, T)                                                  \
    stack_id m_name = 0;                                                       \
    err_t stack_init_rc_##m_name = stack_ctor(&(m_name), ELEMENT_INFO_INIT(T), \
                                              print_##T, sprint_##T,           \
                                              STACK_INFO_INIT(m_name))

#define DEFINE_STACK_PRINTER_SIMPLE(T, FMT)                                     \
    static int print_##T(FILE* __OUT__, const void* __PTR)                      \
    {                                                                           \
        return fprintf(__OUT__, FMT, *(const T*)__PTR);                         \
    }                                                                           \
    static int sprint_##T(char* __dst, size_t __dstsz, const void* __PTR)       \
    {                                                                           \
        size_t __len = (__dst && __dstsz) ? strnlen(__dst, __dstsz) : 0;        \
        if (__len >= __dstsz) return 0;                                         \
        return snprintf(__dst + __len, __dstsz - __len,                         \
                        FMT, *(const T*)__PTR);                                 \
    }

#define DEFINE_STACK_PRINTER(T, BODY)                       \
    static int print_##T(FILE* __OUT__, const void* __VP)   \
    {                                                       \
        const T* __PTR = (const T*)__VP;                    \
        BODY;                                               \
    }

#define DEFINE_STACK_SPRINTER(T, BODY)                                        \
    static int sprint_##T(char* __dst, size_t __dstsz, const void* __VP)      \
    {                                                                         \
        const T* __PTR = (const T*)__VP;                                      \
        size_t __len = (__dst && __dstsz) ? strnlen(__dst, __dstsz) : 0;      \
        if (__len >= __dstsz) return 0;                                       \
        BODY;                                                                 \
    }

#define STACK_PUSH_T(S, T, VAL)          \
    stack_push((S), &(T){ (VAL) })

#define STACK_PUSH_VAR(S, VAR)           \
    stack_push((S), &(VAR))

#define STACK_PUSH_S(S, T, ...)          \
    stack_push((S), &(T){ __VA_ARGS__ })

#define STACK_POP_T(S, T, m_name)             \
    T m_name; (void)stack_pop((S), &(m_name))

#define STACK_PUSH(S, V)                                                   \
    _Generic((V),                                                          \
        char:                 stack_push((S), &(char){(V)}),               \
        signed char:          stack_push((S), &(signed char){(V)}),        \
        unsigned char:        stack_push((S), &(unsigned char){(V)}),      \
        short:                stack_push((S), &(short){(V)}),              \
        unsigned short:       stack_push((S), &(unsigned short){(V)}),     \
        int:                  stack_push((S), &(int){(V)}),                \
        unsigned:             stack_push((S), &(unsigned){(V)}),           \
        long:                 stack_push((S), &(long){(V)}),               \
        unsigned long:        stack_push((S), &(unsigned long){(V)}),      \
        long long:            stack_push((S), &(long long){(V)}),          \
        unsigned long long:   stack_push((S), &(unsigned long long){(V)}), \
        float:                stack_push((S), &(float){(V)}),              \
        double:               stack_push((S), &(double){(V)}),             \
        long double:          stack_push((S), &(long double){(V)})         \
    )

#define STACK_POP(S, VAR)        \
    stack_pop((S), &(VAR))

#define STACK_CHECK(level, cond, stack, errcode, fmt, ...)      \
    if (!CHECK((level), (cond), (fmt), ##__VA_ARGS__)) {        \
        char buf[STR_CAT_MAX_SIZE] = {  };                      \
        (void)snprintf(buf, sizeof(buf), (fmt), ##__VA_ARGS__); \
        stack_dump((level), (stack), (errcode), (buf));         \
        return (errcode);                                       \
    }

#define STACK_ERR_CHECK(level, cond, stack, errcode, fmt, ...)      \
    do {                                                            \
        int sc_res = CHECK((level), (cond), (fmt), ##__VA_ARGS__);  \
        if (!sc_res) {                                              \
            char buf[STR_CAT_MAX_SIZE] = {  };                      \
            (void)snprintf(buf, sizeof(buf), (fmt), ##__VA_ARGS__); \
            stack_dump((level), (stack), (errcode), (buf));         \
            printf("%s", err_str(errcode));                         \
            exit(1);                                                \
        }                                                           \
    } while (0)

#define STACK_VERIFY(st)                   \
    do {                                   \
        err_t sv_res = stack_verify((st)); \
        if (sv_res != OK)                  \
        {                                  \
            printf("%s", err_str(sv_res)); \
            exit(1);                       \
        }                                  \
    } while (0)
    

#endif
