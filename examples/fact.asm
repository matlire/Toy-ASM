IN
CALL :fact
OUT
HLT

:fact
    POPR x0
    PUSHR x0
    PUSH 1
    DUMP
    JBE :base

    PUSHR x0

    PUSHR x0
    PUSH 1
    SUB

    CALL :fact

    DUMP

    MUL
    RET

:base
    PUSH 1
    RET

