import argparse
import os
import sys
from typing import List, Optional

import cv2
import numpy as np

VIDEO_EXTS = {".mp4", ".mov", ".mkv", ".avi", ".webm", ".m4v"}
ON_CHAR  = "#"
OFF_CHAR = " "

# --------------------- Image → ASCII mapping ---------------------

def image_to_ascii_chars(gray: np.ndarray,
                         mode: str,
                         invert: bool,
                         gamma: float,
                         ramp: str) -> np.ndarray:
    if mode == "binary":
        _, bw = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        if invert:
            bw = 255 - bw
        return np.where(bw == 0, ON_CHAR, OFF_CHAR)

    f = gray.astype(np.float32) / 255.0
    if invert:
        f = 1.0 - f
    if gamma != 1.0 and gamma > 0.0:
        f = np.clip(f, 0.0, 1.0) ** (1.0 / gamma)

    if not ramp or len(ramp) < 2:
        raise ValueError("--ramp must contain at least 2 characters (dark→light)")

    L = len(ramp)
    idx = np.rint(f * (L - 1)).astype(np.int32)
    idx = np.clip(idx, 0, L - 1)
    lut = np.array(list(ramp), dtype='<U1')
    return lut[idx]

# --------------------- ASM emission helpers ---------------------

def emit_push_char(ch: str, emit_int: bool) -> str:
    if emit_int or ch in ("'", "\\"):
        return f"PUSH {ord(ch)}"
    return f"PUSH '{ch}'"

def emit_header(no_comments: bool, header_lines: List[str]) -> List[str]:
    return ([] if no_comments else header_lines) + ([] if no_comments else [""])

def emit_subroutine_f(no_comments: bool) -> List[str]:
    """Only long-run writer subroutine; no other helpers, no delays."""
    lines: List[str] = []
    if not no_comments:
        lines += ["; write run helper"]
    lines += [
        ":f",
        ":f_loop",
        "    PUSHR x1",
        "    PUSH 0",
        "    JBE :f_done",
        "    PUSHR x2",
        "    POPVM [x0]",
        "    PUSHR x0",
        "    PUSH 1",
        "    ADD",
        "    POPR x0",
        "    PUSHR x1",
        "    PUSH 1",
        "    SUB",
        "    POPR x1",
        "    JMP :f_loop",
        ":f_done",
        "RET",
        ""
    ]
    return lines

def emit_skip_add(run_len: int) -> List[str]:
    return ["PUSHR x0", f"PUSH {run_len}", "ADD", "POPR x0"]

def emit_write_run(ch: str, run_len: int, emit_int: bool) -> List[str]:
    if run_len <= 3:
        out: List[str] = []
        push_line = emit_push_char(ch, emit_int)
        for _ in range(run_len):
            out.append(push_line)
            out.append("POPVM [x0]")
            out += emit_skip_add(1)
        return out
    return [
        emit_push_char(ch, emit_int),
        "POPR x2",
        f"PUSH {run_len}",
        "POPR x1",
        "CALL :f",
    ]

def emit_frame_full(char_img: np.ndarray,
                    width: int,
                    height: int,
                    skip_char: Optional[str],
                    emit_int: bool,
                    no_comments: bool) -> List[str]:
    lines: List[str] = []
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
                lines += emit_write_run(ch, run_len, emit_int)
            x += run_len
        if not no_comments:
            lines.append("")
    lines.append("DRAW")
    return lines

def emit_frame_delta(prev_img: np.ndarray,
                     cur_img:  np.ndarray,
                     width:    int,
                     height:   int,
                     emit_int: bool,
                     no_comments: bool) -> List[str]:
    lines: List[str] = []
    lines += ["PUSH 0", "POPR x0"]
    for y in range(height):
        if not no_comments:
            lines.append(f"; row {y}")
        prev = prev_img[y]
        cur  = cur_img[y]
        x = 0
        while x < width:
            u = x
            while u < width:
                pv = prev[u].item() if hasattr(prev[u], "item") else prev[u]
                cv = cur[u].item()  if hasattr(cur[u],  "item") else cur[u]
                if cv != pv: break
                u += 1
            if u > x:
                lines += emit_skip_add(u - x)
                x = u
                if x >= width: break
            v = x
            while v < width:
                pv = prev[v].item() if hasattr(prev[v], "item") else prev[v]
                cv = cur[v].item()  if hasattr(cur[v],  "item") else cur[v]
                if cv == pv: break
                v += 1
            i = x
            while i < v:
                ch = cur[i].item() if hasattr(cur[i], "item") else cur[i]
                run_len = 1
                while i + run_len < v:
                    nxt = cur[i + run_len]
                    nxt = nxt.item() if hasattr(nxt, "item") else nxt
                    if nxt != ch: break
                    run_len += 1
                lines += emit_write_run(ch, run_len, emit_int)
                i += run_len
            x = v
        if not no_comments:
            lines.append("")
    lines.append("DRAW")
    return lines

