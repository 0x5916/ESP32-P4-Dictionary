import struct
import math

# dataset is from https://github.com/hackerb9/gwordlist/blob/master/frequency-alpha-alldicts.txt

RECORD_SIZE = 16 + 3  # 16 bytes for word (padded), 1 byte for length, 2 bytes for rank
INPUT  = "frequency-alpha-alldicts.txt"
OUTPUT = "wordlist.bin"

entries = []
with open(INPUT, encoding="utf-8") as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) < 3:
            continue
        word, freq_str = parts[1], parts[2]

        if not word.isalpha(): continue
        if not (2 <= len(word) <= 16): continue

        freq = int(freq_str.replace(",", ""))
        if freq < 500:
            continue

        entries.append((word.lower(), freq))

entries.sort(key=lambda x: x[0])
print(f"Total entries: {len(entries)}")  # expect ~90K-120K

# Log-normalize frequency to rank 1..65535
max_log = math.log(max(f for _, f in entries) + 1)
ranked = []
for word, freq in entries:
    # Higher freq → lower rank number (better)
    norm = math.log(freq + 1) / max_log          # 0.0 → 1.0
    rank = int((1.0 - norm) * 65534) + 1         # 1 → 65535
    ranked.append((word, rank))

with open(OUTPUT, "wb") as f:
    # Write header: record count as uint32
    f.write(struct.pack("<I", len(ranked)))
    for word, rank in ranked:
        word_bytes = word.encode("ascii")[:16].ljust(16, b'\x00')
        f.write(word_bytes)
        f.write(struct.pack("B", len(word)))    # uint8_t length
        f.write(struct.pack("<H", rank))         # uint16_t rank
