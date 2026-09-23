#!/usr/bin/env python3
"""Print one Ghidra-decompiled function without reading a 3 MB file.

Usage:
  python3 tools/decomp_fn.py FUN_521d_0a60 [FUN_...]     # print body (all sources that have it)
  python3 tools/decomp_fn.py --where FUN_521d_0a60       # file:line-range only
  python3 tools/decomp_fn.py --callers FUN_521d_0a60     # names of functions that mention it
  python3 tools/decomp_fn.py --index                     # (re)build cache only

Overlay export (viceroy_overlays.c) names functions FUN_0000_oooo; canonical
FUN_ssss_oooo names are resolved to it through tools/address_mapping.csv (exact rows).
Sources: original_sources_decompiled/{viceroy_unpacked,viceroy_unpacked_2,viceroy_overlays,mapedit}.c
Cache: build/decomp_fn_index.json (rebuilt when a source mtime changes).
Python 3 stdlib only.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = [os.path.join(ROOT, "original_sources_decompiled", f) for f in
       ("viceroy_unpacked.c", "viceroy_unpacked_2.c", "viceroy_overlays.c", "mapedit.c")]
CACHE = os.path.join(ROOT, "build", "decomp_fn_index.json")
MAPPING = os.path.join(ROOT, "tools", "address_mapping.csv")  # canonical FUN_ssss_oooo -> overlay FUN_0000_oooo
SIG = re.compile(r"^[A-Za-z_][\w *]*?\b(?:__cdecl16far|__cdecl16near|__cdecl|__fastcall|__stdcall)?\s*\*?\s*([A-Za-z_][\w:.]*)\s*\([^;]*\)\s*$")


def build_index():
    idx = {"mtimes": {}, "fns": {}, "alias": {}}
    if os.path.exists(MAPPING):
        idx["mtimes"][MAPPING] = os.path.getmtime(MAPPING)
        with open(MAPPING) as fh:
            for row in fh:
                c = row.rstrip("\n").split(",")
                if len(c) >= 6 and c[5] == "exact" and c[0] != c[4]:
                    idx["alias"][c[0]] = c[4]
    for path in SRC:
        if not os.path.exists(path):
            continue
        idx["mtimes"][path] = os.path.getmtime(path)
        with open(path, errors="replace") as fh:
            lines = fh.readlines()
        i = 0
        n = len(lines)
        while i < n:
            m = SIG.match(lines[i].rstrip("\n"))
            k = i + 1
            while k < n and lines[k].strip() == "":
                k += 1
            if m and k < n and lines[k].startswith("{"):
                # walk back over a comment block directly above the signature
                start = i
                while start > 0 and lines[start - 1].lstrip().startswith(("/*", "*", "//")) and not lines[start - 1].startswith("}"):
                    start -= 1
                j = k
                while j < n and not lines[j].startswith("}"):
                    j += 1
                idx["fns"].setdefault(m.group(1), []).append([path, start + 1, j + 1])
                i = j + 1
            else:
                i += 1
    os.makedirs(os.path.dirname(CACHE), exist_ok=True)
    with open(CACHE, "w") as fh:
        json.dump(idx, fh)
    return idx


def load_index():
    try:
        with open(CACHE) as fh:
            idx = json.load(fh)
        for path in SRC + [MAPPING]:
            if os.path.exists(path) and idx["mtimes"].get(path) != os.path.getmtime(path):
                raise ValueError
        return idx
    except (OSError, ValueError, KeyError):
        return build_index()


def body(path, a, b):
    with open(path, errors="replace") as fh:
        return "".join(fh.readlines()[a - 1:b])


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    mode = "body"
    if argv[0] in ("--where", "--callers", "--index"):
        mode = argv[0][2:]
        argv = argv[1:]
    if mode == "index":
        idx = build_index()
        print("indexed", sum(len(v) for v in idx["fns"].values()), "functions")
        return 0
    idx = load_index()
    rc = 0
    for name in argv:
        hits = list(idx["fns"].get(name, []))
        alias = idx["alias"].get(name)
        if alias:  # overlay export names the same body FUN_0000_oooo
            hits += [h for h in idx["fns"].get(alias, []) if "overlays" in h[0]]
        if not hits:
            print(f"{name}: not found (try grep -n {name} original_sources_decompiled/*.c)", file=sys.stderr)
            rc = 1
            continue
        for path, a, b in hits:
            rel = os.path.relpath(path, ROOT)
            if mode == "where":
                print(f"{name} {rel}:{a}-{b}")
            elif mode == "callers":
                pat = re.compile(r"\b" + re.escape(name) + r"\b")
                for fn, locs in idx["fns"].items():
                    if fn == name:
                        continue
                    for p2, a2, b2 in locs:
                        if p2 == path and pat.search(body(p2, a2, b2)):
                            print(f"{fn} {os.path.relpath(p2, ROOT)}:{a2}-{b2}")
                break
            else:
                print(f"/* {rel}:{a}-{b} */")
                print(body(path, a, b))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
