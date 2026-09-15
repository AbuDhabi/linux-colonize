#!/usr/bin/env python3
"""
ai_logic_map.py — render / validate docs/ai_euro_logic_map.yaml.

The YAML is the machine-legible map of the European AI (see its header for
the schema). This tool turns it into something humans can look at and checks
it against the source tree so it does not rot silently.

Usage:
  tools/ai_logic_map.py check   [MAP]                 validate schema + fn refs
  tools/ai_logic_map.py html    [MAP] -o FILE [--no-lib]
                                                      one HTML page, all graphs
                                                      (Mermaid from cdnjs unless
                                                      --no-lib, e.g. for hosts
                                                      that render Mermaid natively)
  tools/ai_logic_map.py mermaid [MAP] -o DIR          one .mmd per graph
  tools/ai_logic_map.py dot     [MAP] -o DIR          one Graphviz .dot per graph
  tools/ai_logic_map.py outline [MAP] [GRAPH]         plain-text outline (terminal / LLM)

MAP defaults to docs/ai_euro_logic_map.yaml relative to the repo root.
Only dependency: PyYAML.
"""
import argparse
import os
import re
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("PyYAML is required: pip install pyyaml")

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_MAP = os.path.join(REPO, "docs", "ai_euro_logic_map.yaml")
SRC_DIRS = [os.path.join(REPO, "src", "core"), os.path.join(REPO, "src")]

KINDS = {"phase", "step", "decision", "loop", "call", "sub", "end"}
STATUSES = {"dos", "structural", "thin", "linux", "golden"}

STATUS_FILL = {
    "dos": ("#d8f5d0", "#2b8a3e"),
    "structural": ("#d0ebff", "#1971c2"),
    "thin": ("#fff3bf", "#e67700"),
    "linux": ("#e9ecef", "#495057"),
    "golden": ("#f3d9fa", "#9c36b5"),
    None: ("#ffffff", "#868e96"),
}


# ----------------------------------------------------------------- loading
def load_map(path):
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def graph_index(doc):
    return {g["id"]: g for g in doc.get("graphs", [])}


# ---------------------------------------------------------------- checking
def find_source_file(name):
    for d in SRC_DIRS:
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    return None


