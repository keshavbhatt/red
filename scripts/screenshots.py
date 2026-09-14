#!/usr/bin/env python3
"""Composes the marketing screenshots and the banner from raw window grabs.

    scripts/screenshots.py <raw-dir>   # writes screenshots/*.png and screenshots/store/*.png

Raw grabs (RED_DEBUG_GRAB output) expected in <raw-dir>:
    desktop.png  tv.png  downloads.png  download-dialog.png  blocking.png
Style: Red's brand gradient with soft circles, a headline, one-line subtitle,
the window framed with rounded corners and a shadow (matches whatsie's set).
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT_BOLD = "/usr/share/fonts/ubuntu/Ubuntu-B.ttf"
FONT_REG = "/usr/share/fonts/ubuntu/Ubuntu-R.ttf"
FONT_MED = "/usr/share/fonts/ubuntu/Ubuntu-M.ttf"
LOGO = ROOT / "src/resources/icons/hicolor/512x512/apps/com.ktechpit.red.png"

TOP = (0x8B, 0x0A, 0x0A)     # deep YouTube red
BOTTOM = (0x2A, 0x05, 0x08)  # towards the dark ground
ACCENT = (0xFF, 0x00, 0x00)


def font(path, size):
    return ImageFont.truetype(path, size)


def background(w, h):
    """Vertical red gradient with two soft, translucent circles."""
    img = Image.new("RGB", (w, h), TOP)
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(TOP[i] * (1 - t) + BOTTOM[i] * t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    r = int(h * 0.55)
    od.ellipse((w - r * 1.1, -r * 0.5, w - r * 1.1 + r * 1.6, -r * 0.5 + r * 1.6), fill=(255, 255, 255, 14))
    od.ellipse((-r * 0.6, h - r * 0.9, -r * 0.6 + r * 1.4, h - r * 0.9 + r * 1.4), fill=(255, 255, 255, 12))
    img = Image.alpha_composite(img.convert("RGBA"), overlay)
    return img


def framed(shot, width, radius=14):
    """The grab scaled to `width`, rounded, on a soft shadow (RGBA)."""
    scale = width / shot.width
    shot = shot.convert("RGB").resize((width, int(shot.height * scale)), Image.LANCZOS)
    mask = Image.new("L", shot.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, shot.width - 1, shot.height - 1), radius=radius, fill=255)
    pad = 60
    canvas = Image.new("RGBA", (shot.width + pad * 2, shot.height + pad * 2), (0, 0, 0, 0))
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (pad, pad + 18, pad + shot.width, pad + shot.height + 18), radius=radius, fill=(0, 0, 0, 150))
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    canvas = Image.alpha_composite(canvas, shadow)
    rounded = Image.new("RGBA", shot.size, (0, 0, 0, 0))
    rounded.paste(shot, (0, 0), mask)
    # a hairline so a dark window separates from the dark ground
    ImageDraw.Draw(rounded).rounded_rectangle((0, 0, shot.width - 1, shot.height - 1), radius=radius,
                                              outline=(255, 255, 255, 40))
    canvas.alpha_composite(rounded, (pad, pad))
    return canvas


def text_centered(draw, y, text, fnt, fill, w):
    tw = draw.textlength(text, font=fnt)
    draw.text(((w - tw) / 2, y), text, font=fnt, fill=fill)


def compose(shot, headline, subtitle, size=(1280, 800), top=64):
    w, h = size
    img = background(w, h)
    d = ImageDraw.Draw(img)
    hf = font(FONT_BOLD, int(h * 0.062))
    sf = font(FONT_REG, int(h * 0.027))
    text_centered(d, top, headline, hf, (255, 255, 255), w)
    text_centered(d, top + int(h * 0.085), subtitle, sf, (255, 220, 220), w)
    # Fit the window into the box under the text, keeping its aspect ratio.
    box_top = top + int(h * 0.15)
    box_w, box_h = int(w * 0.80), h - box_top - int(h * 0.05)
    scale = min(box_w / shot.width, box_h / shot.height)
    fr = framed(shot, int(shot.width * scale))
    inner_h = int(shot.height * scale)
    y = box_top + (box_h - inner_h) // 2 - 60  # 60 = frame padding
    img.alpha_composite(fr, ((w - fr.width) // 2, y))
    return img.convert("RGB")


def banner(dialog, size=(2160, 720)):
    w, h = size
    img = background(w, h)
    d = ImageDraw.Draw(img)
    logo = Image.open(LOGO).convert("RGBA").resize((150, 150), Image.LANCZOS)
    x0 = 150
    img.alpha_composite(logo, (x0, 118))
    d.text((x0 + 180, 96), "Red", font=font(FONT_BOLD, 150), fill=(255, 255, 255))
    d.text((x0, 300), "YouTube, in a real desktop app.", font=font(FONT_REG, 54), fill=(255, 255, 255))
    d.text((x0, 380), "Native and lightweight.",
           font=font(FONT_REG, 30), fill=(255, 200, 200))
    # Translucent pills need their own layer: ImageDraw writes alpha, it does not blend.
    pf = font(FONT_MED, 27)
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    x = x0
    for pill in ("TV mode", "Music mode", "Ad & sponsor blocking", "Downloads", "Media keys"):
        tw = ld.textlength(pill, font=pf)
        ld.rounded_rectangle((x, 470, x + tw + 56, 528), radius=29, fill=(255, 255, 255, 40))
        ld.text((x + 28, 483), pill, font=pf, fill=(255, 255, 255, 255))
        x += tw + 56 + 22
    img = Image.alpha_composite(img, layer)
    fr = framed(dialog, 640, radius=16)
    img.alpha_composite(fr, (w - fr.width - 40, (h - fr.height) // 2))
    return img.convert("RGB")


def main(raw):
    raw = Path(raw)
    out = ROOT / "screenshots"
    store = out / "store"
    store.mkdir(parents=True, exist_ok=True)
    shots = {k: Image.open(raw / f"{k}.png") for k in
             ("desktop", "tv", "downloads", "download-dialog", "blocking")}
    plan = [
        ("00-hero", "desktop", "YouTube, in a real desktop app", "Fast, native, ad-free: YouTube Music, media keys, a mini player and downloads built in."),
        ("01-tv", "tv", "Lean back", "YouTube's living-room interface, driven by keyboard, mouse or gamepad. Ctrl+T switches any time."),
        ("02-downloads", "downloads", "Download anything", "Videos, audio, playlists and channels, queued beside the page, with your sign-in for restricted videos."),
        ("03-download-dialog", "download-dialog", "Video, audio, or the exact streams", "Quality presets, subtitles, chapters and cover art embedded. Playlists are detected from any link."),
        ("04-blocking", "blocking", "Ads gone, sponsors skipped", "Ads stripped before YouTube renders them, SponsorBlock per category, dislikes back, Shorts hidden."),
    ]
    for name, key, head, sub in plan:
        compose(shots[key], head, sub).save(out / f"{name}.png", optimize=True)
        compose(shots[key], head, sub, size=(1600, 1000)).save(store / f"{name}.png", optimize=True)
    banner(shots["download-dialog"]).save(out / "banner.png", optimize=True)
    for f in sorted(out.glob("*.png")) + sorted(store.glob("*.png")):
        print(f.relative_to(ROOT), Image.open(f).size, f.stat().st_size // 1024, "KB")


if __name__ == "__main__":
    main(sys.argv[1])
