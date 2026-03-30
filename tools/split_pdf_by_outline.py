import argparse
import re
from pathlib import Path


def _load_pdf_lib():
    try:
        from pypdf import PdfReader, PdfWriter  # type: ignore

        def get_page_number(reader, destination):
            return reader.get_destination_page_number(destination)

        return PdfReader, PdfWriter, get_page_number
    except Exception:
        from PyPDF2 import PdfReader, PdfWriter  # type: ignore

        def get_page_number(reader, destination):
            return reader.get_destination_page_number(destination)

        return PdfReader, PdfWriter, get_page_number


def sanitize(name: str) -> str:
    name = re.sub(r"[\\/:*?\"<>|]+", "", name).strip()
    name = re.sub(r"\s+", "-", name)
    return name[:100] if name else "untitled"


def flatten_bookmarks(reader, get_page_number):
    raw = getattr(reader, "outline", None)
    if raw is None:
        raw = getattr(reader, "outlines", [])

    bookmarks = []
    def walk(nodes, depth):
        for node in nodes:
            if isinstance(node, list):
                walk(node, depth + 1)
                continue
            title = getattr(node, "title", None)
            if not title:
                continue
            try:
                page = get_page_number(reader, node)
            except Exception:
                continue
            bookmarks.append((str(title).strip(), int(page), depth))

    walk(raw, 0)
    return bookmarks


def dedupe_by_page(items):
    dedup = []
    seen_pages = set()
    for title, page in sorted(items, key=lambda x: x[1]):
        if page in seen_pages:
            continue
        seen_pages.add(page)
        dedup.append((title, page))
    return dedup


def get_top_level_bookmarks(reader, get_page_number):
    flat = flatten_bookmarks(reader, get_page_number)
    top = [(t, p) for t, p, d in flat if d == 0]
    return dedupe_by_page(top)


def get_chapter_like_bookmarks(reader, get_page_number):
    flat = flatten_bookmarks(reader, get_page_number)
    chapter_pattern = re.compile(r"^(chapter\s+\d+|\d+[\.:]|\d+\s)", re.IGNORECASE)
    filtered = []
    for title, page, _depth in flat:
        clean = title.strip()
        if chapter_pattern.match(clean):
            filtered.append((clean, page))

    return dedupe_by_page(filtered)


def write_chunk(reader, start_page, end_page, out_file, PdfWriter):
    writer = PdfWriter()
    for p in range(start_page, end_page + 1):
        writer.add_page(reader.pages[p])
    with out_file.open("wb") as f:
        writer.write(f)


def main():
    parser = argparse.ArgumentParser(description="Split PDF by top-level outline/bookmarks.")
    parser.add_argument("pdf_path", type=Path, help="Path to source PDF")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("docs/book/chunks"),
        help="Output directory for chapter chunks",
    )
    parser.add_argument(
        "--mode",
        choices=["top-level", "chapter-like"],
        default="chapter-like",
        help="Bookmark selection mode",
    )
    parser.add_argument(
        "--list-outline",
        action="store_true",
        help="Only print flattened outline and exit",
    )
    args = parser.parse_args()

    PdfReader, PdfWriter, get_page_number = _load_pdf_lib()
    pdf_path = args.pdf_path.resolve()
    out_dir = args.output_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    reader = PdfReader(str(pdf_path))
    total_pages = len(reader.pages)

    if args.list_outline:
        for title, page, depth in flatten_bookmarks(reader, get_page_number):
            print(f"{depth}|{page + 1}|{title}")
        return

    if args.mode == "top-level":
        bookmarks = get_top_level_bookmarks(reader, get_page_number)
    else:
        bookmarks = get_chapter_like_bookmarks(reader, get_page_number)

    if not bookmarks:
        raise RuntimeError(f"No bookmarks found for mode: {args.mode}")

    index_lines = [
        "# Game Engine Architecture 3e - Chapter Chunks",
        "",
        f"Source: `{pdf_path}`",
        f"Total pages: {total_pages}",
        "",
        "| Chunk | Title | Pages (1-based) | File |",
        "|---|---|---:|---|",
    ]

    chunk_num = 1

    first_start = bookmarks[0][1]
    if first_start > 0:
        front_file = out_dir / "chunk-000-front-matter.pdf"
        write_chunk(reader, 0, first_start - 1, front_file, PdfWriter)
        index_lines.append(
            f"| 000 | Front Matter | 1-{first_start} | `{front_file.name}` |"
        )

    for i, (title, start) in enumerate(bookmarks):
        end = (bookmarks[i + 1][1] - 1) if i + 1 < len(bookmarks) else (total_pages - 1)
        safe_title = sanitize(title)
        filename = f"chunk-{chunk_num:03d}-{safe_title}.pdf"
        out_file = out_dir / filename
        write_chunk(reader, start, end, out_file, PdfWriter)
        index_lines.append(
            f"| {chunk_num:03d} | {title.replace('|', '/')} | {start + 1}-{end + 1} | `{filename}` |"
        )
        chunk_num += 1

    index_path = out_dir / "index.md"
    index_path.write_text("\n".join(index_lines), encoding="utf-8")
    print(f"Wrote {chunk_num - 1} chunks to: {out_dir}")
    print(f"Index: {index_path}")


if __name__ == "__main__":
    main()