def check(doc, verbose=True):
    errors, warnings = [], []
    graphs = doc.get("graphs", [])
    if doc.get("schema") != "euro-ai-logic-map/1":
        errors.append("schema must be euro-ai-logic-map/1")
    gidx = graph_index(doc)
    if len(gidx) != len(graphs):
        errors.append("duplicate graph ids")
    src_cache = {}
    for g in graphs:
        gid = g.get("id", "?")
        nodes = g.get("nodes", [])
        ids = [n.get("id") for n in nodes]
        if len(set(ids)) != len(ids):
            errors.append(f"{gid}: duplicate node ids")
        idset = set(ids)
        if g.get("entry") not in idset:
            errors.append(f"{gid}: entry '{g.get('entry')}' is not a node")
        for n in nodes:
            nid = n.get("id")
            if n.get("kind") not in KINDS:
                errors.append(f"{gid}.{nid}: bad kind '{n.get('kind')}'")
            if "status" in n and n["status"] not in STATUSES:
                errors.append(f"{gid}.{nid}: bad status '{n['status']}'")
            if not n.get("label"):
                errors.append(f"{gid}.{nid}: missing label")
            if n.get("kind") == "sub":
                if n.get("sub") not in gidx:
                    errors.append(f"{gid}.{nid}: sub '{n.get('sub')}' is not a graph")
            elif "sub" in n:
                warnings.append(f"{gid}.{nid}: 'sub' on a non-sub node")
            fn = n.get("fn")
            if fn:
                if ":" not in fn:
                    errors.append(f"{gid}.{nid}: fn must be file:symbol ({fn})")
                else:
                    fname, sym = fn.split(":", 1)
                    path = find_source_file(fname)
                    if not path:
                        errors.append(f"{gid}.{nid}: fn file not found ({fname})")
                    else:
                        if path not in src_cache:
                            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                                src_cache[path] = fh.read()
                        if not re.search(r"\b" + re.escape(sym) + r"\b", src_cache[path]):
                            errors.append(f"{gid}.{nid}: symbol '{sym}' not in {fname}")
        # edges
        outgoing = {i: 0 for i in idset}
        incoming = {i: 0 for i in idset}
        for e in g.get("edges", []):
            if not (isinstance(e, list) and 2 <= len(e) <= 3):
                errors.append(f"{gid}: malformed edge {e}")
                continue
            a, b = e[0], e[1]
            for x in (a, b):
                if x not in idset:
                    errors.append(f"{gid}: edge references unknown node '{x}'")
            if a in outgoing:
                outgoing[a] += 1
            if b in incoming:
                incoming[b] += 1
        for n in nodes:
            nid, kind = n.get("id"), n.get("kind")
            if kind != "end" and outgoing.get(nid, 0) == 0:
                warnings.append(f"{gid}.{nid}: no outgoing edge")
            if nid != g.get("entry") and incoming.get(nid, 0) == 0:
                warnings.append(f"{gid}.{nid}: unreachable (no incoming edge)")
            if kind == "decision" and outgoing.get(nid, 0) < 2:
                warnings.append(f"{gid}.{nid}: decision with < 2 branches")
    if verbose:
        for w in warnings:
            print("warning:", w)
        for e in errors:
            print("error:", e)
        n_nodes = sum(len(g.get("nodes", [])) for g in graphs)
        n_edges = sum(len(g.get("edges", [])) for g in graphs)
        print(f"{len(graphs)} graphs, {n_nodes} nodes, {n_edges} edges, "
              f"{len(errors)} errors, {len(warnings)} warnings")
    return errors, warnings


# ----------------------------------------------------------------- mermaid
def mm_escape(text):
    return (str(text).replace("&", "#38;").replace('"', "#quot;")
            .replace("<", "#60;").replace(">", "#62;"))


def mm_label(n, with_fn=True):
    parts = [mm_escape(n["label"])]
    if with_fn and n.get("fn"):
        parts.append("<i>" + mm_escape(n["fn"].split(":", 1)[1]) + "</i>")
    if n.get("kind") == "sub":
        parts.append("<b>→ " + mm_escape(n.get("sub")) + "</b>")
    return "<br/>".join(parts)


def mm_shape(n, label):
    kind = n.get("kind")
    q = f'"{label}"'
    if kind == "decision":
        return "{" + q + "}"
    if kind == "loop":
        return "[/" + q + "/]"
    if kind in ("call", "sub"):
        return "[[" + q + "]]"
    if kind == "end":
        return "([" + q + "])"
    if kind == "phase":
        return "[" + q + "]"
    return "[" + q + "]"


def graph_to_mermaid(g, with_clicks=False):
    out = ["flowchart TD"]
    gid = g["id"]
    for n in g.get("nodes", []):
        nid = f"{gid}__{n['id']}"
        out.append(f"  {nid}{mm_shape(n, mm_label(n))}")
    for e in g.get("edges", []):
        a, b = f"{gid}__{e[0]}", f"{gid}__{e[1]}"
        if len(e) == 3 and e[2]:
            out.append(f'  {a} -->|"{mm_escape(e[2])}"| {b}')
        else:
            out.append(f"  {a} --> {b}")
    # styling by status / kind
    for n in g.get("nodes", []):
        nid = f"{gid}__{n['id']}"
        st = n.get("status")
        fill, stroke = STATUS_FILL.get(st, STATUS_FILL[None])
        extra = ",stroke-width:2px" if n.get("kind") == "phase" else ""
        if n.get("kind") == "sub":
            fill, stroke = "#fff", "#212529"
            extra = ",stroke-width:2px,stroke-dasharray:4 2"
        out.append(f"  style {nid} fill:{fill},stroke:{stroke},color:#212529{extra}")
        if with_clicks and n.get("kind") == "sub":
            out.append(f'  click {nid} "#graph-{n["sub"]}" "open {n["sub"]}"')
    return "\n".join(out) + "\n"


