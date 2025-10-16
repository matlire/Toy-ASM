IN
POPR x0
CALL :fact
OUT
HLT

:fact
    PUSHR x0
    PUSH 1
    DUMP
    JBE :base

    PUSHR x0

    PUSHR x0
    PUSH 1
    SUB
    POPR x0

    CALL :fact

    DUMP

    MUL
    RET

:base
    PUSH 1
    RET

