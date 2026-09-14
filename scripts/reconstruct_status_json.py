#!/usr/bin/env python3
"""Mechanically reconstruct the /status, /defense_config and /vehicle_ota_status
JSON that the firmware's C++ emitters build, then validate with json.loads.

Walks the accumulator (`j +=` / `json +=`) emission statements of the dashboard
header in source order, substitutes placeholders for non-literal appends, and
concatenates string literals verbatim. Catches the recurring quote/brace bug
class (beta06 / beta07 / beta10) that substring contract tests miss: any
unbalanced delimiter makes json.loads fail at the exact byte offset.

v4 fidelity notes (each gap below produced a FALSE alarm in v1-v3):
- statement splitting is quote/paren/brace aware: `;` only splits at paren
  depth 0; `{`/`}` are their own tokens so a trailing `json += "}"` after a
  scope block still matches; two appends on one line are two statements;
- expressions are tokenized (plain/raw strings, char literals, code) and
  modeled as C concatenations: top-level `+` splits atoms, identifier atoms
  between literals are string-valued macros -> placeholder "0", top-level
  ternary -> branch A, everything else code-ish (String(...), calls,
  numbers) -> "0". Placeholders are valid JSON values so the literal
  skeleton (quotes/commas/braces/colons) is what gets validated;
- emission inside if/for blocks is walked unconditionally (logged as
  conditional) — if/else dual arms can in principle false-alarm; eyeball
  the [skip] list when that happens;
- value-returning builders (`j += dashCapabilitiesJson();`) are inlined.

Target preprocessor world: DRIVER_T2CAN_DUAL + ESP32_DASHBOARD (the
lilygo_t2can_dual build Jordan flashes).
"""
import json
import re
import sys

SRC = "include/web/mcp2515_dashboard.h"
DEFINES = {"DRIVER_T2CAN_DUAL", "ESP32_DASHBOARD"}

FUNCS = [
    "handleStatus",
    "dashAppendLegacySafetyControlsJson",
    "appendJitterDiagJson",
    "dashDefenseConfigJson",
    "appendLegacyNagDiagJson",
    "handleVehicleOtaStatus",
    "dashCapabilitiesJson",
]

RAW_ANY_RE = re.compile(r'R"(\w*)\((.*?)\)\1"', re.S)
CHR_RE = re.compile(r"'(\\.|[^\\'])'")
IDENT_RE = re.compile(r"^\s*\w+\s*$")