# --------------------------------------------------------------------- dot
def dot_escape(text):
    return str(text).replace("\\", "\\\\").replace('"', '\\"')


def graph_to_dot(g):
    out = [f'digraph "{dot_escape(g["id"])}" {{',
           "  rankdir=TB; node [fontname=\"Helvetica\", fontsize=10]; edge [fontname=\"Helvetica\", fontsize=9];",
           f'  label="{dot_escape(g.get("title", g["id"]))}"; labelloc=t;']
    shapes = {"decision": "diamond", "loop": "parallelogram", "call": "box3d",
              "sub": "folder", "end": "ellipse", "phase": "box", "step": "box"}
    for n in g.get("nodes", []):
        fill, stroke = STATUS_FILL.get(n.get("status"), STATUS_FILL[None])
        lab = n["label"]
        if n.get("fn"):
            lab += "\\n" + n["fn"].split(":", 1)[1]
        if n.get("kind") == "sub":
            lab += "\\n-> " + n.get("sub", "")
        out.append(f'  "{n["id"]}" [label="{dot_escape(lab)}", shape={shapes.get(n.get("kind"), "box")}, '
                   f'style="filled,rounded", fillcolor="{fill}", color="{stroke}"];')
    for e in g.get("edges", []):
        lab = f' [label="{dot_escape(e[2])}"]' if len(e) == 3 and e[2] else ""
        out.append(f'  "{e[0]}" -> "{e[1]}"{lab};')
    out.append("}")
    return "\n".join(out) + "\n"


# ------------------------------------------------------------------ outline
def outline(doc, only=None):
    lines = []
    for g in doc.get("graphs", []):
        if only and g["id"] != only:
            continue
        lines.append(f"== {g['id']}: {g.get('title', '')}")
        byid = {n["id"]: n for n in g.get("nodes", [])}
        succ = {}
        for e in g.get("edges", []):
            succ.setdefault(e[0], []).append((e[1], e[2] if len(e) == 3 else ""))
        for n in g.get("nodes", []):
            tag = f"[{n['kind']}" + (f"/{n['status']}" if n.get("status") else "") + "]"
            head = f"  {n['id']} {tag} {n['label']}"
            if n.get("fn"):
                head += f"  <{n['fn']}>"
            if n.get("sub"):
                head += f"  => graph {n['sub']}"
            lines.append(head)
            if n.get("dos"):
                lines.append(f"      dos: {n['dos']}")
            if n.get("note"):
                lines.append(f"      note: {n['note']}")
            for b, lab in succ.get(n["id"], []):
                lines.append(f"      -> {b}" + (f"  [{lab}]" if lab else ""))
        lines.append("")
    return "\n".join(lines)


