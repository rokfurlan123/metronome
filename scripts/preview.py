"""Convert the native smoke test's 32-bit BMP capture to a portable PNG."""
import pathlib
import struct
import zlib

root = pathlib.Path(__file__).resolve().parent.parent
for path in (root / "docs").glob("*.bmp"):
    bmp = path.read_bytes()
    offset = struct.unpack_from("<I", bmp, 10)[0]
    width, height = struct.unpack_from("<ii", bmp, 18)
    rows = []
    data = bmp[offset:]
    for y in range(abs(height)):
        row = data[y * width * 4:(y + 1) * width * 4]
        rgb = bytearray()
        for x in range(width):
            blue, green, red, _ = row[x * 4:x * 4 + 4]
            rgb.extend((red, green, blue))
        rows.append(b"\0" + rgb)
    if height > 0:
        rows.reverse()
    def chunk(kind, value):
        return struct.pack(">I", len(value)) + kind + value + struct.pack(">I", zlib.crc32(kind + value) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, abs(height), 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b"")
    path.with_suffix(".png").write_bytes(png)
