import argparse
import os
import sys
from typing import Optional, Tuple, List

import cv2
import numpy as np

VIDEO_EXTS = {".mp4", ".mov", ".mkv", ".avi", ".webm", ".m4v"}

def image_to_ascii_chars(gray:      np.ndarray,
                         mode:      str,
                         threshold: int,
                         use_otsu:  bool,
                         invert:    bool,
                         on_char:   str,
                         off_char:  str,
                         ramp:      str,
                         gamma:     float) -> np.ndarray:
    if mode == "binary":
        if use_otsu:
            _, bw = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        else:
            _, bw = cv2.threshold(gray, threshold, 255, cv2.THRESH_BINARY)
        if invert:
            bw = 255 - bw
        if len(on_char) != 1 or len(off_char) != 1:
            raise ValueError("--on and --off must be single characters")
        return np.where(bw == 0, on_char, off_char)
    elif mode == "levels":
        f = gray.astype(np.float32) / 255.0
        if invert:
            f = 1.0 - f
        if gamma != 1.0 and gamma > 0:
            f = np.clip(f, 0.0, 1.0) ** (1.0 / gamma)
        if not ramp or len(ramp) < 2:
            raise ValueError("--ramp must have at least 2 characters")
        L = len(ramp)
        idx = np.rint(f * (L - 1)).astype(np.int32)
        idx = np.clip(idx, 0, L - 1)
        lut = np.array(list(ramp), dtype='<U1')
        return lut[idx]
    else:
        raise ValueError("--mode must be 'binary' or 'levels'")

def emit_push_char(ch: str, emit_int: bool, emit_char: bool) -> str:
    if emit_int:
        return f"PUSH {ord(ch)}"
    if ch in ("'", "\\"):
        return f"PUSH {ord(ch)}"
    return f"PUSH '{ch}'"

def emit_header(no_comments: bool, header_lines: List[str]) -> List[str]:
    return ([] if no_comments else header_lines) + ([] if no_comments else [""])

def emit_common_subroutines(no_comments: bool) -> List[str]:
    lines: List[str] = []
    if not no_comments:
        lines += ["; common helpers"]
    lines += [
        ":f",
        "    :f_loop",
        "        PUSHR x1",
        "        PUSH 0",
        "        JBE :f_done",
        "        PUSHR x2",
        "        POPVM [x0]",
        "        PUSHR x0",
        "        PUSH 1",
        "        ADD",
        "        POPR x0",
        "        PUSHR x1",
        "        PUSH 1",
        "        SUB",
        "        POPR x1",
        "        JMP :f_loop",
        "    :f_done",
        "    RET",
        ""
    ]
    lines += [
        ":s",
        "    :s_loop",
        "        PUSHR x1",
        "        PUSH 0",
        "        JBE :s_done",
        "        PUSHR x0",
        "        PUSH 1",
        "        ADD",
        "        POPR x0",
        "        PUSHR x1",
        "        PUSH 1",
        "        SUB",
        "        POPR x1",
        "        JMP :s_loop",
        "    :s_done",
        "    RET",
        ""
    ]
    return lines

def emit_skip_add(run_len: int) -> List[str]:
    return ["PUSHR x0", f"PUSH {run_len}", "ADD", "POPR x0"]

def emit_write_run(ch:       str,  run_len:   int, unroll: int,
                   emit_int: bool, emit_char: bool) -> List[str]:
    if run_len <= unroll:
        out: List[str] = []
        push_line = emit_push_char(ch, emit_int, emit_char)
        for _ in range(run_len):
            out.append(push_line)
            out.append("POPVM [x0]")
            out += emit_skip_add(1)
        return out
    return [
        emit_push_char(ch, emit_int, emit_char),
        "POPR x2",
        f"PUSH {run_len}",
        "POPR x1",
        "CALL :f",
    ]

def emit_frame_full(char_img:    np.ndarray,
                    width:       int,
                    height:      int,
                    clear_vram:  bool,
                    skip_char:   Optional[str],
                    emit_int:    bool,
                    emit_char:   bool,
                    unroll:      int,
                    no_comments: bool) -> List[str]:
    lines: List[str] = []
    if clear_vram:
        lines.append("CLEANVM")
    lines += ["PUSH 0", "POPR x0"]
    for y in range(height):
        if not no_comments:
            lines.append(f"; row {y}")
        row = char_img[y]
        x = 0
        while x < width:
            ch = row[x].item() if hasattr(row[x], "item") else row[x]
            run_len = 1
            while (x + run_len) < width:
                nxt = row[x + run_len]
                nxt = nxt.item() if hasattr(nxt, "item") else nxt
                if nxt != ch:
                    break
                run_len += 1
            if skip_char is not None and ch == skip_char:
                lines += emit_skip_add(run_len)
            else:
                lines += emit_write_run(ch, run_len, unroll, emit_int, emit_char)
            x += run_len
        if not no_comments:
            lines.append("")
    lines.append("DRAW")
    return lines