# --------------------------------------------------------------------- html
HTML_HEAD = """<title>Euro AI Logic Map</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
  :root { --bg:#f3f5f7; --fg:#1f2933; --muted:#52606d; --card:#ffffff; --line:#d9dee3; --accent:#1c5d7a; --diagram:#ffffff; }
  @media (prefers-color-scheme: dark) {
    :root:not([data-theme="light"]) { --bg:#15181c; --fg:#e4e7eb; --muted:#9aa5b1; --card:#1e2227; --line:#323a43; --accent:#7cc0dd; --diagram:#f3f5f7; }
  }
  :root[data-theme="dark"] { --bg:#15181c; --fg:#e4e7eb; --muted:#9aa5b1; --card:#1e2227; --line:#323a43; --accent:#7cc0dd; --diagram:#f3f5f7; }
  body { background:var(--bg); color:var(--fg); font-family:"IBM Plex Sans", system-ui, -apple-system, "Segoe UI", sans-serif; padding-inline:20px; padding-block:24px; margin:0; line-height:1.45; }
  code, .mono { font-family:"IBM Plex Mono", ui-monospace, SFMono-Regular, Menlo, monospace; font-size:.92em; }
  h1 { font-size:1.5rem; font-weight:600; margin:0 0 4px; letter-spacing:-.01em; text-wrap:balance; }
  .sub { color:var(--muted); font-size:.9rem; margin-bottom:14px; max-width:70ch; }
  a { color:var(--accent); text-decoration:none; } a:hover { text-decoration:underline; } a:focus-visible { outline:2px solid var(--accent); outline-offset:2px; }
  nav { display:flex; flex-wrap:wrap; gap:6px 14px; margin:8px 0 14px; font-size:.88rem; }
  .legend { display:flex; flex-wrap:wrap; gap:8px 16px; font-size:.78rem; color:var(--muted); margin-bottom:20px; text-transform:uppercase; letter-spacing:.04em; }
  .legend span { display:inline-flex; align-items:center; gap:6px; }
  .legend i { display:inline-block; width:12px; height:12px; border-radius:2px; border:1px solid #868e96; }
  section { background:var(--card); border:1px solid var(--line); border-radius:6px; padding:14px 16px; margin-bottom:22px; }
  section h2 { font-size:1.05rem; font-weight:600; margin:0 0 4px; text-wrap:balance; }
  section .meta { color:var(--muted); font-size:.8rem; margin-bottom:10px; }
  .diagram { overflow-x:auto; }
  pre.mermaid { margin:0; background:var(--diagram); border-radius:4px; padding:8px; color:#1f2933; }
  details { font-size:.85rem; margin-top:10px; } summary { cursor:pointer; color:var(--muted); }
  details table { border-collapse:collapse; font-size:.8rem; margin-top:6px; }
  details td { border-top:1px solid var(--line); padding:4px 10px 4px 0; vertical-align:top; }
  details td:first-child { white-space:nowrap; font-family:"IBM Plex Mono", ui-monospace, monospace; }
  .top { font-size:.78rem; margin-top:8px; }
  @media (prefers-reduced-motion: no-preference) { html { scroll-behavior:smooth; } }
</style>
"""


