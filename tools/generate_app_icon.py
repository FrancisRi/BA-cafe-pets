"""Rasterize the simple repository SVG shapes into a multi-resolution Windows icon."""
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
svg = ET.parse(root / "assets/app-icon.svg").getroot()
scale = 16
image = Image.new("RGBA", (64 * scale, 64 * scale))
draw = ImageDraw.Draw(image)
for shape in svg:
    kind = shape.tag.rsplit("}", 1)[-1]
    values = shape.attrib
    number = lambda key: float(values[key]) * scale
    fill = None if values.get("fill") == "none" else values.get("fill")
    if kind == "rect":
        x, y = number("x"), number("y")
        draw.rounded_rectangle((x, y, x + number("width"), y + number("height")), number("rx"), fill=fill)
    elif kind in ("ellipse", "circle"):
        x, y = number("cx"), number("cy")
        rx, ry = (number("r"), number("r")) if kind == "circle" else (number("rx"), number("ry"))
        draw.ellipse((x-rx, y-ry, x+rx, y+ry), fill=fill, outline=values.get("stroke"), width=round(float(values.get("stroke-width", 1))*scale))
    elif kind == "polygon":
        points = [tuple(float(value)*scale for value in pair.split(",")) for pair in values["points"].split()]
        draw.polygon(points, fill=fill)
    else:
        raise ValueError("Unsupported icon shape: " + kind)
image.resize((256, 256), Image.Resampling.LANCZOS).save(root / "assets/app.ico", sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
