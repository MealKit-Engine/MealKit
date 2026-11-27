#!/usr/bin/env python3
"""API index for agents (docs/ROADMAP.md A3): one short text file per engine module and per kernel folder.

  python tools/gen_api_index.py                  write API.txt for each kernel folder (engine/core/*); a vendored modules/<name> only if it has one
                                                 (those come from the module repo: python tools/gen_api_index.py --module ../module-<name>)
  python tools/gen_api_index.py --module <dir>   one module folder (a module repo checkout works too)
  python tools/gen_api_index.py --check          write nothing; exit 1 when an API.txt is missing or out of date

Each line: `<header>:<line> <declaration>  // <first sentence of its comment>`. Headers listed in engine/public_headers.txt start the
line with `+`, the others with a space. Only the module's include/mealkit/ headers are read (not src/ and not editor/). Public
members only; data fields and function bodies are left out. Generated: never edit API.txt by hand.
"""
import argparse
import os
import re
import sys

ENGINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_DECL = 140
MAX_NOTE = 90
OUT_NAME = "API.txt"

TYPE_RE = re.compile(r"^(?:template\s*<.*?>\s*)?(class|struct|union)\b(?!\s*\()")
SKIP_START = ("friend ", "static_assert", "using namespace", "extern \"C\"", "return ", "public:", "private:", "protected:")
MACRO_RE = re.compile(r"^[A-Z][A-Z0-9_]{3,}\s*\(")


def squeeze(text):
    return re.sub(r"\s+", " ", text).strip()


def first_sentence(lines):
    text = squeeze(" ".join(lines))
    if not text:
        return ""
    m = re.search(r"[.!?](\s|$)", text)
    if m:
        text = text[: m.start() + 1]
    if len(text) > MAX_NOTE:
        text = text[: MAX_NOTE - 3].rstrip() + "..."
    return text


