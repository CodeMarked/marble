import argparse
import re
from pathlib import Path

from pypdf import PdfReader


ROW_RE = re.compile(
    r"^\|\s*(?P<num>\d+)\s*\|\s*(?P<title>.*?)\s*\|\s*(?P<start>\d+)-(?P<end>\d+)\s*\|\s*`(?P<file>[^`]+)`\s*\|$"
)


def parse_index(index_path: Path):
    rows = []
    for line in index_path.read_text(encoding="utf-8").splitlines():
        m = ROW_RE.match(line.strip())
        if not m:
            continue
        rows.append(
            {
                "num": int(m.group("num")),
                "title": m.group("title"),
                "start": int(m.group("start")),
                "end": int(m.group("end")),
                "file": m.group("file"),
            }
        )
    return rows


def last_meaningful_line(text: str):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    return lines[-1] if lines else ""


def first_meaningful_line(text: str):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    return lines[0] if lines else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pdf", required=True, type=Path)
    parser.add_argument("--index", required=True, type=Path)
    args = parser.parse_args()

    rows = parse_index(args.index)
    reader = PdfReader(str(args.pdf))

    tiny = [r for r in rows if (r["end"] - r["start"] + 1) <= 2]
    print(f"Total chunks: {len(rows)}")
    print(f"Tiny chunks (<=2 pages): {len(tiny)}")
    print("")

    print("Likely risky boundaries (end appears incomplete):")
    flagged = 0
    for i in range(len(rows) - 1):
        a = rows[i]
        b = rows[i + 1]
        a_page = a["end"] - 1  # 1-based to 0-based
        b_page = b["start"] - 1
        a_text = reader.pages[a_page].extract_text() or ""
        b_text = reader.pages[b_page].extract_text() or ""
        a_last = last_meaningful_line(a_text)
        b_first = first_meaningful_line(b_text)
        if not a_last:
            continue
        # Heuristic: no terminal punctuation usually means spillover.
        if a_last[-1] not in ".!?\"')]}":
            flagged += 1
            print(
                f"- {a['num']:03d}->{b['num']:03d} pages {a['end']}->{b['start']}: "
                f"'{a_last[:80]}' || next: '{b_first[:80]}'"
            )

    if flagged == 0:
        print("- none by this heuristic")

    print("")
    print("Tiny chunks list:")
    for r in tiny:
        print(f"- {r['num']:03d} {r['title']} ({r['start']}-{r['end']})")


if __name__ == "__main__":
    main()
