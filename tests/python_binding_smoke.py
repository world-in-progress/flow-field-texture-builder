import pathlib
import struct
import tempfile
import zlib

import numpy as np

from flow_field_texture_builder import FlowFieldBuilder


def _read_rgba_png(path: pathlib.Path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"{path} is not a PNG")

    offset = 8
    width = height = None
    idat = bytearray()
    while offset < len(data):
        length = int.from_bytes(data[offset : offset + 4], "big")
        chunk_type = data[offset + 4 : offset + 8]
        chunk_data = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width = int.from_bytes(chunk_data[0:4], "big")
            height = int.from_bytes(chunk_data[4:8], "big")
            bit_depth = chunk_data[8]
            color_type = chunk_data[9]
            if bit_depth != 8 or color_type != 6:
                raise AssertionError(f"{path} is not RGBA8")
        elif chunk_type == b"IDAT":
            idat.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise AssertionError(f"{path} has no IHDR")

    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    rows = []
    previous = bytearray(stride)
    cursor = 0
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        row = bytearray(raw[cursor : cursor + stride])
        cursor += stride
        for index in range(stride):
            left = row[index - 4] if index >= 4 else 0
            up = previous[index]
            up_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 1:
                row[index] = (row[index] + left) & 0xFF
            elif filter_type == 2:
                row[index] = (row[index] + up) & 0xFF
            elif filter_type == 3:
                row[index] = (row[index] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                predictor = _paeth(left, up, up_left)
                row[index] = (row[index] + predictor) & 0xFF
            elif filter_type != 0:
                raise AssertionError(f"unsupported PNG filter {filter_type}")
        rows.append(bytes(row))
        previous = row
    return width, height, rows


def _paeth(left: int, up: int, up_left: int) -> int:
    estimate = left + up - up_left
    left_distance = abs(estimate - left)
    up_distance = abs(estimate - up)
    up_left_distance = abs(estimate - up_left)
    if left_distance <= up_distance and left_distance <= up_left_distance:
        return left
    if up_distance <= up_left_distance:
        return up
    return up_left


def _pixel(rows, x: int, y: int):
    return rows[y][x * 4 : x * 4 + 4]


def _float32(pixel: bytes) -> float:
    return struct.unpack("<f", pixel)[0]


def _seed(pixel: bytes):
    return (pixel[0] * 256 + pixel[1], pixel[2] * 256 + pixel[3])


def main():
    xs = np.array([0.0, 1.0, 0.0, 1.0], dtype=np.float64)
    ys = np.array([0.0, 0.0, 1.0, 1.0], dtype=np.float64)
    us = np.array([10.0, 20.0, 30.0, 40.0], dtype=np.float64)
    vs = np.array([-1.0, -2.0, -3.0, -4.0], dtype=np.float64)

    with tempfile.TemporaryDirectory() as tmp:
        output_dir = pathlib.Path(tmp)
        builder = FlowFieldBuilder(output_dir)
        builder.set_domain_from_arrays(xs, ys)
        builder.set_quikgrid_options(density_ratio=150)
        builder.set_texture_size(2, 2)

        result = builder.build_uv_texture(us, vs, "step")
        assert result is None
        assert builder.texture_size == (2, 2)
        assert builder.domain_extent == (0.0, 0.0, 1.0, 1.0)

        uv_path = pathlib.Path(builder.uv_texture_path("step"))
        seed_path = pathlib.Path(builder.seed_texture_path("step"))
        assert uv_path == output_dir / "uv_step.png"
        assert seed_path == output_dir / "seed_step.png"

        uv_width, uv_height, uv_rows = _read_rgba_png(uv_path)
        assert (uv_width, uv_height) == (4, 2)
        assert _float32(_pixel(uv_rows, 0, 1)) == 10.0
        assert _float32(_pixel(uv_rows, 1, 1)) == -1.0
        assert _float32(_pixel(uv_rows, 2, 0)) == 40.0
        assert _float32(_pixel(uv_rows, 3, 0)) == -4.0

        seed_width, seed_height, seed_rows = _read_rgba_png(seed_path)
        assert (seed_width, seed_height) == (2, 2)
        assert _seed(_pixel(seed_rows, 0, 1)) == (0, 0)
        assert _seed(_pixel(seed_rows, 1, 0)) == (1, 1)

        builder.close()
        try:
            _ = builder.texture_size
        except RuntimeError as error:
            assert "closed" in str(error)
        else:
            raise AssertionError("closed builder did not reject texture_size access")


if __name__ == "__main__":
    main()
