# Geo oracle (P4-04)

Independent check of the C++ geo engine against the Java rules (handlers-and-porting-plan.md §2.7 M4 item 5, §3.1 item 1, §3.2 P4-04).
Python 3.12, standard library only. Written from the Java sources and the data files; nothing is taken from the C++ port.

| Module | Content |
|---|---|
| `javamath.py` | Java float32 and int arithmetic (one rounding per operation) |
| `jgeo.py` | float32 emulation of Vector3f, Matrix4f (`setTransform`, `mult`, `invert`), Ray triangle tests, BoundingBox (`containAABB`, `transform`, `intersects(Ray)`, clip), `GeoWorldLoader.getVectorHash` |
| `pngread.py` | PNG samples as Java's ImageIO stores them (8/16 bit, non-interlaced) |
| `geofiles.py` | readers of `models.mesh`, `<mapId>.geo` (big endian) and the map ids of `world_maps.xml` |
| `loader.py` | the scene rules of `GeoWorldLoader` (aliases, town levels, despawnable types, material zone names, terrain association) and the counts |
| `probes.py` | `GeoMap.getZ` by brute force over the triangles of the geometries on the ray (no BIH), with rejection of probes near rounding boundaries |
| `run.py` | the document `expected/geo_expected.json`: counts, the FNV-1a 64 digest of all zone names (sorted, joined with LF, UTF-8) and 50 zone names of the sorted list, 200 probes (130 on mesh triangles, 50 on terrains, 20 random) on 12 maps drawn with seed 4804 |

Commands (from `cpp/tools/oracle`):

```
python -m geo generate   # writes expected/geo_expected.json (about 5 s)
python -m geo check      # exit 1 if the committed document is stale
```

Tests: `tests/test_geo_oracle.py` (CTest `tools.oracle`); the C++ side is `game-server/tests/geo/GeoRealDataTest.cpp`, which loads `data/geo`
and compares the counts, the zone names (count, distinct count, digest of the full list and the sample) and every probe bit for bit.
