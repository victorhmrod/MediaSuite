"""Gera assets/icon.ico: quadrado arredondado no azul do LocalFlix com uma seta de download."""

import os

from PIL import Image, ImageDraw

SIZE = 1024
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "icon.ico")


def main():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    # fundo com gradiente vertical (#749ff3 -> #3f6fd8), recortado num quadrado arredondado
    grad = Image.new("RGBA", (SIZE, SIZE))
    top, bottom = (0x74, 0x9F, 0xF3), (0x3F, 0x6F, 0xD8)
    gd = ImageDraw.Draw(grad)
    for y in range(SIZE):
        t = y / (SIZE - 1)
        gd.line([(0, y), (SIZE, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle([40, 40, SIZE - 40, SIZE - 40], radius=220, fill=255)
    img.paste(grad, (0, 0), mask)

    d = ImageDraw.Draw(img)
    white = (255, 255, 255, 255)
    cx = SIZE // 2
    d.rectangle([cx - 80, 230, cx + 80, 560], fill=white)  # haste
    d.polygon([(cx - 230, 500), (cx + 230, 500), (cx, 760)], fill=white)  # ponta
    d.rounded_rectangle([250, 800, SIZE - 250, 870], radius=35, fill=white)  # base

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT, sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    print("ícone:", OUT)


if __name__ == "__main__":
    main()
