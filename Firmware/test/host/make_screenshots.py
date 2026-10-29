# Converts the PPM screenshots to PNG and builds one contact sheet.
from PIL import Image, ImageDraw
import glob
files = sorted(glob.glob('/tmp/shot_*.ppm'))
imgs = [Image.open(f) for f in files]
labels = ['boot (stirred)', 'falling', 'settled', 'gentle shake', 'violent shake', 'energy decaying', 'resettled', 'graph view']
w, h = imgs[0].size
s = 0.62
tw, th = int(w * s), int(h * s)
sheet = Image.new('RGB', (4 * (tw + 10) + 10, 2 * (th + 30) + 10), (30, 30, 34))
d = ImageDraw.Draw(sheet)
for i, im in enumerate(imgs):
    x = 10 + (i % 4) * (tw + 10)
    y = 10 + (i // 4) * (th + 30)
    sheet.paste(im.resize((tw, th), Image.LANCZOS), (x, y + 18))
    d.text((x, y), labels[i], fill=(220, 220, 220))
sheet.save('/tmp/screens_sheet.png')
imgs[2].save('/tmp/screen_settled_full.png')
print('sheet saved', sheet.size)