class Scanner:
    """Walks one header, tracking namespace/class scopes, and collects public declarations."""

    def __init__(self, text):
        self.s = text
        self.i = 0
        self.line = 1
        self.out = []  # (line, decl, note)
        self.pending = []  # comment lines directly above the next declaration
        self.last_blank = True

    # --- low level -------------------------------------------------------------
    def peek(self, n=0):
        j = self.i + n
        return self.s[j] if j < len(self.s) else ""

    def adv(self):
        c = self.s[self.i]
        if c == "\n":
            self.line += 1
        self.i += 1
        return c

    def skip_string(self, q):
        self.adv()
        while self.i < len(self.s):
            c = self.adv()
            if c == "\\":
                if self.i < len(self.s):
                    self.adv()
            elif c == q or c == "\n":
                break

    def skip_raw(self):
        m = re.compile(r'R"([^(\s]*)\(').match(self.s, self.i)
        end = ")" + m.group(1) + '"'
        j = self.s.find(end, m.end())
        j = len(self.s) if j < 0 else j + len(end)
        while self.i < j:
            self.adv()

    def read_line_comment(self):
        j = self.s.find("\n", self.i)
        j = len(self.s) if j < 0 else j
        text = self.s[self.i + 2 : j].strip()
        while self.i < j:
            self.adv()
        return text

    def skip_block_comment(self):
        j = self.s.find("*/", self.i + 2)
        j = len(self.s) if j < 0 else j + 2
        text = self.s[self.i + 2 : j - 2]
        while self.i < j:
            self.adv()
        return squeeze(re.sub(r"^\s*\*+", "", text, flags=re.M))

    def skip_preproc(self):
        while self.i < len(self.s):
            j = self.s.find("\n", self.i)
            j = len(self.s) if j < 0 else j
            cont = self.s[self.i : j].rstrip().endswith("\\")
            while self.i < j:
                self.adv()
            if self.i < len(self.s):
                self.adv()
            if not cont:
                break

    def skip_block(self):
        """At '{': skip to the matching '}' (strings, comments aware)."""
        depth = 0
        while self.i < len(self.s):
            c = self.peek()
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    self.adv()
                    return
            elif c == '"' or c == "'":
                self.skip_string(c)
                continue
            elif c == "/" and self.peek(1) == "/":
                self.read_line_comment()
                continue
            elif c == "/" and self.peek(1) == "*":
                self.skip_block_comment()
                continue
            self.adv()

    # --- scope walking ---------------------------------------------------------
    def scan(self, scope_kind="ns", prefix=""):
        """Reads declarations until the scope's closing '}' (or EOF). scope_kind: ns | class | struct."""
        public = scope_kind != "class"
        buf = []  # accumulated declaration characters
        start_line = self.line
        notes = []  # comment for the declaration being read
        trail = []  # trailing comment on the declaration's last line
        while self.i < len(self.s):
            c = self.peek()
            if c == "\n":
                self.adv()
                if not squeeze("".join(buf)):
                    if self.last_blank:
                        self.pending = []
                    self.last_blank = True
                continue
            if c in " \t\r":
                self.adv()
                buf.append(" ") if buf else None
                continue
            if c == "/" and self.peek(1) == "/":
                text = self.read_line_comment()
                if squeeze("".join(buf)):
                    trail.append(text)
                else:
                    self.pending.append(text)
                    self.last_blank = False
                continue
            if c == "/" and self.peek(1) == "*":
                text = self.skip_block_comment()
                if not squeeze("".join(buf)):
                    self.pending.append(text)
                    self.last_blank = False
                continue
            if c == "#" and not squeeze("".join(buf)):
                self.skip_preproc()
                self.pending = []
                continue
            if c == '"' or c == "'":
                a = self.i
                self.skip_string(c)
                buf.append(self.s[a : self.i])
                continue
            if c == "R" and self.peek(1) == '"' and re.compile(r'R"([^(\s]*)\(').match(self.s, self.i):
                a = self.i
                self.skip_raw()
                buf.append(self.s[a : self.i])
                continue
            if c == "}":
                self.adv()
                return
            text = squeeze("".join(buf))
            if not text:
                start_line = self.line
                notes = self.pending
                self.pending = []
                self.last_blank = False
            # access labels
            if c == ":" and self.peek(1) != ":" and text in ("public", "private", "protected"):
                public = text == "public"
                self.adv()
                buf = []
                trail = []
                continue
            if c == ":" and self.peek(1) == ":":
                buf.append("::")
                self.adv()
                self.adv()
                continue
            if c == ";":
                self.adv()
                self.emit(text, public, prefix, start_line, notes + trail, False)
                buf, trail, notes = [], [], []
                continue
            if c == "{":
                self.on_brace(text, public, prefix, start_line, notes + trail, scope_kind)
                buf, trail, notes = [], [], []
                continue
            if c == "(" and not buf_has_paren_depth(buf) and MACRO_RE.match(text + "(") and not text.count(" "):
                # NAME(...) macro line such as MEALKIT_STRUCT(Tuning) with no ';' after it
                a = self.i
                depth = 0
                while self.i < len(self.s):
                    ch = self.adv()
                    if ch == "(":
                        depth += 1
                    elif ch == ")":
                        depth -= 1
                        if depth == 0:
                            break
                call = text + squeeze(self.s[a : self.i])
                rest = self.s[self.i : self.s.find("\n", self.i) if "\n" in self.s[self.i :] else len(self.s)]
                if rest.strip() == "" or rest.strip().startswith("//"):
                    if public and prefix == "" or public:
                        self.out.append((start_line, call, first_sentence(notes)))
                    buf, trail, notes = [], [], []
                    continue
                buf.append(call)
                continue
            buf.append(self.adv())

    def on_brace(self, text, public, prefix, line, notes, scope_kind):
        m = TYPE_RE.match(text)
        if re.match(r"^(inline\s+)?namespace\b", text):
            name = squeeze(re.sub(r"^(inline\s+)?namespace", "", text))
            self.adv()
            self.scan("ns", prefix)  # namespaces add no prefix: names stay short
            return
        if text.startswith('extern "C"'):
            self.adv()
            self.scan("ns", prefix)
            return
        if m and "(" not in text.split(":")[0] and not text.endswith("="):
            head = re.sub(r"\s*final\b", "", text)
            head = re.sub(r"^template\s*<.*?>\s*", "", head)
            name = squeeze(re.split(r"[:{]", head.split(None, 1)[1] if " " in head else head)[0])
            name = re.sub(r"\[\[.*?\]\]\s*", "", name).split()[-1] if name else ""
            self.adv()
            if public:
                self.out.append((line, squeeze(prefix + self.shorten(text)), first_sentence(notes)))
            self.scan("class" if m.group(1) == "class" else "struct", (prefix + name + "::") if name else prefix)
            self.pending = []
            return
        if re.match(r"^(enum\b|typedef enum\b)", text):
            self.skip_block()
            body_end = self.i
            # trailing `;` handled by the next loop turn (empty declaration is ignored)
            if public:
                self.out.append((line, squeeze(prefix + self.shorten(text)), first_sentence(notes)))
            return
        # function body, lambda, brace initializer, MEALKIT_* block ...
        self.skip_block()
        if "(" in text and public and not text.rstrip().endswith("="):
            self.emit(text, public, prefix, line, notes, True)

    def shorten(self, text):
        text = re.sub(r"\s*:\s*public\s+", " : ", text)
        return text if len(text) <= MAX_DECL else text[: MAX_DECL - 3].rstrip() + "..."

    def emit(self, text, public, prefix, line, notes, had_body):
        if not public or not text:
            return
        if text.startswith(SKIP_START) or text.startswith("MEALKIT_FIELD") or text.startswith("template <> "):
            return
        # data fields and plain constants without '(' stay out, except type aliases and constants
        is_func = "(" in text
        is_alias = text.startswith(("using ", "typedef "))
        is_const = bool(re.match(r"^(template\s*<.*?>\s*)?(inline\s+)?(static\s+)?(constexpr|inline)\s", text))
        if text.startswith(("class ", "struct ", "union ", "enum ")) and not is_func:
            # forward declaration: not worth a line
            return
        if not (is_func or is_alias or is_const):
            return
        if is_func and re.search(r"\)\s*(const\s*)?(noexcept\s*)?=\s*(delete|default)\b", text):
            return
        if is_func and text.startswith("~"):
            return
        text = re.sub(r"\s*=\s*0\s*$", "", text) if text.endswith("= 0") else text
        text = re.sub(r"\)\s*:\s*[A-Za-z_][\w:<>]*\(.*$", ")", text) if had_body else text  # ctor init lists
        if text.startswith("MEALKIT_"):
            return  # field/table declaration macros: the type's own line is enough
        if is_const and not is_func:
            text = re.sub(r"\s*=\s*.*$", "", text)
        if re.search(r"\boperator\b", text.split("(")[0]):
            return  # operators are noise in an index
        if prefix and is_func:
            p = text.index("(")
            m = re.search(r"(~?\w+)\s*$", text[:p])
            decl = (text[: m.start()] + prefix + text[m.start() :]) if m else prefix + text
        elif prefix:
            m = re.search(r"(\w+)\s*$", text)
            decl = (text[: m.start()] + prefix + text[m.start() :]) if m else prefix + text
        else:
            decl = text
        decl = re.sub(r">(?=\w)", "> ", decl)
        decl = self.shorten(squeeze(decl))
        self.out.append((line, decl, first_sentence(notes)))


