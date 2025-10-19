# Toy-ASM
Toy ASM implementation

Own TASM and VM for it.

---

## Quickstart

### Building

```bash
./build.sh
```

### Compiler

```bash
./dist/compiler.out 
```

**FLAGS:**

```
--infile  in.asm
--outfile out.bin
```

### Executor

```bash
./dist/executor.out
```

**FLAGS:**

```
--infile in.bin
```

---

## Visual2tasm

Also you can translate image/video to `.asm` that can be compiled and shown on the VM. The helper script **`gen/visual2tasm.py`** rasterizes content into VRAM writes.

### Image

```bash
python gen/visual2tasm.py --in "examples/media/1.jpg" --out "examples/meowx1.asm" \
                        --threshold 140 --mode levels                             \
                        --ramp "@#*+=-:. " --gamma 1.2 --skip-off
```

### Video

```bash
python gen/visual2tasm.py --in "examples/media/1.mp4" --out "examples/meowx600.asm" \
                        --width 256 --height 64 --mode levels                       \
                        --ramp "@#*+=-:. " --gamma 1.2                              \
                        --emit-int --skip-off --no-comments                         \
                        --no-delta --repeat 1 --delay-iters 10
```

### Requirements

- Python 3.8+
- `numpy`, `opencv-python`

Install:

```bash
pip install opencv-python numpy
```

### Flags (summary)

**I/O & Screen**  
```
--in PATH (required)        : input image or video (.mp4 .mov .mkv .avi .webm .m4v)
--out PATH (default out.asm): output .asm file
--width  INT (default 256)
--height INT (default 64)   : final framebuffer size
```

**Tone Mapping**  
```
--mode {binary,levels} (default binary)

binary:
  --threshold INT (0..255, default 127) : fixed threshold (ignored if --otsu)
  --otsu                                : Otsu automatic threshold
  --on  CH (default '#')                : char for black (ON) pixels
  --off CH (default ' ')                : char for white (OFF) pixels

levels:
  --ramp STRING (default "@#*+=-:. ")   : darkest→lightest characters
  --gamma FLOAT (default 1.0)           : gamma correction (>1 → darker symbols)
  --invert                              : invert after threshold/normalize
```

**Emission & Compression**  
```
--emit-int               : always emit ASCII codes (PUSH 35). Safest.
--emit-char              : prefer char literals (PUSH '#'); fallback to int for ' and \
--skip-off               : on full frames, skip the lightest symbol writes (assumes CLEANVM)
--skip-char CH           : explicit character to skip (overrides --skip-off)
--no-clear               : don’t emit CLEANVM (layer on existing VRAM)
--no-comments            : suppress header/row comments
--unroll INT (default 3) : inline short write runs without CALL :f
```

**Video Sampling & Playback**  
```
--frame-step INT (default 1)       : keep every Nth source frame
--max-frames INT (default 0 = all) : cap total frames after sampling

Deterministic speed:
  --repeat INT (default 1)               : repeat each frame N times
  --fps-target FLOAT (default 0 = off)   : choose integer repeat to ~match FPS
  --delay-iters INT (default 0)          : busy-wait between frame calls

--loop    : loop playback (JMP :__start)
--no-delta: disable delta mode; emit full frames always
```

**Diagnostics**
```
--verbose : print backend, FPS, frame count, retry info to stderr
```
**Flag interactions**
```
If both --emit-int and --emit-char are set, --emit-int wins.
In levels mode, --threshold is parsed but unused (kept for CLI compatibility).
--fps-target computes repeat ≈ round((input_fps / frame_step) / fps_target).
```

---

## Instruction set

This ISA is a stack machine over 64‑bit cells. **v3 bytecode** stores only the opcode and arguments; the number of arguments is taken from metadata (not encoded). Below: mnemonic, argc (arguments count in bytecode), opcode (decimal), and behavior. Stack is marked with "**…**" symbol.