def emit_frame_delta(prev_img:    np.ndarray,
                     cur_img:     np.ndarray,
                     width:       int,
                     height:      int,
                     emit_int:    bool,
                     emit_char:   bool,
                     unroll:      int,
                     no_comments: bool) -> List[str]:
    lines: List[str] = []
    lines += ["PUSH 0", "POPR x0"]
    for y in range(height):
        if not no_comments:
            lines.append(f"; row {y}")
        prev = prev_img[y]
        cur = cur_img[y]
        x = 0
        while x < width:
            u = x
            while u < width:
                if (cur[u] if isinstance(cur[u], str) else cur[u].item()) != \
                   (prev[u] if isinstance(prev[u], str) else prev[u].item()):
                    break
                u += 1
            if u > x:
                lines += emit_skip_add(u - x)
                x = u
                if x >= width:
                    break
            v = x
            while v < width:
                if (cur[v] if isinstance(cur[v], str) else cur[v].item()) == \
                   (prev[v] if isinstance(prev[v], str) else prev[v].item()):
                    break
                v += 1
            i = x
            while i < v:
                ch = cur[i].item() if hasattr(cur[i], "item") else cur[i]
                run_len = 1
                while i + run_len < v:
                    nxt = cur[i + run_len]
                    nxt = nxt.item() if hasattr(nxt, "item") else nxt
                    if nxt != ch:
                        break
                    run_len += 1
                lines += emit_write_run(ch, run_len, unroll, emit_int, emit_char)
                i += run_len
            x = v
        if not no_comments:
            lines.append("")
    lines.append("DRAW")
    return lines

def open_video_with_fallbacks(path: str, verbose: bool):
    cap = cv2.VideoCapture(path)
    if cap.isOpened():
        return cap, "default"
    try:
        cap = cv2.VideoCapture(path, cv2.CAP_FFMPEG)
        if cap.isOpened():
            return cap, "ffmpeg"
    except Exception:
        pass
    try:
        cap = cv2.VideoCapture(path, cv2.CAP_ANY)
        if cap.isOpened():
            return cap, "any"
    except Exception:
        pass
    return None, "failed"

def handle_image(args):
    img = cv2.imread(args.in_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise RuntimeError("Failed to load image (unsupported format?)")
    resized = cv2.resize(img, (args.width, args.height), interpolation=cv2.INTER_AREA)

    char_img = image_to_ascii_chars(resized, args.mode, args.threshold, args.otsu,
                                    args.invert, args.on_char, args.off_char,
                                    args.ramp, args.gamma)

    if args.skip_char is not None:
        eff_skip = args.skip_char
        if len(eff_skip) != 1:
            raise ValueError("--skip-char must be a single character")
    elif args.skip_off:
        eff_skip = args.off_char if args.mode == "binary" else (args.ramp[-1] if len(args.ramp) > 0 else " ")
    else:
        eff_skip = None

    lines: List[str] = []
    lines += emit_header(args.no_comments, [
        "; Auto-generated by visual2tasm.py (image)",
        f"; Source: {os.path.basename(args.in_path)}",
        f"; Screen: {args.width} x {args.height}  Mode: {args.mode}",
        (f"; Threshold: {'otsu' if args.otsu else args.threshold}  Invert: {args.invert}")
        if args.mode == "binary" else
        (f"; Ramp: {args.ramp}  Gamma: {args.gamma}  Invert: {args.invert}")
    ])
    lines.append("JMP :__main")
    lines += emit_common_subroutines(args.no_comments)
    lines.append(":__main")
    lines += emit_frame_full(char_img, args.width, args.height,
                             clear_vram=not args.no_clear,
                             skip_char=eff_skip,
                             emit_int=args.emit_int,
                             emit_char=args.emit_char or not args.emit_int,
                             unroll=args.unroll,
                             no_comments=args.no_comments)
    lines.append("HLT")

    out_dir = os.path.dirname(os.path.abspath(args.out_path))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"Wrote {args.out_path}")

