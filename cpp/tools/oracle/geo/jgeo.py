"""float32 emulation of the Java geo code paths the oracle needs (written from the Java sources, not from the C++ port).

Vectors are 3-tuples of floats holding float32 values, matrices 16-tuples (row major: m00 m01 m02 m03 m10 ... m33).
"""

from __future__ import annotations

import math

from .javamath import FLT_EPSILON, INF, NAN, add, div, f32, fabs, float_to_int_bits, java_long_rem, mul, sqrt, sub

# ---- Vector3f -------------------------------------------------------------------------------------------------------------------------------


def v_sub(a, b):
	return (sub(a[0], b[0]), sub(a[1], b[1]), sub(a[2], b[2]))


def v_add(a, b):
	return (add(a[0], b[0]), add(a[1], b[1]), add(a[2], b[2]))


def v_dot(a, b):
	"""Vector3f.dot: x * vec.x + y * vec.y + z * vec.z"""
	return add(add(mul(a[0], b[0]), mul(a[1], b[1])), mul(a[2], b[2]))


def v_cross(a, b):
	"""Vector3f.cross: resX = y * vz - z * vy, resY = z * vx - x * vz, resZ = x * vy - y * vx"""
	return (sub(mul(a[1], b[2]), mul(a[2], b[1])), sub(mul(a[2], b[0]), mul(a[0], b[2])), sub(mul(a[0], b[1]), mul(a[1], b[0])))


def v_normalize(v):
	"""Vector3f.normalizeLocal"""
	x, y, z = v
	length = add(add(mul(x, x), mul(y, y)), mul(z, z))
	if length != 1.0 and length != 0.0:
		length = div(1.0, sqrt(length))
		return (mul(x, length), mul(y, length), mul(z, length))
	return v


def v_distance(a, b):
	"""Vector3f.distance: FastMath.sqrt((float) (dx * dx + dy * dy + dz * dz)) with double dx = x - v.x (a float difference)"""
	dx = sub(a[0], b[0])
	dy = sub(a[1], b[1])
	dz = sub(a[2], b[2])
	return sqrt(f32(dx * dx + dy * dy + dz * dz))


# ---- Matrix4f ------------------------------------------------------------------------------------------------------------------------------


def world_matrix(rotation, location, scale):
	"""Geometry.setTransform: loadIdentity, setRotationMatrix(rotation), scale(scale), setTranslation(location)

	rotation: 9 floats, row major (Matrix3f.set(i, j) in reading order)."""
	sx, sy, sz = scale
	m = [rotation[0], rotation[1], rotation[2], 0.0, rotation[3], rotation[4], rotation[5], 0.0, rotation[6], rotation[7], rotation[8], 0.0, 0.0, 0.0,
		0.0, 1.0]
	for row in range(4):  # Matrix4f.scale: m[row][0] *= x, m[row][1] *= y, m[row][2] *= z for all four rows
		m[row * 4 + 0] = mul(m[row * 4 + 0], sx)
		m[row * 4 + 1] = mul(m[row * 4 + 1], sy)
		m[row * 4 + 2] = mul(m[row * 4 + 2], sz)
	m[3], m[7], m[11] = location
	return tuple(m)


def m_mult(m, v):
	"""Matrix4f.mult(Vector3f, store): m00 * vx + m01 * vy + m02 * vz + m03 (left to right)"""
	x, y, z = v
	return (add(add(add(mul(m[0], x), mul(m[1], y)), mul(m[2], z)), m[3]), add(add(add(mul(m[4], x), mul(m[5], y)), mul(m[6], z)), m[7]),
		add(add(add(mul(m[8], x), mul(m[9], y)), mul(m[10], z)), m[11]))


def m_mult_proj_w(m, v):
	"""the w value of Matrix4f.multProj: m30 * vx + m31 * vy + m32 * vz + m33"""
	x, y, z = v
	return add(add(add(mul(m[12], x), mul(m[13], y)), mul(m[14], z)), m[15])


def m_mult_normal(m, v):
	"""Matrix4f.multNormal: no translation"""
	x, y, z = v
	return (add(add(mul(m[0], x), mul(m[1], y)), mul(m[2], z)), add(add(mul(m[4], x), mul(m[5], y)), mul(m[6], z)),
		add(add(mul(m[8], x), mul(m[9], y)), mul(m[10], z)))