### Core / flow
| Mnemonic | argc | Op | Description |
|---|---:|---:|---|
| `NOP`    | 0 | 0  | No operation. |
| `HLT`    | 0 | 1  | Halt execution. |
| `JMP a`  | 1 | 16 | PC ← `a` (absolute offset). |
| `CALL a` | 1 | 7  | Push return address, jump to `a`. (**callee must preserve stack depth**) |
| `RET`    | 0 | 8  | Return to caller (verifies stack depth in strict mode). |

### Stack & I/O
| Mnemonic | argc | Op | Effect |
|---|---:|---:|---|
| `PUSH v`   | 1 | 2 | Push 64‑bit argument (int or float literal). |
| `POP`      | 0 | 3 | Pop and discard. |
| `OUT`      | 0 | 4 | Pop int and print. |
| `TOPOUT`   | 0 | 5 | Print top int (no pop). |
| `IN`       | 0 | 6 | Read int from stdin, push. |

### Integer ALU (i64)
| Mnemonic | argc | Op | Stack effect |
|---|---:|---:|---|
| `ADD`   | 0 | 10 | `… → a, b → a+b → …` |
| `SUB`   | 0 | 11 | `… → a, b → a-b → …`|
| `MUL`   | 0 | 12 | `… → a, b → a*b → …`|
| `DIV`   | 0 | 13 | `… → a, b → a/b → … `(error on `b=0`) |
| `SQRT`  | 0 | 14 | `… → a → ⌊√a⌋   → … `(int form)|
| `SQ`    | 0 | 15 | `… → a → a*a    → … `|

### Bitwise (integers)
| Mnemonic | argc | Op | Stack effect | Semantics |
|---|---:|---:|---|---|
| `NOT` | 0 | 42 | `… → a → ~a      → …` | bitwise NOT |
| `OR`  | 0 | 43 | `… → a, b → a\|b  → …` | |
| `AND` | 0 | 44 | `… → a, b → a&b  → …` | |
| `XOR` | 0 | 45 | `… → a, b → a^b  → …` | |
| `SHL` | 0 | 46 | `… → val, cnt → val << (cnt & 63) → …` | left shift (bit‑pattern) |
| `SHR` | 0 | 47 | `… → val, cnt → val >> (cnt & 63) → …` | **arithmetic** right shift (sign‑extend) |

> For shifts the VM pops **rhs (count)** then **lhs (value)**; shift count is masked with `& 63`.

### Branches (signed compare on i64; pops `rhs` then `lhs`)
| Mnemonic | argc | Op | Condition |
|---|---:|---:|---|
| `JB a`  | 1 | 17 | if `lhs <  rhs` jump `a` |
| `JBE a` | 1 | 18 | if `lhs <= rhs` jump `a` |
| `JA a`  | 1 | 19 | if `lhs >  rhs` jump `a` |
| `JAE a` | 1 | 20 | if `lhs >= rhs` jump `a` |
| `JE a`  | 1 | 21 | if `lhs == rhs` jump `a` |
| `JNE a` | 1 | 22 | if `lhs != rhs` jump `a` |

### Registers
| Mnemonic | argc | Op | Effect |
|---|---:|---:|---|
| `PUSHR xN` | 1 | 33 | Push contents of integer reg `xN`. |
| `POPR xN`  | 1 | 34 | Pop into integer reg `xN`. |
| `FPUSHR fxN`|1 | 76 | Push contents of float reg `fxN`. |
| `FPOPR fxN` |1 | 77 | Pop into float reg `fxN`. |

### RAM / VRAM
| Mnemonic | argc | Op | Effect |
|---|---:|---:|---|
| `PUSHM xN`  | 1 | 35 | Push `RAM[xN]` (address in integer reg). |
| `POPM  xN`  | 1 | 36 | Pop to  `RAM[xN]`. |
| `PUSHVM xN` | 1 | 37 | Push `VRAM[xN]` (byte). |
| `POPVM  xN` | 1 | 38 | Pop to  `VRAM[xN]` (byte). |
| `CLEANVM`   | 0 | 39 | Fill VRAM with spaces. |
| `DRAW`      | 0 | 9  | Render VRAM. |