# --------------------- Generators ---------------------

def handle_image(args):
    img = cv2.imread(args.in_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise RuntimeError("Failed to load image")
    resized = cv2.resize(img, (args.width, args.height), interpolation=cv2.INTER_AREA)
    char_img = image_to_ascii_chars(resized, args.mode, args.invert, args.gamma, args.ramp)

    eff_skip = None
    if args.skip_off:
        eff_skip = OFF_CHAR if args.mode == "binary" else (args.ramp[-1] if args.ramp else " ")

    lines: List[str] = []
    lines += emit_header(args.no_comments, [
        "; Generated by visual2tasm.py (image, minimal)",
        f"; {os.path.basename(args.in_path)}  {args.width}x{args.height}  mode={args.mode} inv={args.invert} gamma={args.gamma} ramp='{args.ramp if args.mode=='levels' else ''}'"
    ])
    lines.append("JMP :__main")
    lines += emit_subroutine_f(args.no_comments)
    lines.append(":__main")
    lines += emit_frame_full(char_img, args.width, args.height,
                             skip_char=eff_skip,
                             emit_int=args.emit_int,
                             no_comments=args.no_comments)
    lines.append("HLT")

    out_dir = os.path.dirname(os.path.abspath(args.out_path))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("Wrote", args.out_path)

def handle_video_singlefile(args):
    cap = cv2.VideoCapture(args.in_path)
    if not cap.isOpened():
        raise RuntimeError("Failed to open video")

    frames: List[np.ndarray] = []
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        resized = cv2.resize(gray, (args.width, args.height), interpolation=cv2.INTER_AREA)
        char_img = image_to_ascii_chars(resized, args.mode, args.invert, args.gamma, args.ramp)
        frames.append(char_img)
    cap.release()

    if not frames:
        raise RuntimeError("No frames captured")

    eff_skip = None
    if args.skip_off:
        eff_skip = OFF_CHAR if args.mode == "binary" else (args.ramp[-1] if args.ramp else " ")

    out: List[str] = []
    out += emit_header(args.no_comments, [
        "; Generated by visual2tasm.py (video, minimal)",
        f"; {os.path.basename(args.in_path)}  frames={len(frames)}  {args.width}x{args.height} mode={args.mode} ramp='{args.ramp if args.mode=='levels' else ''}'"
    ])
    out.append("JMP :__start")
    out += emit_subroutine_f(args.no_comments)

    out.append(":__start")
    for i in range(len(frames)):
        out.append(f"    CALL :frame_{i:06d}")
    out.append("    HLT")
    out.append("")

    for i, img in enumerate(frames):
        if not args.no_comments:
            out.append(f"; ------- frame {i} -------")
        out.append(f":frame_{i:06d}")
        if i == 0 or args.no_delta:
            out += ["    " + ln if ln and not ln.startswith(';') else ln
                    for ln in emit_frame_full(img, args.width, args.height,
                                              skip_char=eff_skip,
                                              emit_int=args.emit_int,
                                              no_comments=args.no_comments)]
        else:
            prev = frames[i - 1]
            out += ["    " + ln if ln and not ln.startswith(';') else ln
                    for ln in emit_frame_delta(prev, img, args.width, args.height,
                                               emit_int=args.emit_int,
                                               no_comments=args.no_comments)]
        out.append("    RET")
        out.append("")

    out_path = os.path.abspath(args.out_path)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    print("Wrote", out_path)

def main():
    ap = argparse.ArgumentParser(description="Minimal image/video → Toy-ASM (no pacing), with ramp support")
    ap.add_argument("--in",  dest="in_path",  required=True, help="Input image or video path")
    ap.add_argument("--out", dest="out_path", default="out.asm", help="Output .asm path")
    ap.add_argument("--width",  type=int, default=256, help="Screen width")
    ap.add_argument("--height", type=int, default=64,  help="Screen height")

    ap.add_argument("--mode", choices=["binary", "levels"], default="binary", help="Mapping mode")
    ap.add_argument("--gamma", type=float, default=1.0, help="Levels: gamma correction")
    ap.add_argument("--invert", action="store_true", help="Invert after threshold/normalize")
    ap.add_argument("--ramp", default="@#*+=-:. ", help="Levels: darkest→lightest chars (default '@#*+=-:. ')")

    ap.add_argument("--emit-int",    action="store_true", help="Emit PUSH <code> instead of PUSH 'c'")
    ap.add_argument("--skip-off",    action="store_true", help="Skip OFF/lightest writes on full frames")
    ap.add_argument("--no-comments", action="store_true", help="Suppress comments")
    ap.add_argument("--no-delta",    action="store_true", help="Disable delta frames (emit full frames for all)")

    args = ap.parse_args()

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
