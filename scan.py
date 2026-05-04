from pathlib import Path

root = Path("./main")
output_file = Path("all_c_h_files.txt")

if not root.exists() or not root.is_dir():
    raise SystemExit(f"Directory not found: {root.resolve()}")

files = sorted(
    [p for p in root.rglob("*") if p.is_file() and p.suffix.lower() in {".c", ".h"}],
    key=lambda p: str(p).lower()
)

with output_file.open("w", encoding="utf-8", errors="replace") as out:
    for i, path in enumerate(files):
        out.write(f"{path.as_posix()}:\n")
        content = path.read_text(encoding="utf-8", errors="replace")
        out.write(content)
        if not content.endswith("\n"):
            out.write("\n")
        if i != len(files) - 1:
            out.write("\n")
