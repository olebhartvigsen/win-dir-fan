from PySide6.QtCore import QEvent
codes = [74, 78, 77, 24, 99, 207, 12, 25, 105]
rev = {}
for name in dir(QEvent):
    if not name[0].isupper():
        continue
    val = getattr(QEvent, name)
    # Qt enum members may expose .value
    try:
        v = getattr(val, 'value', None)
        if v is None:
            v = int(val)
        rev[int(v)] = name
    except Exception:
        continue

for c in codes:
    print(c, rev.get(c, '<unknown>'))