def extract_function(src, name):
    sig_re = re.compile(r"(?:static\s+)?(?:void|String)\s+" + name + r"\s*\(")
    i = None
    for m in sig_re.finditer(src):
        # find the matching close paren of this signature
        depth = 0
        k = m.end() - 1
        while k < len(src):
            if src[k] == "(":
                depth += 1
            elif src[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        # a definition is followed by '{'; a forward declaration by ';'
        window = src[k + 1 : k + 200]
        if not window.lstrip().startswith("{"):
            continue
        i = k + 1 + window.index("{")
        break
    if i is None:
        return None
    depth = 0
    in_str = in_chr = in_line = in_blk = esc = False
    k = i
    while k < len(src):
        c = src[k]
        # Raw string literals (R"JSON(...)JSON") — the naive quote toggling
        # below loses phase on their embedded quotes (a beta14 accident hid
        # inside a quote-parity coincidence; fixed with the beta15 JITTER
        # surgery). Skip the whole literal before any state machine sees it.
        if not in_str and not in_chr and not in_line and not in_blk:
            m = RAW_ANY_RE.match(src, k if c == "R" else k - 1)
            if m and m.start() == (k if c == "R" else k - 1) and src[m.start():m.start()+2] == 'R"':
                k = m.end()
                continue
        if esc:
            esc = False
        elif in_str:
            if c == "\\":
                esc = True
            elif c == '"':
                in_str = False
        elif in_chr:
            if c == "\\":
                esc = True
            elif c == "'":
                in_chr = False
        elif in_line:
            if c == "\n":
                in_line = False
        elif in_blk:
            if c == "*" and src[k + 1 : k + 2] == "/":
                in_blk = False
                k += 1
        elif c == '"':
            in_str = True
        elif c == "'":
            in_chr = True
        elif c == "/" and src[k + 1 : k + 2] == "/":
            in_line = True
        elif c == "/" and src[k + 1 : k + 2] == "*":
            in_blk = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[i + 1 : k]
        k += 1
    return None


def c_unescape(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append({'"': '"', "\\": "\\", "n": "\n", "t": "\t", "'": "'"}.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(ln.split("//", 1)[0] for ln in text.splitlines())


def cond_value(cond):
    """Evaluate an #if expression in the target world; None = unknown."""
    cond = cond.strip()
    m = re.fullmatch(r"defined\s*\(\s*(\w+)\s*\)", cond)
    if m:
        return m.group(1) in DEFINES
    m = re.fullmatch(r"!\s*defined\s*\(\s*(\w+)\s*\)", cond)
    if m:
        return m.group(1) not in DEFINES
    return None


def preprocess(text):
    out, stack, unknown = [], [], []
    for ln in strip_comments(text).splitlines():
        s = ln.strip()
        if s.startswith("#ifdef "):
            cond = s[7:].strip()
            stack.append((None, cond in DEFINES, cond))
        elif s.startswith("#ifndef "):
            cond = s[8:].strip()
            stack.append((None, cond not in DEFINES, cond))
        elif s.startswith("#if "):
            cond = s[3:]
            val = cond_value(cond)
            if val is None:
                unknown.append(cond)
                val = True
            stack.append((None, val, cond))
        elif s.startswith("#else"):
            _, cond, _ = stack[-1]
            stack[-1] = (True, not cond, "")
        elif s.startswith("#elif"):
            stack[-1] = (True, False, "")
        elif s.startswith("#endif"):
            stack.pop()
        else:
            active = True
            for _, val, _ in stack:
                active = active and val
            if active:
                out.append(ln)
    return "\n".join(out), unknown


# ---------------------------------------------------------------- tokens

def tokenize(expr):
    """Split an expression into ('str', content) / ('chr', c) / ('code', text)."""
    toks = []
    i, n = 0, len(expr)
    code_start = None

    def flush(end):
        nonlocal code_start
        if code_start is not None and end > code_start:
            toks.append(("code", expr[code_start:end]))
        code_start = None

    while i < n:
        if expr[i] == '"':
            j = i + 1
            while j < n:
                if expr[j] == "\\":
                    j += 2
                elif expr[j] == '"':
                    break
                else:
                    j += 1
            flush(i)
            toks.append(("str", c_unescape(expr[i + 1 : min(j, n)])))
            i = j + 1
            continue
        m = RAW_ANY_RE.match(expr, i)
        if m:
            flush(i)
            toks.append(("str", m.group(2)))
            i = m.end()
            continue
        m = CHR_RE.match(expr, i)
        if m:
            flush(i)
            toks.append(("chr", c_unescape(m.group(1))))
            i += m.end() - i
            continue
        if code_start is None:
            code_start = i
        i += 1
    flush(n)
    return toks


def parse_tokens(toks):
    """Literal skeleton of a tokenized expression (see docstring)."""
    # 1) top-level ternary -> branch A only. Track CHARACTER positions so a
    #    branch that starts and ends inside one code run is handled.
    depth = 0
    q = colon = None  # (token_idx, char_offset)
    for idx, (kind, val) in enumerate(toks):
        if kind != "code":
            continue
        for ci, ch in enumerate(val):
            if ch in "([":
                depth += 1
            elif ch in ")]":
                depth -= 1
            elif ch == "?" and depth == 0 and q is None:
                q = (idx, ci)
            elif ch == ":" and depth == 0 and q is not None and colon is None:
                colon = (idx, ci)
    if q is not None and colon is not None:
        if q[0] == colon[0]:
            # branch A is pure code within one run
            return "0"
        sliced = []
        for idx, (kind, val) in enumerate(toks):
            if idx < q[0] or idx > colon[0]:
                continue
            if idx == q[0]:
                sliced.append((kind, val[q[1] + 1 :]))
            elif idx == colon[0]:
                sliced.append((kind, val[: colon[1]]))
            else:
                sliced.append((kind, val))
        sliced = [t for t in sliced if not (t[0] == "code" and not t[1].strip())]
        return parse_tokens(sliced)

    # 2) split at top-level '+' into atoms
    parts = [[]]
    depth = 0
    for kind, val in toks:
        if kind == "code":
            seg_start = 0
            for ci, ch in enumerate(val):
                if ch in "([":
                    depth += 1
                elif ch in ")]":
                    depth -= 1
                elif ch == "+" and depth == 0:
                    parts[-1].append(("code", val[seg_start:ci]))
                    parts.append([])
                    seg_start = ci + 1
            parts[-1].append(("code", val[seg_start:]))
        else:
            parts[-1].append((kind, val))

    out = []
    for part in parts:
        ptoks = [t for t in part if not (t[0] == "code" and not t[1].strip())]
        if ptoks:
            out.append(parse_atom(ptoks))
    return "".join(out)


def parse_atom(toks):
    if len(toks) == 1:
        kind, val = toks[0]
        return val if kind in ("str", "chr") else "0"
    # C string adjacency: only valid if every code token is a bare identifier
    # (a macro expanding to a string literal); placeholder per macro.
    out = []
    for kind, val in toks:
        if kind in ("str", "chr"):
            out.append(val)
        elif IDENT_RE.match(val):
            out.append("0")
        else:
            return "0"
    return "".join(out)


# ------------------------------------------------------------- statements

def split_statements(text):
    """Quote/paren/brace-aware statement splitter -> [('stmt'|'brace', s)]."""
    items = []
    buf = []
    paren = 0
    i, n = 0, len(text)

    def flush_stmt():
        s = "".join(buf).strip()
        if s:
            items.append(("stmt", s))
        buf.clear()

    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                elif text[j] == '"':
                    break
                else:
                    j += 1
            buf.append(text[i : min(j + 1, n)])
            i = j + 1
            continue
        m = RAW_ANY_RE.match(text, i)
        if m:
            buf.append(m.group(0))
            i = m.end()
            continue
        m = CHR_RE.match(text, i)
        if m:
            buf.append(m.group(0))
            i += m.end() - i
            continue
        if c == "(":
            paren += 1
        elif c == ")":
            paren -= 1
        elif c in "{}":
            flush_stmt()
            items.append(("brace", c))
            i += 1
            continue
        elif c == ";" and paren <= 0:
            flush_stmt()
            i += 1
            continue
        buf.append(c)
        i += 1
    flush_stmt()
    return items


CTRL_HEAD_RE = re.compile(r"^\s*(?:}\s*)?(?:else\s+)?(?:if|for|while|switch)\s*\(")
CASE_RE = re.compile(r"^\s*(?:case\s+[^:]+|default)\s*:")


def strip_control(s):
    """Remove leading if/for/while/switch/case prefixes; -> (core, conditional)."""
    cond = False
    while True:
        m = CASE_RE.match(s)
        if m:
            s = s[m.end() :].strip()
            cond = True
            continue
        m = CTRL_HEAD_RE.match(s)
        if not m:
            m2 = re.match(r"^\s*(?:}\s*)?else\b\s*(.*)$", s, re.S)
            if m2 and m2.group(1).strip():
                s = m2.group(1).strip()
                cond = True
                continue
            break
        depth = 0
        start = m.end() - 1
        end = None
        for k in range(start, len(s)):
            if s[k] == "(":
                depth += 1
            elif s[k] == ")":
                depth -= 1
                if depth == 0:
                    end = k + 1
                    break
        if end is None:
            break
        s = s[end:].strip()
        cond = True
    return s, cond


# ------------------------------------------------------------------ build

def find_accum(name, src, body):
    m = re.search(r"String\s+(\w+)\s*=", body)
    if m:
        return m.group(1)
    sig = re.search(r"(?:static\s+)?(?:void|String)\s+" + name + r"\s*\(([^)]*)\)", src)
    if sig:
        p = re.search(r"String\s*&\s*(\w+)", sig.group(1))
        if p:
            return p.group(1)
    return None


def literal_from_expr(expr, callmap=None, skipped=None, depth=0):
    expr = expr.strip()
    if callmap:
        m = re.fullmatch(r"(\w+)\s*\(\s*\)", expr)
        if m and m.group(1) in callmap:
            sub = build(m.group(1), callmap[m.group(1)], callmap, skipped or [], depth + 1)
            if sub is not None:
                return sub
    return parse_tokens(tokenize(expr))


def build(name, body, callmap, skipped, depth=0, src=None):
    if src is None:
        src = open(SRC, encoding="utf-8").read()
    if depth > 4:
        return "0"
    var = find_accum(name, src, body)
    if var is None:
        print(f"!! {name}: no accumulator found")
        return None
    text, _unknown = preprocess(body)
    out = []
    for kind, s in split_statements(text):
        if kind == "brace":
            continue
        core, conditional = strip_control(s)
        core = core.rstrip(";").strip()
        m = re.match(re.escape(var) + r"\s*\+=\s*(.+)$", core, re.S)
        if m:
            lit = literal_from_expr(m.group(1), callmap, skipped, depth)
            if conditional and lit in (",", ""):
                # loop-join comma (if (i) j += "," / if (!first) j += ','):
                # loop bodies are not unrolled, so the join comma would land
                # before the FIRST element — drop it (log below).
                skipped.append((name, s[:90]))
                continue
            out.append(lit)
            if conditional:
                skipped.append((name, s[:90]))
            continue
        m = re.match(r"(?:String\s+)?" + re.escape(var) + r"\s*=\s*(.+)$", core, re.S)
        if m and not out:
            out.append(literal_from_expr(m.group(1), callmap, skipped, depth))
            continue
        # builder call: foo(var); / foo(var, extra...);
        m = re.match(r"(\w+)\s*\(\s*" + re.escape(var) + r"\s*[,)]", core)
        if m and m.group(1) in callmap:
            sub = build(m.group(1), callmap[m.group(1)], callmap, skipped, depth + 1, src)
            if sub is not None:
                out.append(sub)
            continue
    return "".join(out)


def main():
    src = open(SRC, encoding="utf-8").read()
    bodies = {f: (extract_function(src, f) or "") for f in FUNCS}
    missing = [f for f in FUNCS if not bodies[f]]
    if missing:
        print(f"!! functions not found: {missing}")
        return 1
    callmap = {k: v for k, v in bodies.items() if k != "handleStatus"}

    rc = 0
    for name in ("handleStatus", "dashDefenseConfigJson", "handleVehicleOtaStatus"):
        skipped = []
        text = build(name, bodies[name], callmap, skipped)
        print(f"=== {name}: {len(text)} bytes ===")
        if skipped:
            print(f"  ({len(skipped)} conditional appends walked — eyeball list below)")
        try:
            obj = json.loads(text)
            keys = list(obj.keys())
            print(f"  PARSE OK — {len(keys)} top-level keys: {keys[:10]}{' …' if len(keys) > 10 else ''}")
        except json.JSONDecodeError as e:
            pos = e.pos
            print(f"  !!! PARSE FAILED: {e.msg} at byte {pos}")
            print(f"  before: {text[max(0, pos - 100):pos]!r}")
            print(f"  HERE→  {text[pos:pos + 70]!r}")
            rc = 1
        for fn, st in skipped:
            print(f"    [cond] {fn}: {st}")
    if rc == 0:
        print("\nALL EMISSIONS PARSE CLEAN")
    return rc


if __name__ == "__main__":
    sys.exit(main())
