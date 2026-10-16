"""Independent geo data oracle for the C++ port of the Java geo engine (handlers-and-porting-plan.md §2.7 item 5, §3.1, §3.2 P4-04).

Python 3.12, standard library only (struct, zlib, array). Nothing here reuses the C++ port or the geomath test oracle: the models.mesh and
<mapId>.geo readers, the PNG decoder and the float32 emulation of the Java code paths were written from the Java sources
(GeoWorldLoader, GeoMap, Terrain, BIHNode, BIHTree, BoundingBox, Ray, Matrix4f, Vector3f) and the data files.

Outputs (expected/geo_expected.json): the entity counts of GeoWorldLoader, deterministic getZ probes (brute force over the triangles instead
of the BIH tree) and material zone names. See run.py and README.md.
"""


class GeoOracleError(Exception):
	"""Data the oracle does not model, or data on which Java itself would fail."""
