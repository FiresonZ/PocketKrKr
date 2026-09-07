#!/usr/bin/env python3
"""Regenerate PocketKrKr Android launcher icons at full density from the
1024px adaptive-foreground source, so the app icon is crisp instead of blurry.

- adaptive foregrounds: mdpi..xxxhdpi (108 / 162 / 216 / 324 / 432 px)
  -> preserves the original safe-zone composition (fish keeps margins).
- legacy ic_launcher.png: mdpi..xxxhdpi (48 / 72 / 96 / 144 / 192 px),
  fish slightly larger but centered on the light-pink background.
"""
from PIL import Image
import os

BASE = os.path.join(os.path.dirname(__file__), "..",
                    "apps/flutter_app/android/app/src/main/res")

SRC = os.path.join(BASE, "mipmap-anydpi-v26/ic_launcher_foreground.png")
BG_COLOR = (252, 235, 240, 255)  # #FCEBF0, matches ic_launcher_background.xml

# (density dir, foreground_px, legacy_px)
DENSITIES = [
    ("mdpi",        108,  48),
    ("hdpi",        162,  72),
    ("xhdpi",       216,  96),
    ("xxhdpi",      324, 144),
    ("xxxhdpi",     432, 192),
]


def center_paste(canvas, img):
    w, h = canvas.size
    x = (w - img.size[0]) // 2
    y = (h - img.size[1]) // 2
    canvas.alpha_composite(img, (x, y))


src = Image.open(SRC).convert("RGBA")  # 1024x1024, fish kept with safe margins

for den, fore_px, legacy_px in DENSITIES:
    # --- adaptive foreground: preserve safe-zone composition ---
    fg = src.resize((fore_px, fore_px), Image.LANCZOS)
    fg_path = os.path.join(BASE, f"mipmap-{den}/ic_launcher_foreground.png")
    fg.save(fg_path)
    print(f"adaptive foreground {den}: {fore_px}x{fore_px} -> {fg_path}")

    # --- legacy full icon: crop to ~86% center then scale so the fish looks
    #     a bit larger while still leaving a safe margin. ---
    crop = int(1024 * 0.86)
    left = (1024 - crop) // 2
    leg = src.crop((left, left, left + crop, left + crop)).resize(
        (legacy_px, legacy_px), Image.LANCZOS)
    canvas = Image.new("RGBA", (legacy_px, legacy_px), BG_COLOR)
    center_paste(canvas, leg)
    leg_path = os.path.join(BASE, f"mipmap-{den}/ic_launcher.png")
    canvas.convert("RGB").save(leg_path)
    print(f"legacy icon {den}: {legacy_px}x{legacy_px} -> {leg_path}")

# The 1024px raster is now redundant in the vector/XML-only folder;
# per-density foregrounds cover adaptive icons, so drop it to avoid ambiguity.
misplaced = os.path.join(BASE, "mipmap-anydpi-v26/ic_launcher_foreground.png")
if os.path.exists(misplaced):
    os.remove(misplaced)
    print("removed misplaced:", misplaced)

print("done")