IN
POPR x0
CALL :fact
OUT
HLT

:fact
    PUSHR x0
    PUSH 1
    JBE :base

    PUSHR x0
    PUSH 20
    JA :overflow

    PUSHR x0

    PUSHR x0
    PUSH 1
    SUB
    POPR x0

    CALL :fact

    MUL
    RET

:base
    PUSH 1
    RET

:overflow
    PUSH 0
    RET
