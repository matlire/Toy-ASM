IN
POPR x0
PUSH 1
OUT
PUSH 1
OUT
CALL :fib
HLT

:fib
    PUSH 1
    POPR x1
    PUSH 1
    POPR x2
    PUSH 2
    POPR x3
    CALL :fib_loop
    RET

:fib_loop
    PUSHR x0
    PUSHR x3
    JA :fib_calc
    RET

    :fib_calc
    PUSHR x1
    PUSHR x2
    ADD
    PUSHR x2
    POPR x1
    POPR x2
    PUSHR x2
    OUT
    PUSH 1
    PUSHR x3
    ADD
    POPR x3
    CALL :fib_loop
    
