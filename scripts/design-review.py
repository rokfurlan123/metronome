"""Arrange unaltered native layout captures for side-by-side design review."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent.parent / "docs/design-review"
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20)
small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
for name in ("rhythm", "sequence", "tools"):
    sheet = Image.new("RGB", (1848, 698), "#202733")
    draw = ImageDraw.Draw(sheet)
    for i, (folder, label) in enumerate((("before", "BEFORE / Pulse 0.3.1"), ("after", "AFTER / local review build"))):
        x = 16 + i * 916
        draw.text((x, 15), label, font=font, fill="#edf2f7")
        sheet.paste(Image.open(root / folder / f"{name}.png"), (x, 52))
    draw.text((16, 672), "Native Wine client-area captures; child controls composited. Not a physical Windows screenshot.", font=small, fill="#b6c3d2")
    sheet.save(root / f"{name}-comparison.png")
sheet = Image.new("RGB", (1128, 498), "#202733")
draw = ImageDraw.Draw(sheet)
for i, (folder, label) in enumerate((("before", "BEFORE / Pulse 0.3.1"), ("after", "AFTER / local review build"))):
    x = 16 + i * 556
    draw.text((x, 14), label, font=font, fill="#edf2f7")
    draw.text((x, 50), "TINY BAR", font=small, fill="#b6c3d2")
    sheet.paste(Image.open(root / folder / "tiny.png"), (x, 78))
    draw.text((x, 152), "COMPACT OVERLAY", font=small, fill="#b6c3d2")
    sheet.paste(Image.open(root / folder / "compact.png"), (x, 180))
draw.text((16, 476), "Native Wine layout captures; no Windows installation changes.", font=small, fill="#b6c3d2")
sheet.save(root / "overlays-comparison.png")
