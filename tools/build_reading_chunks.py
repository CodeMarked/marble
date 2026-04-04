import argparse
import re
from pathlib import Path

from pypdf import PdfReader, PdfWriter


ROW_RE = re.compile(
    r"^\|\s*(?P<num>\d+)\s*\|\s*(?P<title>.*?)\s*\|\s*(?P<start>\d+)-(?P<end>\d+)\s*\|\s*`(?P<file>[^`]+)`\s*\|$"
)


def parse_index(path: Path):
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
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


def is_major(title: str) -> bool:
    t = title.strip()
    return bool(re.match(r"^\d+\s", t))


def write_pdf(reader: PdfReader, start_1: int, end_1: int, out_path: Path):
    w = PdfWriter()
    for p in range(start_1 - 1, end_1):
        w.add_page(reader.pages[p])
    with out_path.open("wb") as f:
        w.write(f)


def slug(title: str) -> str:
    cleaned = re.sub(r"[\\/:*?\"<>|]+", "", title).strip()
    cleaned = re.sub(r"\s+", "-", cleaned)
    return cleaned[:80] if cleaned else "chunk"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pdf", required=True, type=Path)
    parser.add_argument("--index", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--min-pages", type=int, default=8)
    parser.add_argument("--max-pages", type=int, default=28)
    parser.add_argument("--overlap", type=int, default=1)
    args = parser.parse_args()

    rows = parse_index(args.index)
    if not rows:
        raise RuntimeError("No rows parsed from index.")

    reader = PdfReader(str(args.pdf))
    total_pages = len(reader.pages)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    groups = []
    i = 0
    while i < len(rows):
        start_i = i
        start_p = rows[i]["start"]
        end_p = rows[i]["end"]
        i += 1

        while i < len(rows):
            candidate = rows[i]
            candidate_len = candidate["end"] - candidate["start"] + 1
            curr_len = end_p - start_p + 1
            # Keep groups coherent and readable.
            if is_major(candidate["title"]) and curr_len >= args.min_pages:
                break
            if curr_len < args.min_pages:
                end_p = candidate["end"]
                i += 1
                continue
            if curr_len + candidate_len <= args.max_pages:
                end_p = candidate["end"]
                i += 1
                continue
            break

        groups.append((start_i, i - 1, start_p, end_p))

    index_lines = [
        "# Game Engine Architecture 3e - Reading Chunks",
        "",
        f"Source: `{args.pdf.resolve()}`",
        f"Total pages: {total_pages}",
        "",
        f"Rules: merge to {args.min_pages}-{args.max_pages} pages, {args.overlap}-page overlap.",
        "",
        "| Chunk | Content | Base Pages | With Overlap | File |",
        "|---|---|---:|---:|---|",
    ]

    for n, (a, b, start_p, end_p) in enumerate(groups, start=1):
        out_start = max(1, start_p - args.overlap)
        out_end = min(total_pages, end_p + args.overlap)
        titles = rows[a]["title"] if a == b else f"{rows[a]['title']} -> {rows[b]['title']}"
        filename = f"reading-{n:03d}-{slug(rows[a]['title'])}.pdf"
        out_path = args.output_dir / filename
        write_pdf(reader, out_start, out_end, out_path)
        index_lines.append(
            f"| {n:03d} | {titles.replace('|', '/')} | {start_p}-{end_p} | {out_start}-{out_end} | `{filename}` |"
        )

    (args.output_dir / "index.md").write_text("\n".join(index_lines), encoding="utf-8")
    print(f"Wrote {len(groups)} reading chunks to: {args.output_dir.resolve()}")
    print(f"Index: {(args.output_dir / 'index.md').resolve()}")


if __name__ == "__main__":
    main()