def handle_video_singlefile(args):
    if not os.path.exists(args.in_path):
        raise RuntimeError(f"Input not found: {args.in_path}")

    cap, backend = open_video_with_fallbacks(args.in_path, args.verbose)
    if cap is None or not cap.isOpened():
        raise RuntimeError("Failed to open video (all backends)")

    input_fps = cap.get(cv2.CAP_PROP_FPS) or 0.0
    frame_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0)
    if args.verbose:
        print(f"[probe] backend={backend} fps={input_fps:.3f} frames={frame_count}", file=sys.stderr)

    frame_step = max(1, args.frame_step)
    idx = 0
    frames: List[np.ndarray] = []

    reads = 0
    while True:
        ret, frame = cap.read()
        reads += 1
        if not ret:
            break
        if idx % frame_step != 0:
            idx += 1
            continue
        if args.max_frames and len(frames) >= args.max_frames:
            break
        
        gray     = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        resized  = cv2.resize(gray, (args.width, args.height), interpolation=cv2.INTER_AREA)
        char_img = image_to_ascii_chars(resized, args.mode, args.threshold, args.otsu,
                                        args.invert, args.on_char, args.off_char,
                                        args.ramp, args.gamma)
        frames.append(char_img)
        idx += 1

    cap.release()

    if not frames:
        if args.verbose:
            print(f"[warn] first pass captured 0 frames; reads={reads}, retrying from start", file=sys.stderr)
        cap2, backend2 = open_video_with_fallbacks(args.in_path, args.verbose)
        if cap2 is None or not cap2.isOpened():
            raise RuntimeError("Failed to open video on retry")
        cap2.set(cv2.CAP_PROP_POS_FRAMES, 0)
        idx = 0
        while True:
            ret, frame = cap2.read()
            if not ret:
                break
            if idx % frame_step != 0:
                idx += 1
                continue
            gray     = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            resized  = cv2.resize(gray, (args.width, args.height), interpolation=cv2.INTER_AREA)
            char_img = image_to_ascii_chars(resized, args.mode, args.threshold, args.otsu,
                                            args.invert, args.on_char, args.off_char,
                                            args.ramp, args.gamma)
            frames.append(char_img)
            idx += 1
        cap2.release()

    if not frames:
        raise RuntimeError("No frames captured (check --frame-step / --max-frames and video codec)")

    repeat = max(1, int(args.repeat))
    if args.fps_target and args.fps_target > 0:
        eff_in = (input_fps or 30.0) / frame_step
        repeat = max(1, int(round(eff_in / args.fps_target)))

    if args.skip_char is not None:
        eff_skip = args.skip_char
        if len(eff_skip) != 1:
            raise ValueError("--skip-char must be a single character")
    elif args.skip_off:
        eff_skip = args.off_char if args.mode == "binary" else (args.ramp[-1] if len(args.ramp) > 0 else " ")
    else:
        eff_skip = None

    out_lines: List[str] = []
    out_lines += emit_header(args.no_comments, [
        "; Auto-generated by visual2tasm.py (video — single file)",
        f"; Source: {os.path.basename(args.in_path)}  Frames: {len(frames)}  InputFPS: {input_fps:.3f}  FrameStep: {frame_step}  Repeat: {repeat}  Backend: {backend}",
        f"; Screen: {args.width} x {args.height}  Mode: {args.mode}",
        (f"; Threshold: {'otsu' if args.otsu else args.threshold}  Invert: {args.invert}")
        if args.mode == "binary" else
        (f"; Ramp: {args.ramp}  Gamma: {args.gamma}  Invert: {args.invert}")
    ])
    out_lines.append("JMP :__start")
    out_lines += emit_common_subroutines(args.no_comments)

    if args.delay_iters > 0 and not args.no_comments:
        out_lines.append(f"; delay iters = {args.delay_iters}")
    if args.delay_iters > 0:
        out_lines += [
            ":delay",
            f"    PUSH {args.delay_iters}",
            "    POPR x8",
            ":delay_loop",
            "    PUSHR x8",
            "    PUSH 0",
            "    JBE :delay_done",
            "    PUSHR x8",
            "    PUSH 1",
            "    SUB",
            "    POPR x8",
            "    JMP :delay_loop",
            ":delay_done",
            "    RET",
            ""
        ]

    out_lines.append(":__start")
    for i in range(len(frames)):
        for _ in range(repeat):
            out_lines.append(f"    CALL :frame_{i:06d}")
            if args.delay_iters > 0:
                out_lines.append("    CALL :delay")
    out_lines.append("    JMP :__start" if args.loop else "    HLT")
    out_lines.append("")

    for i, img in enumerate(frames):
        if not args.no_comments:
            out_lines.append(f"; ------- frame {i} -------")
        out_lines.append(f":frame_{i:06d}")
        if i == 0 or args.no_delta:
            out_lines += ["    " + ln if ln and not ln.startswith(';') else ln
                          for ln in emit_frame_full(img, args.width, args.height,
                                                    clear_vram=(not args.no_clear),
                                                    skip_char=eff_skip,
                                                    emit_int=args.emit_int,
                                                    emit_char=args.emit_char or not args.emit_int,
                                                    unroll=args.unroll,
                                                    no_comments=args.no_comments)]
        else:
            prev = frames[i - 1]
            out_lines += ["    " + ln if ln and not ln.startswith(';') else ln
                          for ln in emit_frame_delta(prev, img, args.width, args.height,
                                                     emit_int=args.emit_int,
                                                     emit_char=args.emit_char or not args.emit_int,
                                                     unroll=args.unroll,
                                                     no_comments=args.no_comments)]
        out_lines.append("    RET")
        out_lines.append("")

    out_path = os.path.abspath(args.out_path)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(out_lines))
    print(f"Wrote single ASM with {len(frames)} frame(s), repeat={repeat}: {out_path}")

