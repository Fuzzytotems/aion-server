"""PNG reading for the terrain files: the raw samples Java's ImageIO puts into the raster's DataBuffer (16-bit grayscale heightmaps, 8-bit
grayscale materials, 8-bit palette images whose indices are used). Only non-interlaced 8 and 16 bit images are modelled."""

from __future__ import annotations

import struct
import sys
import zlib
from array import array
from dataclasses import dataclass
from pathlib import Path

from . import GeoOracleError

SIGNATURE = b"\x89PNG\r\n\x1a\n"
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


@dataclass
class PngSamples:
	width: int
	height: int
	bit_depth: int
	color_type: int
	channels: int
	# "ushort" (DataBufferUShort) or "byte" (DataBufferByte)
	buffer: str
	# unsigned sample values in raster order
	samples: array


def _paeth(a: int, b: int, c: int) -> int:
	p = a + b - c
	pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
	if pa <= pb and pa <= pc:
		return a
	return b if pb <= pc else c


def _add_bytes(a: bytearray, b: bytearray) -> bytearray:
	"""(a[i] + b[i]) & 0xFF for every byte, computed on big integers (SWAR: the low 7 bits add without carries between bytes, the top bits are
	combined by xor)"""
	n = len(a)
	if n == 0:
		return bytearray()
	low = int.from_bytes(bytes([0x7F]) * n, "big")
	high = int.from_bytes(bytes([0x80]) * n, "big")
	x = int.from_bytes(a, "big")
	y = int.from_bytes(b, "big")
	return bytearray((((x & low) + (y & low)) ^ ((x ^ y) & high)).to_bytes(n, "big"))


def read_png(path: Path) -> PngSamples:
	data = path.read_bytes()
	if not data.startswith(SIGNATURE):
		raise GeoOracleError(f"{path}: not a PNG file")
	pos = len(SIGNATURE)
	header = None
	idat = bytearray()
	while pos + 12 <= len(data):
		(length,) = struct.unpack_from(">I", data, pos)
		kind = data[pos + 4:pos + 8]
		body = data[pos + 8:pos + 8 + length]
		pos += 12 + length
		if kind == b"IHDR":
			header = struct.unpack(">IIBBBBB", body)
		elif kind == b"IDAT":
			idat += body
		elif kind == b"IEND":
			break
		elif kind == b"tRNS":
			raise GeoOracleError(f"{path}: tRNS chunks are not modelled")
	if header is None:
		raise GeoOracleError(f"{path}: no IHDR chunk")
	width, height, bit_depth, color_type, _compression, _filter, interlace = header
	if interlace != 0 or bit_depth not in (8, 16) or color_type not in CHANNELS:
		raise GeoOracleError(f"{path}: interlace {interlace}, bit depth {bit_depth}, color type {color_type} are not modelled")
	channels = CHANNELS[color_type]
	bpp = channels * bit_depth // 8
	stride = width * bpp
	raw = zlib.decompress(bytes(idat))
	if len(raw) < height * (stride + 1):
		raise GeoOracleError(f"{path}: image data too short")
	out = bytearray(height * stride)
	previous = bytearray(stride)
	for row in range(height):
		start = row * (stride + 1)
		kind = raw[start]
		line = bytearray(raw[start + 1:start + 1 + stride])
		if kind == 1:
			for i in range(bpp, stride):
				line[i] = (line[i] + line[i - bpp]) & 0xFF
		elif kind == 2:
			line = _add_bytes(line, previous)
		elif kind == 3:
			for i in range(stride):
				left = line[i - bpp] if i >= bpp else 0
				line[i] = (line[i] + ((left + previous[i]) >> 1)) & 0xFF
		elif kind == 4:
			for i in range(stride):
				left = line[i - bpp] if i >= bpp else 0
				up_left = previous[i - bpp] if i >= bpp else 0
				line[i] = (line[i] + _paeth(left, previous[i], up_left)) & 0xFF
		elif kind != 0:
			raise GeoOracleError(f"{path}: unknown filter {kind}")
		out[row * stride:(row + 1) * stride] = line
		previous = line
	if bit_depth == 16:
		samples = array("H")
		samples.frombytes(bytes(out))
		if samples.itemsize != 2:
			raise GeoOracleError("array('H') is not 16 bits on this platform")
		if sys.byteorder == "little":
			samples.byteswap()  # PNG samples are big endian
		return PngSamples(width, height, bit_depth, color_type, channels, "ushort", samples)
	return PngSamples(width, height, bit_depth, color_type, channels, "byte", array("B", bytes(out)))
