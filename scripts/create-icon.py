"""Generate Pulse's geometric metronome icon using Pillow (Linux only)."""
from pathlib import Path
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parents[1]
image = Image.new('RGBA', (1024, 1024))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((24, 24, 1000, 1000), radius=220, fill='#096b59')
draw.polygon([(440, 160), (584, 160), (778, 826), (246, 826)], fill='#eaf7ef')
draw.polygon([(478, 248), (546, 248), (680, 738), (344, 738)], fill='#153d39')
draw.line((506, 725, 713, 273), fill='#ffce70', width=42)
draw.rounded_rectangle((592, 394, 684, 464), radius=14, fill='#ffce70')
draw.ellipse((465, 675, 553, 763), fill='#ffce70')
image.save(root/'src/pulse.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
image.resize((256,256), Image.Resampling.LANCZOS).save(root/'docs/icon.png')