def buf_has_paren_depth(buf):
    s = "".join(buf)
    return s.count("(") != s.count(")")


def public_headers():
    out = set()
    path = os.path.join(ENGINE, "engine", "public_headers.txt")
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            h = re.sub(r"#.*$", "", line).strip()
            if h:
                out.add(h)
    return out


def module_index(mod_dir, public):
    inc = os.path.join(mod_dir, "include", "mealkit")
    lines = []
    if not os.path.isdir(inc):
        return None
    for root, dirs, files in os.walk(inc):
        dirs.sort()
        for f in sorted(files):
            if not f.endswith((".h", ".hpp")):
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, inc).replace(os.sep, "/")
            text = open(full, encoding="utf-8", errors="replace").read().replace("\r\n", "\n")
            if text.lstrip("\ufeff").startswith("// Vendored copy of module-"):  # added by the engine's copy: line numbers are the repo's
                text = text.split("\n", 1)[-1]
            sc = Scanner(text)
            try:
                sc.scan()
            except Exception as e:  # keep going: one odd header must not stop the index
                sys.stderr.write(f"warning: {rel}: {e}\n")
            mark = "+" if rel in public else " "
            seen = set()
            for ln, decl, note in sc.out:
                key = (ln, decl)
                if key in seen:
                    continue
                seen.add(key)
                lines.append(f"{mark}{rel}:{ln} {decl}" + (f"  // {note}" if note else ""))
    return lines


