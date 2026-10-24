"""m5i/geoshield.py: the geo shield crossing of C5 on a synthetic shield (a vertical wall of two triangles), no geo files."""

import unittest
from array import array

from geo import loader as geo_loader
from geo.geofiles import MeshModel
from geo.jgeo import world_matrix
from m5i.geoshield import COLLISION_CHECK_Z_OFFSET, SHIELD_MATERIAL, shield_crossing

IDENTITY_ROTATION = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)


def wall(x: float, material: int = SHIELD_MATERIAL, intentions: int = geo_loader.MATERIAL):
	"""a wall in the plane x = const, y in [-10, 10], z in [0, 20], placed at the origin"""
	vertices = array("f", [x, -10.0, 0.0, x, 10.0, 0.0, x, 10.0, 20.0, x, -10.0, 20.0])
	indices = array("i", [0, 1, 2, 0, 2, 3])
	mesh = MeshModel(vertices, indices, 2, material, intentions)
	return geo_loader.PlacedGeometry(0, 0, "levels/test/castleshield_wall.cgf", mesh, world_matrix(IDENTITY_ROTATION, (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
		0, 0, intentions)


class ShieldCrossingTest(unittest.TestCase):
	def test_the_crossing_lies_on_the_wall_between_m1_and_m2(self):
		result = shield_crossing([wall(5.0)], (30.0, 0.0, 0.0), (0.0, 0.0, 10.0))
		self.assertIsNotNone(result)
		self.assertAlmostEqual(result["hit"][0], 5.0, places=3)
		self.assertAlmostEqual(result["hit"][2], 10.0, places=3)  # the height of the bound's center
		self.assertAlmostEqual(result["M1"][0], 8.0, places=3)  # 3 m before the wall, on S1's side
		self.assertAlmostEqual(result["M2"][0], 2.0, places=3)  # 3 m after it
		self.assertAlmostEqual(result["M1"][2], 10.0 - COLLISION_CHECK_Z_OFFSET, places=3)  # the observer's rays run 1 m above the feet
		self.assertAlmostEqual(result["distanceFromS1"], 25.0, places=3)

	def test_only_material_11_with_the_material_intention_is_a_shield(self):
		self.assertIsNone(shield_crossing([wall(5.0, material=10)], (30.0, 0.0, 0.0), (0.0, 0.0, 10.0)))
		self.assertIsNone(shield_crossing([wall(5.0, intentions=0)], (30.0, 0.0, 0.0), (0.0, 0.0, 10.0)))

	def test_a_shield_far_from_the_sphere_is_not_the_fortress_s(self):
		self.assertIsNone(shield_crossing([wall(5.0)], (30.0, 0.0, 0.0), (500.0, 0.0, 10.0)))


if __name__ == "__main__":
	unittest.main()
