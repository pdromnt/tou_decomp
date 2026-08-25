# Localizable menu sprite sources

These PNG files were extracted losslessly from `data/all3.gfx` with:

```powershell
python tools/extract_gfx_sprites.py 0x38 0x3C 0x3D 0x3E 0x3F --output artwork/menu-sprites
```

| File | Atlas ID | Baked text |
| --- | ---: | --- |
| `sprite-037.png` | `0x37` | In-match F10 menu |
| `sprite-038.png` | `0x38` | Back to the menu |
| `sprite-03C.png` | `0x3C` | Team stats |
| `sprite-03D.png` | `0x3D` | Awards |
| `sprite-03E.png` | `0x3E` | Player stats |
| `sprite-03F.png` | `0x3F` | Round-transition labels and instructions |

When cleaning these assets, preserve each PNG's exact canvas dimensions and
transparent background. Remove only the lettering; retain the button/panel
artwork beneath it. The cleaned sprites will become language-neutral backplates,
with localized text drawn by the game at runtime.

Import the finished backplates with:

```powershell
python tools/replace_gfx_sprites.py 0x37 0x38 0x3C 0x3D 0x3E 0x3F
```

The importer rejects dimension changes and safely converts Photoshop alpha to
the archive's original black color-key transparency.
