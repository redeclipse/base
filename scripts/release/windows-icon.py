#!/usr/bin/env python3
"""Generate the Windows client resource icon from the supplied PNG."""

from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[2]
output = root / ".csgopen/windows-resources"
output.mkdir(parents=True, exist_ok=True)
icon = output / "EclipseRecoil.ico"
with Image.open(root / "data/csgopen/branding/icon.png") as image:
    image.save(icon, format="ICO", sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
resource = (root / "src/redeclipse.rc").read_text()
resource = resource.replace('ICON "redeclipse.ico"', f'ICON "{icon.as_posix()}"')
(output / "eclipse-recoil.rc").write_text(resource)