def m_invert(m):
	"""Matrix4f.invert(): None where Java throws ArithmeticException (|det| <= 0)"""
	m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33 = m

	def d(a, b, c, e):  # a*b - c*e
		return sub(mul(a, b), mul(c, e))

	fA0 = d(m00, m11, m01, m10)
	fA1 = d(m00, m12, m02, m10)
	fA2 = d(m00, m13, m03, m10)
	fA3 = d(m01, m12, m02, m11)
	fA4 = d(m01, m13, m03, m11)
	fA5 = d(m02, m13, m03, m12)
	fB0 = d(m20, m31, m21, m30)
	fB1 = d(m20, m32, m22, m30)
	fB2 = d(m20, m33, m23, m30)
	fB3 = d(m21, m32, m22, m31)
	fB4 = d(m21, m33, m23, m31)
	fB5 = d(m22, m33, m23, m32)
	det = add(sub(add(add(sub(mul(fA0, fB5), mul(fA1, fB4)), mul(fA2, fB3)), mul(fA3, fB2)), mul(fA4, fB1)), mul(fA5, fB0))
	if fabs(det) <= 0.0:
		return None

	def t3(a, b, c, e, g, h, sign_first):  # sign_first*a*b - c*e + g*h  (Java: + a*b - c*e + g*h or - a*b + c*e - g*h)
		if sign_first > 0:
			return add(sub(mul(a, b), mul(c, e)), mul(g, h))
		return sub(add(mul(-a, b), mul(c, e)), mul(g, h))

	s00 = t3(m11, fB5, m12, fB4, m13, fB3, 1)
	s10 = t3(m10, fB5, m12, fB2, m13, fB1, -1)
	s20 = t3(m10, fB4, m11, fB2, m13, fB0, 1)
	s30 = t3(m10, fB3, m11, fB1, m12, fB0, -1)
	s01 = t3(m01, fB5, m02, fB4, m03, fB3, -1)
	s11 = t3(m00, fB5, m02, fB2, m03, fB1, 1)
	s21 = t3(m00, fB4, m01, fB2, m03, fB0, -1)
	s31 = t3(m00, fB3, m01, fB1, m02, fB0, 1)
	s02 = t3(m31, fA5, m32, fA4, m33, fA3, 1)
	s12 = t3(m30, fA5, m32, fA2, m33, fA1, -1)
	s22 = t3(m30, fA4, m31, fA2, m33, fA0, 1)
	s32 = t3(m30, fA3, m31, fA1, m32, fA0, -1)
	s03 = t3(m21, fA5, m22, fA4, m23, fA3, -1)
	s13 = t3(m20, fA5, m22, fA2, m23, fA1, 1)
	s23 = t3(m20, fA4, m21, fA2, m23, fA0, -1)
	s33 = t3(m20, fA3, m21, fA1, m22, fA0, 1)
	inv_det = div(1.0, det)
	return tuple(mul(value, inv_det) for value in (s00, s01, s02, s03, s10, s11, s12, s13, s20, s21, s22, s23, s30, s31, s32, s33))


# ---- Ray ----------------------------------------------------------------------------------------------------------------------------------


