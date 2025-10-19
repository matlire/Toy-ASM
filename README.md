# Toy-ASM
Toy ASM implementation

Own TASM and VM for it.

### Building

./build.sh

### Compiler

    ./dist/compiler.out 

    FLAGS:

        --infile  in.asm

        --outfile out.bin

### Executor

    ./dist/executor.out

    FLAGS:

        --infile in.bin
        
### Visual2tasm

Also you can translate image/video to .asm file that can be compiled and shown on VM. To do it you can use helper generation script.

Requirements:

- python3.8+

- numpy

- opencv

Installation: 

    pip install opencv-python numpy

Quick start:

Image:

    python gen/visual2tasm.py --in "examples/media/1.jpg" --out "examples/meowx1.asm" \
            --threshold 140 --mode levels                                             \
            --ramp "@#*+=-:. " --gamma 1.2 --skip-off

Video:

    python gen/visual2tasm.py --in "examples/media/1.mp4" --out "examples/meowx600.asm" \
            --width 256 --height 64 --mode levels --ramp "@#*+=-:. " --gamma 1.2 \
            --emit-int --skip-off --no-comments \
            --no-delta --repeat 1 --delay-iters 10

Flags:

    I/O & Screen
        --in PATH (required)        : input image or video (.mp4 .mov .mkv .avi .webm .m4v).
        --out PATH (default out.asm): output .asm file.

        --width  INT (default 256)
        --height INT (default 64)   : final framebuffer size.

    Tone Mapping
        --mode {binary,levels} (default binary).

        binary:
            --threshold INT (0..255, default 127): fixed threshold (ignored if --otsu).

            --otsu                  : Otsu automatic threshold.
            --on  CH (default #)    : char for black (ON) pixels.
            --off CH (default space): char for white (OFF) pixels.

        levels:
            --ramp STRING (default @#*+=-:. ): darkest→lightest characters.
            --gamma FLOAT (default 1.0)      : gamma correction (>1 biases to darker symbols).
            --invert                         : invert after threshold/normalize.

    Emission & Compression
        --emit-int    : always emit ASCII codes (PUSH 35). Safest.
        --emit-char   : prefer char literals (PUSH '#'), fallback to int for ' and \. Default behavior (no flag) = like --emit-char
        --skip-off    : on full frames, skip the lightest symbol writes (assumes CLEANVM). Ignored on delta frames (OFFs must overwrite old ONs).
        --skip-char CH: explicit character to skip (overrides --skip-off).
        --no-clear    : don’t emit CLEANVM (use when layering on existing VRAM).
        --no-comments : suppress header/row comments to shrink file size.

        --unroll INT (default 3): inline short write runs without calling :f.

    Video Sampling & Playback
        --frame-step INT (default 1)      : keep every Nth source frame.
        --max-frames INT (default 0 = all): cap total frames after sampling.

        Deterministic speed:
            --repeat INT (default 1)            : repeat each frame N times.
            --fps-target FLOAT (default 0 = off): estimate input FPS and pick an integer repeat so output ≈ target. Discrete and approximate.
            --delay-iters INT (default 0)       : busy‑wait between frame calls. CPU‑dependent.

        --loop    : loop playback (JMP :__start).
        --no-delta: disable delta mode; emit full frames for all frames.

    Diagnostics
        --verbose: print backend, FPS, frame count, and retry info to stderr.

    Flag interactions
        If both --emit-int and --emit-char are set, --emit-int wins.
        In levels mode, --threshold is parsed but unused (kept for CLI compatibility).
        --fps-target computes repeat ≈ round((input_fps / frame_step) / fps_target). If that’s not good enough, tune --repeat and/or --delay-iters directly.