### Floating ALU (f64)
| Mnemonic | argc | Op | Stack effect |
|---|---:|---:|---|
| `FADD`  | 0 | 64 | `… → a, b → a+b  → …` |
| `FSUB`  | 0 | 65 | `… → a, b → a-b  → …` |
| `FMUL`  | 0 | 66 | `… → a, b → a*b  → …` |
| `FDIV`  | 0 | 67 | `… → a, b → a/b  → …` (error on `b=0`) |
| `FSQRT` | 0 | 68 | `… → a → sqrt(a) → …` |
| `FSQ`   | 0 | 69 | `… → a → a*a     → …` |

### Floating I/O & rounding
| Mnemonic | argc | Op | Effect |
|---|---:|---:|---|
| `FIN`     | 0 | 70 | Read double, push. |
| `FOUT`    | 0 | 71 | Pop double, print. |
| `FTOPOUT` | 0 | 72 | Print top double (no pop). |
| `FLOOR`   | 0 | 80 | `… → a → floor(a) → …` |
| `CEIL`    | 0 | 81 | `… → a → ceil(a)  → …` |
| `ROUND`   | 0 | 82 | `… → a → round(a) → …` |

### Conversions
| Mnemonic | argc | Op | Effect |
|---|---:|---:|---|
| `ITOF` | 0 | 90 | Convert top int    → double (in place). |
| `FTOI` | 0 | 91 | Convert top double → int    (floors double). |

### Diagnostics
| Mnemonic | argc | Op | Effect |
|---|---:|---:|---|
| `DUMP` | 0 | 23 | Log CPU state. |

---

## Binary format

**Header (v3):**

```
offset  size  field
0x00    4     "TASM"
0x04    1     version_major (3)
0x05    1     version_minor (0)
0x06    2     padding (0)
0x08    8     code_size (bytes)
0x10          code bytes...
```

**Code section:**

```
[ 1 byte opcode ] [ 0..N arguments, each 8 bytes little-endian ]
```

- The VM finds `N` in the opcode metadata (`expected_args`).
- Arguments are 64-bit cells; ints are signed; floats are using doubles'.

Example (v3) for:
```asm
PUSH 10
PUSH 20
ADD
HLT
```
Code bytes:
```
02 0A 00 00 00 00 00 00 00
02 14 00 00 00 00 00 00 00
0A
01
```

---

## Introduction to compiler

The assembler is a classic **two-pass** encoder:

1. **Pass 0 (analysis):** tokenize lines, collect labels (`:label`) and their code offsets, validate mnemonics and operand forms.
2. **Pass 1 (emit):** write header, then for each instruction:
   - opcode byte
   - arguments (if any), each as an 8-byte little-endian cell  

Diagnostics go to the dumper: source line, resulting bytes, and offsets (when enabled).

**CLI**: `--infile`, `--outfile`

---

## Introduction to executor

The VM loads the binary, validates header and version, and executes loop:

- **Fetch**: read 1 byte opcode at `PC`.
- **Decode**: look up metadata (mnemonic, `expected_args`).
- **Fetch arguments**: read `expected_args * 8` bytes (if any).
- **Execute**: run `exec_<MNEMONIC>`

Key points:

- **PC modifies**: `JMP`, `CALL`, `RET`, and conditional jumps modify `PC`. Jumps are absolute byte offsets into code.
- **Call checks**: callee must preserve data-stack depth. `RET` verifies depth equals the saved value from `CALL` and errors on mismatch.
- **Bitwise/shift semantics**: bitwise ops act on the 64-bit pattern; `SHL` is a left shift; `SHR` is an arithmetic right shift (sign-extend). Shift counts are masked with `& 63`.
- **I/O**: `IN/OUT` for integers, `FIN/FOUT` for doubles.
- **Memory**: `PUSHM/POPM` read/write RAM via integer registers; `PUSHVM/POPVM` read/write VRAM bytes; `CLEANVM` clears; `DRAW` renders.

Errors (invalid opcode, incorrect arguments, div-by-zero, stack under/overflow, memory OOB, call-balance mismatch) stop execution with diagnostics.