def ray_intersects_t(origin, direction, v0, v1, v2):
	"""Ray.intersects(v0, v1, v2): the ray parameter t, INF for a miss (the limit is not checked)"""
	edge1X = sub(v1[0], v0[0])
	edge1Y = sub(v1[1], v0[1])
	edge1Z = sub(v1[2], v0[2])
	edge2X = sub(v2[0], v0[0])
	edge2Y = sub(v2[1], v0[1])
	edge2Z = sub(v2[2], v0[2])
	normX = sub(mul(edge1Y, edge2Z), mul(edge1Z, edge2Y))
	normY = sub(mul(edge1Z, edge2X), mul(edge1X, edge2Z))
	normZ = sub(mul(edge1X, edge2Y), mul(edge1Y, edge2X))
	dirDotNorm = add(add(mul(direction[0], normX), mul(direction[1], normY)), mul(direction[2], normZ))
	diffX = sub(origin[0], v0[0])
	diffY = sub(origin[1], v0[1])
	diffZ = sub(origin[2], v0[2])
	if dirDotNorm > FLT_EPSILON:
		sign = 1.0
	elif dirDotNorm < -FLT_EPSILON:
		sign = -1.0
		dirDotNorm = -dirDotNorm
	else:
		return INF
	dex = sub(mul(diffY, edge2Z), mul(diffZ, edge2Y))
	dey = sub(mul(diffZ, edge2X), mul(diffX, edge2Z))
	dez = sub(mul(diffX, edge2Y), mul(diffY, edge2X))
	dirDotDiffxEdge2 = mul(sign, add(add(mul(direction[0], dex), mul(direction[1], dey)), mul(direction[2], dez)))
	if dirDotDiffxEdge2 >= 0.0:
		dex = sub(mul(edge1Y, diffZ), mul(edge1Z, diffY))
		dey = sub(mul(edge1Z, diffX), mul(edge1X, diffZ))
		dez = sub(mul(edge1X, diffY), mul(edge1Y, diffX))
		dirDotEdge1xDiff = mul(sign, add(add(mul(direction[0], dex), mul(direction[1], dey)), mul(direction[2], dez)))
		if dirDotEdge1xDiff >= 0.0:
			if add(dirDotDiffxEdge2, dirDotEdge1xDiff) <= dirDotNorm:
				diffDotNorm = mul(-sign, add(add(mul(diffX, normX), mul(diffY, normY)), mul(diffZ, normZ)))
				if diffDotNorm >= 0.0:
					inv = div(1.0, dirDotNorm)
					return mul(diffDotNorm, inv)
	return INF


def ray_intersect_where(origin, direction, v0, v1, v2):
	"""Ray.intersectWhere(v0, v1, v2, store): the intersection point or None (intersects(..., store, doPlanar=false, quad=false))"""
	diff = v_sub(origin, v0)
	edge1 = v_sub(v1, v0)
	edge2 = v_sub(v2, v0)
	norm = v_cross(edge1, edge2)
	dirDotNorm = v_dot(direction, norm)
	if dirDotNorm > FLT_EPSILON:
		sign = 1.0
	elif dirDotNorm < -FLT_EPSILON:
		sign = -1.0
		dirDotNorm = -dirDotNorm
	else:
		return None
	dirDotDiffxEdge2 = mul(sign, v_dot(direction, v_cross(diff, edge2)))  # diff.cross(edge2, edge2)
	if dirDotDiffxEdge2 >= 0.0:
		dirDotEdge1xDiff = mul(sign, v_dot(direction, v_cross(edge1, diff)))  # edge1.crossLocal(diff)
		if dirDotEdge1xDiff >= 0.0:
			if add(dirDotDiffxEdge2, dirDotEdge1xDiff) <= dirDotNorm:
				diffDotNorm = mul(-sign, v_dot(diff, norm))
				if diffDotNorm >= 0.0:
					inv = div(1.0, dirDotNorm)
					t = mul(diffDotNorm, inv)
					return (add(origin[0], mul(direction[0], t)), add(origin[1], mul(direction[1], t)), add(origin[2], mul(direction[2], t)))
	return None


# ---- BoundingBox --------------------------------------------------------------------------------------------------------------------------


def contain_aabb(vertices):
	"""BoundingBox.containAABB over a flat float32 sequence: (center, (xExtent, yExtent, zExtent))"""
	xs = vertices[0::3]
	ys = vertices[1::3]
	zs = vertices[2::3]
	# min/max with < and > like Java (the data has no NaN; min() and max() agree then)
	minX, maxX, minY, maxY, minZ, maxZ = min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)
	center = (mul(add(minX, maxX), 0.5), mul(add(minY, maxY), 0.5), mul(add(minZ, maxZ), 0.5))
	return center, (sub(maxX, center[0]), sub(maxY, center[1]), sub(maxZ, center[2]))


