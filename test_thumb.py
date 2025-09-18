from thumbnail_worker import ThumbnailWorker
from pathlib import Path
p = Path(r"C:\Windows\Web\Wallpaper\Windows\img0.jpg")
if p.exists():
    w = ThumbnailWorker()
    b = w.generate_thumbnail(p)
    print('type', type(b), 'len', len(b))
else:
    print('test image not found')
