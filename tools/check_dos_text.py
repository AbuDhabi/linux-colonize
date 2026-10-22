#!/usr/bin/env python3
"""check_dos_text: no DOS wording typed into src/ (docs/data_vs_hardcoded.md Part D).

Every C string literal in src/ (comments, diag_*/stderr debug output and
identifier-like words skipped; adjacent literals joined; split at printf
conversions) is normalised (lowercase, punctuation -> space) and looked up in
COLONIZE/*.TXT and in the printable strings of COLONIZE/VICEROY.EXE. A hit means
real DOS wording is compiled in: read it from the catalog (miss = ""), or, if it
only lives in the EXE, rephrase it in the port's own words. Reviewed false
positives (generic phrases) go in tools/check_dos_text.allow:
    path|normalised fragment|reason
Exit 0 = clean, 1 = unlisted hits, 77 = no COLONIZE/ here (ctest SKIP).
Usage (repo root): python3 tools/check_dos_text.py [--min-words N] [--min-chars N] [--all]
  --all also prints allowlisted hits (to re-review the list).
"""
import bisect
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEP = "\x01"  # never matches: stops a fragment spanning two DOS strings/tokens
DEBUG_STMT = re.compile(
    r"\b(diag_\w+|stderr|COLONIZE_TRACE\w*|dbg_\w+|trace_\w+|assert|static_assert)\b"
)
FMT = re.compile(r"%[-+ #0-9.*lhzjt]*[a-zA-Z%]")


def norm(s):
    return re.sub(r"[^a-z0-9\x01]+", " ", s.lower()).strip()


class Corpus:
    """Normalised DOS text; remembers which file/line each offset came from."""

    def __init__(self, root):
        self.text, self.offs, self.src = "", [], []
        for f in sorted(glob.glob(os.path.join(root, "COLONIZE", "*.TXT"))):
            self._add(os.path.basename(f), open(f, "rb").read().decode("latin1").split("\n"), True)
        exe = open(os.path.join(root, "COLONIZE", "VICEROY.EXE"), "rb").read()
        strs = [m.decode("latin1") for m in re.findall(rb"[\x20-\x7e]{6,}", exe)]
        self._add("VICEROY.EXE", strs, False)

    def _add(self, name, lines, is_txt):
        for n, line in enumerate(lines, 1):
            if is_txt:  # %STRING0 / %NUMBER1 / %COUNTRY are slots, not words
                line = re.sub(r"%(STRING|NUMBER)\d|%COUNTRY", SEP, line)
            q = norm(line)
            self.offs.append(len(self.text))
            self.src.append(f"{name}:{n}" if is_txt else f"{name}(string {n})")
            self.text += " " + q + " "

    def find(self, frag):
        i = self.text.find(" " + frag + " ")
        return self.src[bisect.bisect_right(self.offs, i + 1) - 1] if i >= 0 else None


def literals(path):
    """Yield (line, text) for each (concatenated) C string literal outside
    comments, with debug-statement literals dropped."""
    s = open(path, encoding="utf-8", errors="replace").read()
    i, n, line = 0, len(s), 1
    stmt, pending = [], None  # statement text since last ; { }, open concatenation
    gap_blank = False
    while i < n:
        c = s[i]
        if s.startswith("/*", i):
            j = s.find("*/", i + 2)
            j = n if j < 0 else j + 2
            line += s.count("\n", i, j)
            i = j
        elif s.startswith("//", i):
            j = s.find("\n", i)
            i = n if j < 0 else j
        elif c == '"':
            j, buf = i + 1, []
            while j < n and s[j] != '"' and s[j] != "\n":
                if s[j] == "\\" and j + 1 < n:
                    e = s[j + 1]
                    buf.append({"n": " ", "t": " ", "r": " "}.get(e, e))
                    j += 2
                else:
                    buf.append(s[j])
                    j += 1
            text = "".join(buf)
            if pending and gap_blank:  # "a" "b": implicit concatenation
                pending[1] += text
            else:
                if pending:
                    yield tuple(pending)
                pending = [line, text, bool(DEBUG_STMT.search("".join(stmt)))]
            stmt.append("S")
            gap_blank = True
            i = j + 1
        elif c == "'":  # char literal
            j = i + 1
            while j < n and s[j] != "'" and s[j] != "\n":
                j += 2 if s[j] == "\\" else 1
            i = j + 1
            stmt.append("C")
            gap_blank = False
        else:
            if c == "\n":
                line += 1
            if not c.isspace():
                gap_blank = False
                if c in ";{}":
                    stmt = []
                    if pending:
                        yield tuple(pending)
                        pending = None
                else:
                    stmt.append(c)
            i += 1
    if pending:
        yield tuple(pending)


def scan(root, min_words, min_chars, corpus):
    hits = []
    for f in sorted(glob.glob(os.path.join(root, "src", "**", "*.[ch]"), recursive=True)):
        rel = os.path.relpath(f, root)
        for line, text, is_debug in literals(f):
            if is_debug:
                continue
            for frag in FMT.split(text):
                words = [w for w in frag.split() if re.search(r"[A-Za-z]", w)]
                if len(words) < min_words or re.search(r"[_/\\]|^\W*$", frag):
                    continue
                q = norm(frag)
                if len(q) < min_chars:
                    continue
                where = corpus.find(q)
                if where:
                    hits.append((rel, line, frag.strip(), q, where))
    return hits


def selftest(corpus):
    """Canary so a broken scanner cannot pass silently: 2 real hits (one split
    over two literals), comment/diag_ literals ignored."""
    import tempfile

    with tempfile.TemporaryDirectory() as d:
        os.makedirs(os.path.join(d, "src"))
        open(os.path.join(d, "src", "t.c"), "w").write(
            'f("Sons of Liberty membership in");\n'
            'diag_info("Sons of Liberty membership in");\n'
            '/* "Sons of Liberty membership in" */\n'
            'g("Sons of " "Liberty membership in %s");\n'
        )
        assert len(scan(d, 2, 8, corpus)) == 2, "check_dos_text selftest failed"


def main():
    args = sys.argv[1:]
    opt = lambda k, d: int(args[args.index(k) + 1]) if k in args else d
    if not os.path.exists(os.path.join(ROOT, "COLONIZE", "VICEROY.EXE")):
        print("check_dos_text: no COLONIZE/ here, skipped")
        return 77
    allow = {}
    for ln in open(os.path.join(ROOT, "tools", "check_dos_text.allow"), encoding="utf-8"):
        if ln.strip() and not ln.startswith("#"):
            p = ln.rstrip("\n").split("|", 2)
            allow[(p[0], p[1])] = p[2] if len(p) > 2 else ""
    corpus = Corpus(ROOT)
    selftest(corpus)
    hits = scan(ROOT, opt("--min-words", 2), opt("--min-chars", 8), corpus)
    bad, used = [], set()
    for rel, line, frag, q, where in hits:
        if (rel, q) in allow:
            used.add((rel, q))
            if "--all" in args:
                print(f"  allowed {rel}:{line}: \"{frag}\" [{where}] ({allow[(rel, q)]})")
        else:
            bad.append(f'{rel}:{line}: "{frag}"  <- {where}')
    for k in sorted(set(allow) - used):
        print(f"note: stale allowlist entry {k[0]}|{k[1]}")
    if bad:
        print("\n".join(bad))
        print(f"check_dos_text: {len(bad)} DOS wording hit(s) in src/ (see tools/check_dos_text.py)")
        return 1
    print(f"check_dos_text: ok ({len(hits)} reviewed hits allowlisted)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