def transform_box(center, extents, m):
	"""BoundingBox.transform(trans, store): world center (multProj, divided by w) and extents (|rotation| * extents)"""
	c = m_mult(m, center)
	w = m_mult_proj_w(m, center)
	inv = div(1.0, w)  # Vector3f.divideLocal(w): multiply by 1 / w
	c = (mul(c[0], inv), mul(c[1], inv), mul(c[2], inv))
	a = tuple(fabs(value) for value in m)  # Matrix3f.absoluteLocal of toRotationMatrix
	ex, ey, ez = extents
	e = (add(add(mul(a[0], ex), mul(a[1], ey)), mul(a[2], ez)), add(add(mul(a[4], ex), mul(a[5], ey)), mul(a[6], ez)),
		add(add(mul(a[8], ex), mul(a[9], ey)), mul(a[10], ez)))
	return c, (fabs(e[0]), fabs(e[1]), fabs(e[2]))


def box_intersects_ray(center, extents, origin, direction):
	"""BoundingBox.intersects(Ray) (separating axis test)"""
	xe, ye, ze = extents
	diff = v_sub(origin, center)
	fWdU = [0.0, 0.0, 0.0]
	fAWdU = [0.0, 0.0, 0.0]
	for axis, extent in ((0, xe), (1, ye), (2, ze)):
		fWdU[axis] = v_dot(direction, (1.0 if axis == 0 else 0.0, 1.0 if axis == 1 else 0.0, 1.0 if axis == 2 else 0.0))
		fAWdU[axis] = fabs(fWdU[axis])
		fDdU = v_dot(diff, (1.0 if axis == 0 else 0.0, 1.0 if axis == 1 else 0.0, 1.0 if axis == 2 else 0.0))
		fADdU = fabs(fDdU)
		if fADdU > extent and mul(fDdU, fWdU[axis]) >= 0.0:
			return False
	wCrossD = v_cross(direction, diff)
	if fabs(v_dot(wCrossD, (1.0, 0.0, 0.0))) > add(mul(ye, fAWdU[2]), mul(ze, fAWdU[1])):
		return False
	if fabs(v_dot(wCrossD, (0.0, 1.0, 0.0))) > add(mul(xe, fAWdU[2]), mul(ze, fAWdU[0])):
		return False
	if fabs(v_dot(wCrossD, (0.0, 0.0, 1.0))) > add(mul(xe, fAWdU[1]), mul(ye, fAWdU[0])):
		return False
	return True


def _clip(denom, numer, t):
	if denom > 0.0:
		if numer > mul(denom, t[1]):
			return False
		if numer > mul(denom, t[0]):
			t[0] = div(numer, denom)
		return True
	if denom < 0.0:
		if numer > mul(denom, t[0]):
			return False
		if numer > mul(denom, t[1]):
			t[1] = div(numer, denom)
		return True
	return numer <= 0.0


def box_collide_ray_count(center, extents, origin, direction, limit):
	"""the number of collisions BoundingBox.collideWithRay adds (distances NaN are not counted by CollisionResults)"""
	xe, ye, ze = extents
	diff = v_sub(origin, center)
	t = [0.0, limit]
	save0, save1 = t[0], t[1]
	not_clipped = (_clip(direction[0], sub(-diff[0], xe), t) and _clip(-direction[0], sub(diff[0], xe), t) and _clip(direction[1], sub(-diff[1], ye), t)
		and _clip(-direction[1], sub(diff[1], ye), t) and _clip(direction[2], sub(-diff[2], ze), t) and _clip(-direction[2], sub(diff[2], ze), t))
	count = 0
	if not_clipped and (t[0] != save0 or t[1] != save1):
		if not math.isnan(t[0]):
			count += 1
		if t[1] > t[0] and not math.isnan(t[1]):
			count += 1
	return count


def box_contains(center, extents, point):
	"""BoundingBox.contains: strictly inside"""
	return (fabs(sub(center[0], point[0])) < extents[0] and fabs(sub(center[1], point[1])) < extents[1]
		and fabs(sub(center[2], point[2])) < extents[2])


# ---- GeoWorldLoader -------------------------------------------------------------------------------------------------------------------------


def vector_hash(location):
	"""GeoWorldLoader.getVectorHash: (int) ((x * 73856093 ^ y * 19349669 ^ z * 83492791) % 700001) over the long float bits"""
	x = float_to_int_bits(location[0])
	y = float_to_int_bits(location[1])
	z = float_to_int_bits(location[2])
	value = (x * 73856093) ^ (y * 19349669) ^ (z * 83492791)
	return java_long_rem(value, 700001)


__all__ = [name for name in globals() if not name.startswith("_")] + ["NAN", "f32"]
