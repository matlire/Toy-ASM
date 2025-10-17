PUSH 5
POPR x8 ; radius

PUSH 16
POPR x0 ; height

PUSH 64
POPR x10 ; width

PUSHR x10
PUSH 2
DIV
POPR x1 ; x0, width / 2

PUSHR x0
PUSH 2
DIV
POPR x2 ; y0, height / 2

PUSH 0
POPR x3 ; row
PUSH 0
POPR x4 ; col

PUSH 0
POPR x5 ; vram address

CALL :loop
DRAW
DUMP
HLT

:loop
    ; Calculate x
    PUSHR x4
    PUSHR x1
    SUB
    POPR x6
    PUSHR x6
    PUSHR x6
    CALL :mod
    POPR x6 ; x

    ;Calculate y
    PUSHR x3
    PUSHR x2
    SUB 
    POPR x7
    PUSHR x7
    PUSHR x7
    CALL :mod
    POPR x7 ; y

    ; Check if x^2 + y^2 < r^2
    PUSHR x6
    SQ
    PUSHR x7
    SQ
    ADD
    PUSHR x8
    SQ
    JAE :not_in_r
    PUSHR x5
    POPR x9
    PUSH '#'
    POPVM [x9]
    :not_in_r

    ; Increment vram index
    PUSHR x5
    PUSH 1
    ADD
    POPR x5

    ; Increment column
    PUSHR x4
    PUSHR x10
    PUSH 1
    SUB
    JB :inc_col
    PUSH -1
    POPR x4

    ; Increment row
    PUSHR x3
    PUSH 1
    ADD
    POPR x3

    :inc_col
    PUSHR x4
    PUSH 1
    ADD
    POPR x4
    
    PUSHR x0
    PUSHR x3
    JA :loop
    RET

:mod
    PUSH 0
    JAE :mod_ready
    PUSH -1
    MUL
    :mod_ready
    RET