def module_info(mod_dir):
    name = os.path.basename(mod_dir.rstrip("/\\"))
    summary = version = ""
    mt = os.path.join(mod_dir, "module.txt")
    if os.path.exists(mt):
        for line in open(mt, encoding="utf-8"):
            if line.startswith("name "):
                name = line.split(None, 1)[1].strip()
            elif line.startswith("version "):
                version = line.split(None, 1)[1].strip()
            elif line.startswith("summary "):
                summary = line.split(None, 1)[1].strip()
    return name, version, summary


def render(mod_dir, public):
    lines = module_index(mod_dir, public)
    if lines is None:
        return None
    name, version, summary = module_info(mod_dir)
    head = [
        f"# API index: module {name} {version}".rstrip(),
        f"# {summary}" if summary else "# ",
        "# Generated by tools/gen_api_index.py from include/mealkit/ (never edit by hand). Public members only, no data fields.",
        "# Line: [+ = listed in engine/public_headers.txt]<header under include/mealkit/>:<line> <declaration>  // <comment>",
        "",
    ]
    return "\n".join(head + lines) + "\n"


def all_modules():
    mods = []
    for base in (os.path.join(ENGINE, "engine", "core"), os.path.join(ENGINE, "modules")):
        if not os.path.isdir(base):
            continue
        for d in sorted(os.listdir(base)):
            full = os.path.join(base, d)
            if not os.path.exists(os.path.join(full, "module.txt")):
                continue
            # modules/<name> is a vendored copy: its API.txt comes from the module repo (run --module ..\module-<name> there), so it is
            # only checked or refreshed here when it already exists. Kernel folders belong to this repo and always get one.
            if base.endswith("modules") and not os.path.exists(os.path.join(full, OUT_NAME)):
                continue
            mods.append(full)
    return mods


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--module", action="append", default=[], help="a module folder (default: every module of this engine)")
    ap.add_argument("--check", action="store_true", help="write nothing; exit 1 if an API.txt is missing or stale")
    args = ap.parse_args()
    public = public_headers()
    mods = [os.path.abspath(m) for m in args.module] or all_modules()
    bad = 0
    for m in mods:
        text = render(m, public)
        if text is None:
            print(f"skip {m}: no include/mealkit/")
            continue
        out = os.path.join(m, OUT_NAME)
        old = open(out, encoding="utf-8", newline="").read() if os.path.exists(out) else None
        if args.check:
            if old != text:
                print(f"STALE {os.path.relpath(out, ENGINE)}")
                bad += 1
            continue
        if old != text:
            with open(out, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
        print(f"{'wrote' if old != text else 'same '} {os.path.relpath(out, ENGINE)} ({text.count(chr(10)) - 5} lines)")
    if args.check:
        print("OK API index up to date" if not bad else f"FAIL API index: {bad} file(s) stale (python tools/gen_api_index.py)")
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