def main():
    ap = argparse.ArgumentParser(description="Convert image/video to Toy-ASM ASCII (optimized, verbose).")
    ap.add_argument("--in", dest="in_path", required=True, help="Input image or video path")
    ap.add_argument("--out", dest="out_path", default="out.asm", help="Output .asm path (image or single video file)")
    ap.add_argument("--width", type=int, default=256, help="Screen width (default 256)")
    ap.add_argument("--height", type=int, default=64, help="Screen height (default 64)")

    ap.add_argument("--mode", choices=["binary", "levels"], default="binary", help="Binary threshold or multi-level mapping")
    ap.add_argument("--threshold", type=int, default=127, help="Binary: threshold 0..255 (ignored with --otsu)")
    ap.add_argument("--otsu", action="store_true", help="Binary: use Otsu's thresholding")
    ap.add_argument("--on", dest="on_char", default="#", help="Binary: char for ON pixels (default '#')")
    ap.add_argument("--off", dest="off_char", default=" ", help="Binary: char for OFF pixels (default space)")
    ap.add_argument("--ramp", default="@#*+=-:. ", help="Levels: darkest->lightest chars (default '@#*+=-:. ')")
    ap.add_argument("--gamma", type=float, default=1.0, help="Levels: gamma correction (default 1.0)")
    ap.add_argument("--invert", action="store_true", help="Invert after threshold/normalize")

    ap.add_argument("--emit-int", action="store_true", help="Emit PUSH <code> instead of PUSH 'c'")
    ap.add_argument("--emit-char", action="store_true", help="Prefer PUSH 'c' (fallback to int for ' and \\). Default if neither is set.")
    ap.add_argument("--skip-off", action="store_true", help="Skip OFF/lightest writes (full frames only; requires CLEANVM)")
    ap.add_argument("--skip-char", default=None, help="Explicit character to skip writing (overrides --skip-off)")
    ap.add_argument("--no-clear", action="store_true", help="Do not emit CLEANVM")
    ap.add_argument("--unroll", type=int, default=3, help="Unroll write runs up to this length (default 3)")
    ap.add_argument("--no-comments", action="store_true", help="Suppress comments for smaller output")

    ap.add_argument("--frame-step", type=int, default=1, help="Keep every Nth frame (video)")
    ap.add_argument("--max-frames", type=int, default=0, help="Limit number of frames (0 = no limit)")
    ap.add_argument("--delay-iters", type=int, default=0, help="Busy-wait iterations between frames (0 = none)")
    ap.add_argument("--repeat", type=int, default=1, help="Repeat each frame N times (slows playback deterministically)")
    ap.add_argument("--fps-target", type=float, default=0.0, help="Approximate target FPS from input FPS (<=0 disables)")
    ap.add_argument("--loop", action="store_true", help="Loop video playback")
    ap.add_argument("--no-delta", action="store_true", help="Disable delta frames (emit full frames for all)")

    ap.add_argument("--verbose", action="store_true", help="Print probe info and debug messages to stderr")

    args = ap.parse_args()

    if args.emit_int and args.emit_char:
        print("Warning: both --emit-int and --emit-char set; using --emit-int.", file=sys.stderr)

    ext = os.path.splitext(args.in_path)[1].lower()
    is_video = ext in VIDEO_EXTS

    if is_video:
        handle_video_singlefile(args)
    else:
        handle_image(args)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("ERROR:", e, file=sys.stderr)
        sys.exit(1)
