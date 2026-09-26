"""Unit tests of the geo oracle (geo/): Java float arithmetic, the Java geometry emulation, the PNG and geo file readers, the loader rules and
the getZ brute force on small generated inputs, and the drift check of expected/geo_expected.json against the real data (skipped without
the Java tree). Expectations are derived by hand from the Java sources."""

from __future__ import annotations

import math
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from geo import GeoOracleError, loader, m4, run
from geo.geofiles import read_meshes, read_placements, read_world_map_ids
from geo.javamath import (add, div, f32, float_compare, float_to_int, float_to_int_bits, float_to_raw_uint_bits, java_long_rem, java_max,
	java_min, mul, sqrt, sub)
from geo.jgeo import (box_collide_ray_count, box_contains, box_intersects_ray, contain_aabb, m_invert, m_mult, m_mult_normal, ray_intersect_where,
	ray_intersects_t, transform_box, v_distance, v_normalize, vector_hash, world_matrix)
from geo.pngread import read_png
from geo.probes import MapScene

IDENTITY_ROTATION = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)


class JavaMathTest(unittest.TestCase):
	def test_float32_rounding(self):
		self.assertEqual(float_to_raw_uint_bits(f32(0.1)), 0x3DCCCCCD)
		self.assertEqual(add(3.4028234663852886e38, 3.4028234663852886e38), math.inf)
		self.assertEqual(f32(3.4028235e38), 3.4028234663852886e38)
		self.assertEqual(mul(16777217.0, 1.0), 16777216.0)  # 2^24 + 1 is not a float32
		self.assertEqual(sub(1.0, 1e-9), 1.0)

	def test_division_and_sqrt(self):
		self.assertEqual(div(1.0, 0.0), math.inf)
		self.assertEqual(div(1.0, -0.0), -math.inf)
		self.assertTrue(math.isnan(div(0.0, 0.0)))
		self.assertTrue(math.isnan(sqrt(-1.0)))
		self.assertEqual(sqrt(4.0), 2.0)

	def test_int_conversions_and_comparisons(self):
		self.assertEqual(float_to_int(math.nan), 0)
		self.assertEqual(float_to_int(3e9), 2147483647)
		self.assertEqual(float_to_int(-3e9), -2147483648)
		self.assertEqual(float_to_int(-0.75), 0)
		self.assertEqual(float_to_int_bits(math.nan), 0x7FC00000)
		self.assertEqual(float_to_int_bits(-1.0), -1082130432)  # 0xBF800000 as int
		self.assertEqual(java_long_rem(-7, 3), -1)
		self.assertEqual(float_compare(-0.0, 0.0), -1)
		self.assertEqual(float_compare(math.nan, math.inf), 1)
		self.assertTrue(math.isnan(java_max(math.nan, 1.0)))
		self.assertTrue(math.isnan(java_max(1.0, math.nan)))
		self.assertEqual(math.copysign(1.0, java_max(-0.0, 0.0)), 1.0)
		self.assertEqual(math.copysign(1.0, java_min(0.0, -0.0)), -1.0)