def html_escape(t):
    return (str(t).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def render_html(doc, with_lib=True):
    gidx = graph_index(doc)
    parts = [HTML_HEAD, "<a id='top'></a>", f"<h1>{html_escape(doc.get('title', 'Euro AI logic map'))}</h1>",
             f"<div class='sub'>Generated from docs/ai_euro_logic_map.yaml ({html_escape(doc.get('date', ''))}) "
             f"by tools/ai_logic_map.py. Start at <a href='#graph-{doc['graphs'][0]['id']}'>"
             f"{html_escape(doc['graphs'][0]['id'])}</a> and follow the dashed “→ graph” boxes.</div>"]
    parts.append("<nav>" + " ".join(
        f"<a href='#graph-{g['id']}'>{html_escape(g['id'])}</a>" for g in doc["graphs"]) + "</nav>")
    parts.append("<div class='legend'>" + "".join(
        f"<span><i style='background:{STATUS_FILL[s][0]};border-color:{STATUS_FILL[s][1]}'></i>{lab}</span>"
        for s, lab in [("dos", "DOS-literal"), ("structural", "structural port"), ("thin", "thin approximation"),
                       ("linux", "Linux-only"), ("golden", "golden-fit")]) +
        "<span><i style='border:2px dashed #212529'></i>jump to another graph</span>"
        "<span>◇ decision · ▱ loop · ⟦ ⟧ call · ( ) exit</span></div>")
    for g in doc["graphs"]:
        parts.append(f"<section id='graph-{g['id']}'><h2>{html_escape(g.get('title', g['id']))}</h2>")
        meta = [f"id <code>{g['id']}</code>", f"entry <code>{g.get('entry')}</code>"]
        callers = [h["id"] for h in doc["graphs"] for n in h.get("nodes", [])
                   if n.get("kind") == "sub" and n.get("sub") == g["id"]]
        if callers:
            meta.append("reached from " + ", ".join(f"<a href='#graph-{c}'>{c}</a>" for c in sorted(set(callers))))
        subs = sorted({n["sub"] for n in g.get("nodes", []) if n.get("kind") == "sub"})
        if subs:
            meta.append("opens " + ", ".join(f"<a href='#graph-{s}'>{s}</a>" for s in subs))
        parts.append(f"<div class='meta'>{' · '.join(meta)}</div>")
        parts.append("<div class='diagram'><pre class='mermaid'>" +
                     html_escape(graph_to_mermaid(g, with_clicks=with_lib)) + "</pre></div>")
        rows = []
        for n in g.get("nodes", []):
            bits = []
            if n.get("fn"):
                bits.append("<code>" + html_escape(n["fn"]) + "</code>")
            if n.get("dos"):
                bits.append("DOS: " + html_escape(n["dos"]))
            if n.get("note"):
                bits.append(html_escape(n["note"]))
            rows.append(f"<tr><td>{html_escape(n['id'])}</td><td>{html_escape(n['label'])}</td>"
                        f"<td>{'<br/>'.join(bits)}</td></tr>")
        parts.append("<details><summary>Node notes and citations</summary><table>" +
                     "".join(rows) + "</table></details>")
        parts.append("<div class='top'><a href='#top'>top</a></div></section>")
    if with_lib:
        parts.append(
            '<script src="https://cdnjs.cloudflare.com/ajax/libs/mermaid/11.4.0/mermaid.min.js"></script>\n'
            "<script>mermaid.initialize({startOnLoad:true, securityLevel:'loose', maxTextSize:1000000, "
            "maxEdges:5000, flowchart:{useMaxWidth:false, htmlLabels:true}});</script>\n")
        # Standalone file for a local browser; --no-lib emits a body fragment
        # for hosts that wrap the page and render Mermaid themselves.
        return ('<!doctype html>\n<html lang="en"><head><meta charset="utf-8">'
                '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
                + "\n".join(parts) + "\n</head><body></body></html>\n").replace(
                    "</style>\n", "</style>\n</head><body>\n", 1).replace(
                    "\n</head><body></body></html>\n", "\n</body></html>\n")
    return "\n".join(parts) + "\n"


# -------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["check", "html", "mermaid", "dot", "outline"])
    ap.add_argument("map", nargs="?", default=DEFAULT_MAP)
    ap.add_argument("graph", nargs="?", help="outline: restrict to one graph id")
    ap.add_argument("-o", "--out", help="html: output file; mermaid/dot: output directory")
    ap.add_argument("--no-lib", action="store_true", help="html: omit the Mermaid <script> (host renders natively)")
    args = ap.parse_args(argv)

    doc = load_map(args.map)
    errors, _ = check(doc, verbose=(args.command == "check"))
    if args.command == "check":
        return 1 if errors else 0
    if errors:
        for e in errors:
            print("error:", e, file=sys.stderr)
        return 1

    if args.command == "outline":
        sys.stdout.write(outline(doc, args.graph))
        return 0
    if args.command == "html":
        out = args.out or os.path.join(REPO, "docs", "diagrams", "ai_euro_logic.html")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as fh:
            fh.write(render_html(doc, with_lib=not args.no_lib))
        print("wrote", out)
        return 0
    outdir = args.out or os.path.join(REPO, "docs", "diagrams", "ai_euro_logic")
    os.makedirs(outdir, exist_ok=True)
    for g in doc["graphs"]:
        ext = "mmd" if args.command == "mermaid" else "dot"
        p = os.path.join(outdir, f"{g['id']}.{ext}")
        with open(p, "w", encoding="utf-8") as fh:
            fh.write(graph_to_mermaid(g) if args.command == "mermaid" else graph_to_dot(g))
        print("wrote", p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
