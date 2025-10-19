PUSH 32.0
FPOPR fx0        ; r

PUSH 64
POPR x0          ; height
PUSH 256
POPR x1          ; width

; centers
PUSHR x1
PUSH 2
DIV
POPR x6          ; cx = width / 2

PUSHR x0
PUSH 2
DIV
POPR x2          ; cy = height / 2

; aspect ratio correction
PUSH 2.6
FPOPR fx1       ; a

; init iterators, vram ptr
PUSH 0
POPR x3          ; row
PUSH 0
POPR x4          ; col
PUSH 0
POPR x5          ; vram ptr

CALL :loop
DRAW
HLT

:loop
    ; dx = float(col - cx)
    PUSHR x4
    PUSHR x6
    SUB
    ITOF
    FPOPR fx2

    ; dy = float(row - cy)
    PUSHR x3
    PUSHR x2
    SUB
    ITOF
    FPOPR fx3

    ; dist^2 = dx^2 + (AY*dy)^2
    FPUSHR fx2
    FSQ                          ; dx^2
    FPUSHR fx3
    FPUSHR fx1
    FMUL                         ; AY*dy
    FSQ                          ; (AY*dy)^2
    FADD                         ; dist^2

    FPUSHR fx0
    FSQ                          ; r^2

    FSUB                         ; dist2 - r2
    FLOOR
    FTOI
    PUSH 0
    JAE :not_in_r

    PUSHR x5
    POPR x7
    PUSH '#'
    POPVM [x7]

:not_in_r
    ; vram++
    PUSHR x5
    PUSH 1
    ADD
    POPR x5

    ; col++
    PUSHR x4
    PUSHR x1
    PUSH 1
    SUB
    JB :inc_col
    PUSH -1
    POPR x4

    ; row++
    PUSHR x3
    PUSH 1
    ADD
    POPR x3

:inc_col
    PUSHR x4
    PUSH 1
    ADD
    POPR x4

    ; loop while row < height
    PUSHR x0
    PUSHR x3
    JA :loop
    RET