class JavaGeometryTest(unittest.TestCase):
	def test_normalize_and_distance(self):
		self.assertEqual(v_normalize((0.0, 0.0, -30.0)), (0.0, 0.0, -1.0))
		self.assertEqual(v_normalize((0.0, 0.0, 0.0)), (0.0, 0.0, 0.0))
		self.assertEqual(v_distance((0.0, 0.0, 0.0), (3.0, 4.0, 0.0)), 5.0)

	def test_world_matrix_and_inverse(self):
		# rotation 90 degrees around z, then columns scaled by (2, 3, 4), translation (10, 20, 30)
		matrix = world_matrix((0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0), (10.0, 20.0, 30.0), (2.0, 3.0, 4.0))
		self.assertEqual(m_mult(matrix, (1.0, 2.0, 3.0)), (4.0, 22.0, 42.0))
		self.assertEqual(m_mult_normal(matrix, (1.0, 0.0, 0.0)), (0.0, 2.0, 0.0))
		inverse = m_invert(matrix)
		self.assertEqual(m_mult(inverse, (4.0, 22.0, 42.0)), (1.0, 2.0, 3.0))
		self.assertIsNone(m_invert(world_matrix(IDENTITY_ROTATION, (0.0, 0.0, 0.0), (0.0, 1.0, 1.0))), "singular: Java throws")

	def test_ray_triangle(self):
		triangle = ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 10.0, 0.0))
		self.assertEqual(ray_intersects_t((1.0, 1.0, 5.0), (0.0, 0.0, -1.0), *triangle), 5.0)
		self.assertEqual(ray_intersects_t((1.0, 1.0, 5.0), (0.0, 0.0, 1.0), *triangle), math.inf, "behind the origin")
		self.assertEqual(ray_intersects_t((9.0, 9.0, 5.0), (0.0, 0.0, -1.0), *triangle), math.inf, "outside")
		self.assertEqual(ray_intersects_t((1.0, 1.0, 5.0), (1.0, 0.0, 0.0), *triangle), math.inf, "parallel")
		self.assertEqual(ray_intersect_where((1.0, 1.0, 5.0), (0.0, 0.0, -1.0), *triangle), (1.0, 1.0, 0.0))
		# the sloped cell of the C++ terrain test: z = 10 + 10 (x - 2) + 5 (y - 2) at (2.5, 2.5) is 17.5
		self.assertEqual(ray_intersect_where((2.5, 2.5, 100.0), (0.0, 0.0, -1.0), (2.0, 2.0, 10.0), (2.0, 4.0, 20.0), (4.0, 2.0, 30.0)),
			(2.5, 2.5, 17.5))

	def test_bounding_box(self):
		center, extents = contain_aabb([0.0, 0.0, 0.0, 2.0, 4.0, -6.0])
		self.assertEqual((center, extents), ((1.0, 2.0, -3.0), (1.0, 2.0, 3.0)))
		matrix = world_matrix((0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0), (10.0, 20.0, 30.0), (2.0, 3.0, 4.0))
		self.assertEqual(transform_box((1.0, 2.0, 3.0), (1.0, 1.0, 1.0), matrix), ((4.0, 22.0, 42.0), (3.0, 2.0, 4.0)))
		unit = ((0.0, 0.0, 0.0), (1.0, 1.0, 1.0))
		self.assertTrue(box_intersects_ray(*unit, (-5.0, 0.0, 0.0), (1.0, 0.0, 0.0)))
		self.assertFalse(box_intersects_ray(*unit, (-5.0, 0.0, 0.0), (-1.0, 0.0, 0.0)))
		self.assertFalse(box_intersects_ray(*unit, (-5.0, 3.0, 0.0), (1.0, 0.0, 0.0)))
		self.assertEqual(box_collide_ray_count(*unit, (-5.0, 0.0, 0.0), (1.0, 0.0, 0.0), math.inf), 2)
		self.assertEqual(box_collide_ray_count(*unit, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), 0.5), 0, "segment inside: nothing clipped")
		self.assertFalse(box_contains(*unit, (1.0, 0.0, 0.0)))
		self.assertTrue(box_contains(*unit, (0.5, 0.0, 0.0)))

	def test_vector_hash(self):
		# 1065353216 * 73856093 ^ 1073741824 * 19349669 ^ 1077936128 * 83492791 = 27557996166905856, % 700001
		self.assertEqual((1065353216 * 73856093) ^ (1073741824 * 19349669) ^ (1077936128 * 83492791), 27557996166905856)
		self.assertEqual(vector_hash((1.0, 2.0, 3.0)), 27557996166905856 % 700001)
		self.assertEqual(vector_hash((45.0, 45.0, 1.0)), 260974)
		self.assertEqual(vector_hash((41.0, 41.0, 1.0)), 418423)
		self.assertLess(vector_hash((-1.0, 0.0, math.nan)), 0, "a negative dividend keeps its sign")


