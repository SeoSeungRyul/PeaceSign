from __future__ import annotations

import hashlib
import difflib
import json
import sys
import zipfile
from pathlib import Path

from docx import Document
from docx.oxml.ns import qn


def iter_blocks(doc):
    body = doc.element.body
    for child in body.iterchildren():
        if child.tag == qn("w:p"):
            texts = [n.text or "" for n in child.iter(qn("w:t"))]
            text = "".join(texts).strip()
            drawings = len(list(child.iter(qn("w:drawing")))) + len(list(child.iter(qn("w:pict"))))
            if text or drawings:
                yield {"kind": "p", "text": text, "drawings": drawings}
        elif child.tag == qn("w:tbl"):
            rows = []
            for tr in child.iterchildren(qn("w:tr")):
                cells = []
                for tc in tr.iterchildren(qn("w:tc")):
                    texts = [n.text or "" for n in tc.iter(qn("w:t"))]
                    cells.append("".join(texts).strip())
                rows.append(cells)
            yield {"kind": "table", "rows": rows}


def media(path):
    out = []
    with zipfile.ZipFile(path) as z:
        for name in sorted(n for n in z.namelist() if n.startswith("word/media/")):
            data = z.read(name)
            out.append({"name": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    return out


def main():
    payloads = []
    for src, dest in [(Path(sys.argv[1]), Path(sys.argv[3])), (Path(sys.argv[2]), Path(sys.argv[4]))]:
        doc = Document(src)
        payload = {
            "source": str(src),
            "sections": len(doc.sections),
            "blocks": list(iter_blocks(doc)),
            "media": media(src),
        }
        dest.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
        payloads.append(payload)

    def label(block):
        if block["kind"] == "p":
            return block["text"] if block["text"] else f"[그림 {block['drawings']}개]"
        return "[표] " + " | ".join(" / ".join(row) for row in block["rows"])

    old = [label(x) for x in payloads[0]["blocks"]]
    new = [label(x) for x in payloads[1]["blocks"]]
    matcher = difflib.SequenceMatcher(a=old, b=new, autojunk=False)
    report = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag != "equal":
            report.append({"tag": tag, "old_range": [i1, i2], "new_range": [j1, j2],
                           "old": old[i1:i2], "new": new[j1:j2]})
    Path(sys.argv[5]).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
