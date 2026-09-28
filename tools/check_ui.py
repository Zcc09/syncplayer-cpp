"""Check the panel's controls for duplicates and dead entries.

Run before calling any UI change done. It reads the source, so it needs no build:

  python tools/check_ui.py

It fails on:

- an ID_* declared but never used, or a control rect that is never drawn
- the same ID_* driven from two draw functions, where one control can be hit twice
- the same label drawn twice in one function: two buttons offering the same thing, which is
  what a duplicated Play button looked like
"""
import io
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "app", "panel.cpp")
HDR = os.path.join(ROOT, "src", "app", "panel.h")

CONTROL = r"ui_\.(?:button|icon_button|text_field|slider|switch|progress)"
problems = []


def body_of(text, name):
    m = re.search(r"\n\w[\w:<> ]*Panel::%s\([^)]*\)\s*\{" % name, text)
    if not m:
        return None
    i = text.index("{", m.start())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    return None


def main():
    text = io.open(SRC, encoding="utf-8", newline="").read().replace("\r\n", "\n")
    hdr = io.open(HDR, encoding="utf-8", newline="").read().replace("\r\n", "\n")

    declared = [i for i in re.findall(r"\b(ID_[A-Z0-9_]+)\b", text)]
    declared = list(dict.fromkeys(declared))

    # 1. ids that are declared but never used
    for i in declared:
        if len(re.findall(r"\b%s\b" % i, text)) <= 1:
            problems.append("ID %s is declared but never used" % i)

    # 2. the same id from two draw functions
    funcs = {}
    for m in re.finditer(r"Panel::(draw_[a-z_]+)\(", text):
        f = body_of(text, m.group(1))
        if f:
            funcs[m.group(1)] = f
    per_id = defaultdict(set)
    for fn, body in funcs.items():
        for m in re.finditer(CONTROL + r"\((ID_[A-Z0-9_]+)", body):
            per_id[m.group(1)].add(fn)
    for i, fns in sorted(per_id.items()):
        if len(fns) > 1:
            problems.append("ID %s is driven from %s" % (i, ", ".join(sorted(fns))))

    # 3. the same literal label twice in one function, ignoring format strings
    for fn, body in sorted(funcs.items()):
        seen = defaultdict(int)
        for m in re.finditer(r'L"([^"]{3,})"', body):
            lbl = m.group(1)
            if "%" in lbl:
                continue
            seen[lbl] += 1
        for lbl, n in sorted(seen.items()):
            if n > 1:
                problems.append('%s draws "%s" %d times' % (fn, lbl, n))

    # 4. control rects that no draw function mentions
    rects = re.findall(r"ui::RectF ([a-z_]+)_;", hdr)
    for r in rects:
        if r.endswith("_") or "card_" in r or r in ("title_bar", "tab_strip", "content",
                                                    "status_strip"):
            continue
        if not re.search(r"\b%s_\b" % r, text):
            problems.append("rect %s_ is declared but never used" % r)

    for p in problems:
        print("  FAIL", p)
    print("%d problem(s)" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