def png_bytes(width, height, bit_depth, color_type, rows, filters):
	raw = bytearray()
	bpp = (bit_depth // 8) * (1 if color_type in (0, 3) else 3)
	previous = bytes(len(rows[0]))
	for row, kind in zip(rows, filters):
		line = bytearray()
		for i, value in enumerate(row):
			left = row[i - bpp] if i >= bpp else 0
			predictor = [0, left, previous[i], (left + previous[i]) // 2][kind]
			line.append((value - predictor) & 0xFF)
		raw.append(kind)
		raw += line
		previous = row

	def chunk(kind, body):
		return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

	header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
	return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b"")


class PngReadTest(unittest.TestCase):
	def test_gray16_and_gray8(self):
		with tempfile.TemporaryDirectory() as tmp:
			path = Path(tmp) / "h.png"
			values = [0, 100, 65535, 4097, 256, 1]
			rows = [b"".join(struct.pack(">H", v) for v in values[0:3]), b"".join(struct.pack(">H", v) for v in values[3:6])]
			path.write_bytes(png_bytes(3, 2, 16, 0, rows, [1, 2]))
			image = read_png(path)
			self.assertEqual((image.width, image.height, image.buffer), (3, 2, "ushort"))
			self.assertEqual(list(image.samples), values)
			rows8 = [bytes([1, 2, 3, 4]), bytes([200, 100, 50, 25])]
			path.write_bytes(png_bytes(4, 2, 8, 3, rows8, [3, 2]))
			image = read_png(path)
			self.assertEqual((image.buffer, list(image.samples)), ("byte", [1, 2, 3, 4, 200, 100, 50, 25]))


def mesh_entry(name, models):
	out = struct.pack(">h", len(name)) + name.encode() + bytes([len(models)])
	for vertices, indices, index_size, material, intentions in models:
		out += struct.pack(">H", len(vertices) // 3) + struct.pack(f">{len(vertices)}f", *vertices)
		out += struct.pack(">Hb", len(indices) // 3, index_size)
		out += bytes(indices) if index_size == 1 else struct.pack(f">{len(indices)}H", *indices)
		out += struct.pack(">Bb", material, intentions)
	return out


def placement(name, x, y, z, kind=0, ident=0, level=0):
	return struct.pack(">h", len(name)) + name.encode() + struct.pack(">15fbhb", x, y, z, *IDENTITY_ROTATION, 1.0, 1.0, 1.0, kind, ident, level)


QUAD = ([0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0], [0, 1, 2, 1, 3, 2], 1, 0, 1)
FIRE_QUAD = (QUAD[0], QUAD[1], 1, 11, 3)
FIRE_TRIANGLE = ([0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 2.0, 0.0], [0, 1, 2], 2, 11, 3)


class LoaderTest(unittest.TestCase):
	"""the scenario of the C++ GeoWorldLoaderFilesTest"""

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.dir = Path(self.tmp.name)
		(self.dir / "models.mesh").write_bytes(mesh_entry("levels/common/rock_a.cgf", [QUAD])
			+ mesh_entry("levels/common/fire_a.cgf|levels/common/fire_b.cgf", [FIRE_QUAD, FIRE_TRIANGLE])
			+ mesh_entry("town/house_01.cgf", [QUAD]) + mesh_entry("town/house_03.cgf", [QUAD]))
		(self.dir / "110010000.geo").write_bytes(placement("levels/common/rock_a.cgf", 20, 20, 5) + placement("levels/common/fire_b.cgf", 40, 40, 1)
			+ placement("town/house_01.cgf", 100, 100, 0, 5, 7, 1) + placement("missing/model.cgf", 0, 0, 0))
		heights = [b"".join(struct.pack(">H", 64) for _ in range(4)) for _ in range(4)]
		(self.dir / "110010000.png").write_bytes(png_bytes(4, 4, 16, 0, heights, [0, 0, 0, 0]))
		self.world_maps = self.dir / "world_maps.xml"
		self.world_maps.write_text('<world_maps>\n\t<map id="110010000" world_size="1024"/>\n\t<map id="120010000" world_size="0"/>\n</world_maps>\n')

	def tearDown(self):
		self.tmp.cleanup()

	def test_counts_and_zone_names(self):
		self.assertEqual(read_world_map_ids(self.world_maps), [110010000, 120010000])
		self.assertEqual(len(read_meshes(self.dir / "models.mesh")), 4)
		self.assertEqual(len(read_placements(self.dir / "110010000.geo")), 4)
		placed = {}
		result = loader.load(self.dir, self.world_maps, {110010000}, placed)
		self.assertEqual((result.mesh_entries, result.meshes, result.mesh_names), (4, 5, 5))
		self.assertEqual((result.geo_files, result.placements, result.missing_mesh_placements), (1, 4, 1))
		self.assertEqual((result.attached_nodes, result.geometries, result.placement_geometries), (4, 5, 4))
		self.assertEqual(result.material_geometries, 2)
		self.assertEqual(result.terrain_maps, 1)
		self.assertEqual(sorted(result.zone_names), ["FIRE_B_CHILD1_260974_110010000", "FIRE_B_CHILD2_418423_110010000"])
		self.assertEqual(result.material_zones, [("FIRE_B_CHILD1_260974_110010000", 110010000, "FIRE_B_CHILD1_260974", 11),
			("FIRE_B_CHILD2_418423_110010000", 110010000, "FIRE_B_CHILD2_418423", 11)])
		self.assertEqual([g.name for g in placed[110010000] if g.mesh.material_id == 11],
			["levels/common/fire_b.cgf", "levels/common/fire_a.cgf|levels/common/fire_b.cgf"])
		self.assertEqual(result.despawnable_nodes, {"TOWN_OBJECT": 2})

		scene = MapScene(110010000, placed[110010000], loader.read_heightmap(result.terrains[110010000]))
		self.assertEqual(scene.get_z(f32(23.3), f32(24.4), 20.0, -10.0), (5.0, "mesh"))
		self.assertEqual(scene.get_z(3.3000000476837158, 3.3000000476837158, 20.0, -10.0), (2.0, "terrain"))
		z, source = scene.get_z(105.0, 105.0, 20.0, -10.0)
		self.assertTrue(math.isnan(z), "town objects start inactive")
		self.assertEqual(source, "none")

	def test_java_load_errors(self):
		(self.dir / "110010000.geo").write_bytes(placement("levels/common/rock_a.cgf", 1, 1, 1, 9, 1, 0))
		with self.assertRaises(GeoOracleError):
			loader.load(self.dir, self.world_maps)
		(self.dir / "110010000.geo").write_bytes(placement("levels/common/rock_a.cgf", 1, 1, 1, 2, 1, 1))
		with self.assertRaises(GeoOracleError):
			loader.load(self.dir, self.world_maps)


class ZoneNamesDigestTest(unittest.TestCase):
	def test_fnv1a64_reference_vectors(self):
		# the published FNV-1a 64 test vectors
		self.assertEqual(run.fnv1a64(b""), 0xCBF29CE484222325)
		self.assertEqual(run.fnv1a64(b"a"), 0xAF63DC4C8601EC8C)
		self.assertEqual(run.fnv1a64(b"foobar"), 0x85944171F73967E8)

	def test_digest_joins_the_names_with_line_feeds(self):
		self.assertEqual(run.zone_names_digest(["foobar"]), "85944171f73967e8")
		self.assertEqual(run.zone_names_digest(["foo", "bar"]), format(run.fnv1a64(b"foo\nbar"), "016x"))
		self.assertNotEqual(run.zone_names_digest(["foo", "bar"]), run.zone_names_digest(["foobar"]))


class M4CompareTest(unittest.TestCase):
	"""geo/m4.py helpers, expectations from WorldMap.getInstanceCount, WorldMapTemplate twin counts, ZoneData.afterUnmarshal and Float bits"""

	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		self.dir = Path(self.tmp.name)

	def tearDown(self):
		self.tmp.cleanup()

	def test_instance_count(self):
		# twinCount 0 counts as 1; WORLD_MAX_TWINS_USUAL 0 = unlimited, else min; beginner -1 = disabled, 0 = unlimited, else min
		self.assertEqual(m4.instance_count({}, 1, -1), 1)
		self.assertEqual(m4.instance_count({"twin_count": "5", "beginner_twin_count": "3"}, 1, -1), 1)
		self.assertEqual(m4.instance_count({"twin_count": "5", "beginner_twin_count": "3"}, 0, 0), 8)
		self.assertEqual(m4.instance_count({"twin_count": "5", "beginner_twin_count": "3"}, 2, 2), 4)
		self.assertEqual(m4.instance_count({"twin_count": "0", "beginner_twin_count": "3"}, 2, 1), 2)

	def test_float_bits_equal(self):
		self.assertTrue(m4.float_bits_equal(0x41200000, 0x41200000))
		self.assertFalse(m4.float_bits_equal(0x00000000, 0x80000000), "0.0 and -0.0 differ bitwise")
		self.assertTrue(m4.float_bits_equal(0x7FC00000, 0x7FC00001), "any NaN equals any NaN")
		self.assertTrue(m4.float_bits_equal(0xFFC00000, 0x7FC00000))
		self.assertFalse(m4.float_bits_equal(0x7F800000, 0x7FC00000), "infinity is no NaN")
		self.assertTrue(math.isnan(m4.struct_float(0x7FC00000)))

	def test_properties_and_probe_file(self):
		(self.dir / "main").mkdir()
		(self.dir / "main" / "world.properties").write_text("# comment\ngameserver.world.max.twincount.usual = 3\n! other\nkey:value\n")
		(self.dir / "mygs.properties").write_text("gameserver.world.max.twincount.usual=0\n")
		self.assertEqual(m4.read_properties(self.dir), {"gameserver.world.max.twincount.usual": "0", "key": "value"})
		probes = {"probes": [{"map": 210010000, "instanceId": 1, "x": {"bits": 1}, "y": {"bits": 2}, "zMax": {"bits": 3}, "zMin": {"bits": 4}}]}
		self.assertEqual(m4.write_probes(probes, self.dir / "probes.txt"), 1)
		self.assertEqual((self.dir / "probes.txt").read_text().splitlines()[1], "210010000 1 1 2 3 4")

	def test_world_zones_and_xml_zone_names(self):
		(self.dir / "world_zones.txt").write_text("map 1 instances 1\ninstance 1 zones 2\nzone 1\nzone A_1\nmap 2 missing\n")
		self.assertEqual(m4.read_world_zones(self.dir / "world_zones.txt"), {1: [(1, ["1", "A_1"])], 2: None})
		static_data = self.dir / "static_data"
		(static_data / "zones").mkdir(parents=True)
		(static_data / "static_data.xml").write_text('<static_data>\n	<import file="zones" singleRootTag="true"/>\n</static_data>\n')
		(static_data / "zones" / "zones.xml").write_text('<zones>\n	<zone name="a_1" mapid="1"><points/></zone>\n'
			'	<zone name="Sphere_1" mapid="1" area_type="SPHERE"><sphere r="0"/></zone>\n'
			'	<zone name="b_2" mapid="2" area_type="CYLINDER"><cylinder r="5"/></zone>\n</zones>\n')
		self.assertEqual(m4.xml_zone_names_by_map(static_data, 99), {1: {"A_1"}, 2: {"B_2"}})


class RealDataTest(unittest.TestCase):
	def test_expected_document_is_current(self):
		if not run.DEFAULT_GEO_DIR.is_dir() or not run.DEFAULT_WORLD_MAPS.is_file():
			self.skipTest("the Java tree's geo data is not present")
		document = run.generate()
		counts = document["counts"]
		# M4 item 5 anchors (handlers-and-porting-plan.md §2.7)
		self.assertEqual((counts["meshEntries"], counts["meshes"], counts["geoFiles"]), (18583, 25437, 151))
		self.assertEqual((counts["placements"], counts["placementGeometries"], counts["materialGeometries"]), (419707, 484111, 7961))
		self.assertEqual(len(document["probes"]), 200)
		self.assertEqual(len(document["zoneNames"]["sample"]), 50)
		self.assertRegex(document["zoneNames"]["fnv1a64"], "^[0-9a-f]{16}$")
		self.assertEqual(run.dumps(document), run.DEFAULT_EXPECTED.read_text(encoding="utf-8"),
			"expected/geo_expected.json is stale: python -m geo generate")


if __name__ == "__main__":
	unittest.main()
