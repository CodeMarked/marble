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
            }
        )
    return rows


def is_major(title: str) -> bool:
    return bool(re.match(r"^\d+\s", title.strip()))


def slug(title: str) -> str:
    cleaned = re.sub(r"[\\/:*?\"<>|]+", "", title).strip()
    cleaned = re.sub(r"\s+", "-", cleaned)
    return cleaned[:80] if cleaned else "chunk"


def first_meaningful_line(text: str):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    return lines[0] if lines else ""


def last_meaningful_line(text: str):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    return lines[-1] if lines else ""


def likely_needs_overlap(left_last: str, right_first: str) -> bool:
    if not left_last or not right_first:
        return False
    # Header-like starts usually don't need overlap.
    if re.match(r"^\d+(\.\d+)*\s", right_first):
        return False
    # If left side does not end with terminal punctuation, likely continuation.
    if left_last[-1] not in ".!?\"')]}":
        return True
    return False


def write_pdf(reader: PdfReader, start_1: int, end_1: int, out_path: Path):
    writer = PdfWriter()
    for p in range(start_1 - 1, end_1):
        writer.add_page(reader.pages[p])
    with out_path.open("wb") as f:
        writer.write(f)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pdf", required=True, type=Path)
    parser.add_argument("--index", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--min-pages", type=int, default=8)
    parser.add_argument("--max-pages", type=int, default=28)
    args = parser.parse_args()

    rows = parse_index(args.index)
    if not rows:
        raise RuntimeError("No rows parsed from index.")

    reader = PdfReader(str(args.pdf))
    total_pages = len(reader.pages)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Build merged reading groups first.
    groups = []
    i = 0
    while i < len(rows):
        a = i
        start_p = rows[i]["start"]
        end_p = rows[i]["end"]
        i += 1
        while i < len(rows):
            candidate = rows[i]
            curr_len = end_p - start_p + 1
            add_len = candidate["end"] - candidate["start"] + 1
            if is_major(candidate["title"]) and curr_len >= args.min_pages:
                break
            if curr_len < args.min_pages:
                end_p = candidate["end"]
                i += 1
                continue
            if curr_len + add_len <= args.max_pages:
                end_p = candidate["end"]
                i += 1
                continue
            break
        groups.append((a, i - 1, start_p, end_p))

    # Decide per-boundary whether overlap is needed.
    boundary_needs_overlap = {}
    for gi in range(len(groups) - 1):
        left = groups[gi]
        right = groups[gi + 1]
        left_end = left[3]
        right_start = right[2]
        left_text = reader.pages[left_end - 1].extract_text() or ""
        right_text = reader.pages[right_start - 1].extract_text() or ""
        boundary_needs_overlap[gi] = likely_needs_overlap(
            last_meaningful_line(left_text), first_meaningful_line(right_text)
        )

    index_lines = [
        "# Game Engine Architecture 3e - Reading Chunks (Smart Overlap)",
        "",
        f"Source: `{args.pdf.resolve()}`",
        f"Total pages: {total_pages}",
        "",
        f"Rules: merge to {args.min_pages}-{args.max_pages} pages; add overlap only on risky boundaries.",
        "",
        "| Chunk | Content | Base Pages | Output Pages | Overlap Added | File |",
        "|---|---|---:|---:|---|---|",
    ]

    overlaps_used = 0
    for n, (a, b, start_p, end_p) in enumerate(groups, start=1):
        add_prev = n > 1 and boundary_needs_overlap.get(n - 2, False)
        add_next = n < len(groups) and boundary_needs_overlap.get(n - 1, False)

        out_start = max(1, start_p - (1 if add_prev else 0))
        out_end = min(total_pages, end_p + (1 if add_next else 0))
        if add_prev:
            overlaps_used += 1
        if add_next:
            overlaps_used += 1

        title = rows[a]["title"] if a == b else f"{rows[a]['title']} -> {rows[b]['title']}"
        filename = f"reading-smart-{n:03d}-{slug(rows[a]['title'])}.pdf"
        out_path = args.output_dir / filename
        write_pdf(reader, out_start, out_end, out_path)
        overlap_note = []
        if add_prev:
            overlap_note.append("prev")
        if add_next:
            overlap_note.append("next")
        index_lines.append(
            f"| {n:03d} | {title.replace('|', '/')} | {start_p}-{end_p} | {out_start}-{out_end} | {','.join(overlap_note) if overlap_note else 'none'} | `{filename}` |"
        )

    (args.output_dir / "index.md").write_text("\n".join(index_lines), encoding="utf-8")
    print(f"Wrote {len(groups)} smart reading chunks to: {args.output_dir.resolve()}")
    print(f"Boundary overlaps added (counting both sides): {overlaps_used}")
    print(f"Index: {(args.output_dir / 'index.md').resolve()}")


if __name__ == "__main__":
    main()
