"""Create JPEG samples with varied component sampling and quality."""
import argparse
import struct
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    args.folder.mkdir(parents=True, exist_ok=True)
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, 3, 2, 24, 32)
    pixels = bytes((index * 31) & 255 for index in range(18))
    (args.folder / 'rgb.tga').write_bytes(header + pixels)
    for mode in ('L', 'RGB'):
        for sampling in (0, 1, 2):
            for quality in (35, 95):
                image = Image.new(mode, (19, 17))
                if mode == 'L':
                    image.putdata([(x * 29 + y * 37) & 255 for y in range(17) for x in range(19)])
                else:
                    image.putdata([tuple((x * 29 + y * 37 + c * 61) & 255 for c in range(3))
                                   for y in range(17) for x in range(19)])
                image.save(args.folder / f'{mode}_{sampling}_{quality}.jpg', quality=quality, subsampling=sampling)


if __name__ == '__main__':
    main()
