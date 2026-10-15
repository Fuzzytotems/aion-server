#include "aion/gameserver/geoEngine/math/Ray.h"

#include "aion/gameserver/geoEngine/math/FastMath.h"
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::math {

bool Ray::intersectWhere(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f& loc) const noexcept {
	return intersects(v0, v1, v2, &loc, false, false);
}

bool Ray::intersectWhere(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2) const noexcept {
	return intersects(v0, v1, v2, nullptr, false, false);
}

bool Ray::intersects(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f* store, bool doPlanar, bool quad) const noexcept {
	Vector3f tempVa, tempVb, tempVc, tempVd; // Java: TempVars vect1..vect4

	Vector3f& diff = origin.subtract(v0, tempVa);
	Vector3f& edge1 = v1.subtract(v0, tempVb);
	Vector3f& edge2 = v2.subtract(v0, tempVc);
	Vector3f& norm = edge1.cross(edge2, tempVd);

	float dirDotNorm = direction.dot(norm);
	float sign;
	if (dirDotNorm > FastMath::FLOAT_EPSILON) {
		sign = 1;
	} else if (dirDotNorm < -FastMath::FLOAT_EPSILON) {
		sign = -1.0f;
		dirDotNorm = -dirDotNorm;
	} else {
		// ray and triangle/quad are parallel
		return false;
	}

	const float dirDotDiffxEdge2 = sign * direction.dot(diff.cross(edge2, edge2));
	if (dirDotDiffxEdge2 >= 0.0f) {
		const float dirDotEdge1xDiff = sign * direction.dot(edge1.crossLocal(diff));

		if (dirDotEdge1xDiff >= 0.0f) {
			if (!quad ? dirDotDiffxEdge2 + dirDotEdge1xDiff <= dirDotNorm : dirDotEdge1xDiff <= dirDotNorm) {
				const float diffDotNorm = -sign * diff.dot(norm);
				if (diffDotNorm >= 0.0f) {
					// ray intersects triangle
					// if storage vector is null, just return true,
					if (store == nullptr)
						return true;

					// else fill in.
					const float inv = 1.0f / dirDotNorm;
					const float t = diffDotNorm * inv;
					if (!doPlanar) {
						store->set(origin).addLocal(direction.x * t, direction.y * t, direction.z * t);
					} else {
						// these weights can be used to determine interpolated values, such as texture coord.
						const float w1 = dirDotDiffxEdge2 * inv;
						const float w2 = dirDotEdge1xDiff * inv;
						store->set(t, w1, w2);
					}
					return true;
				}
			}
		}
	}
	return false;
}

float Ray::intersects(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2) const noexcept {
	const float edge1X = v1.x - v0.x;
	const float edge1Y = v1.y - v0.y;
	const float edge1Z = v1.z - v0.z;

	const float edge2X = v2.x - v0.x;
	const float edge2Y = v2.y - v0.y;
	const float edge2Z = v2.z - v0.z;

	const float normX = ((edge1Y * edge2Z) - (edge1Z * edge2Y));
	const float normY = ((edge1Z * edge2X) - (edge1X * edge2Z));
	const float normZ = ((edge1X * edge2Y) - (edge1Y * edge2X));

	float dirDotNorm = direction.x * normX + direction.y * normY + direction.z * normZ;

	const float diffX = origin.x - v0.x;
	const float diffY = origin.y - v0.y;
	const float diffZ = origin.z - v0.z;

	float sign;
	if (dirDotNorm > FastMath::FLOAT_EPSILON) {
		sign = 1;
	} else if (dirDotNorm < -FastMath::FLOAT_EPSILON) {
		sign = -1.0f;
		dirDotNorm = -dirDotNorm;
	} else {
		// ray and triangle/quad are parallel
		return std::numeric_limits<float>::infinity();
	}

	float diffEdge2X = ((diffY * edge2Z) - (diffZ * edge2Y));
	float diffEdge2Y = ((diffZ * edge2X) - (diffX * edge2Z));
	float diffEdge2Z = ((diffX * edge2Y) - (diffY * edge2X));

	const float dirDotDiffxEdge2 = sign * (direction.x * diffEdge2X + direction.y * diffEdge2Y + direction.z * diffEdge2Z);

	if (dirDotDiffxEdge2 >= 0.0f) {
		diffEdge2X = ((edge1Y * diffZ) - (edge1Z * diffY));
		diffEdge2Y = ((edge1Z * diffX) - (edge1X * diffZ));
		diffEdge2Z = ((edge1X * diffY) - (edge1Y * diffX));

		const float dirDotEdge1xDiff = sign * (direction.x * diffEdge2X + direction.y * diffEdge2Y + direction.z * diffEdge2Z);

		if (dirDotEdge1xDiff >= 0.0f) {
			if (dirDotDiffxEdge2 + dirDotEdge1xDiff <= dirDotNorm) {
				const float diffDotNorm = -sign * (diffX * normX + diffY * normY + diffZ * normZ);
				if (diffDotNorm >= 0.0f) {
					// ray intersects triangle
					const float inv = 1.0f / dirDotNorm;
					const float t = diffDotNorm * inv;
					return t;
				}
			}
		}
	}

	return std::numeric_limits<float>::infinity();
}

bool Ray::intersectWherePlanarQuad(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f& loc) const noexcept {
	return intersects(v0, v1, v2, &loc, true, true);
}

float Ray::distanceSquared(const Vector3f& point) const noexcept {
	Vector3f tempVa, tempVb; // Java: TempVars vect1, vect2

	point.subtract(origin, tempVa);
	const float rayParam = direction.dot(tempVa);
	if (rayParam > 0) {
		origin.add(direction.mult(rayParam, tempVb), tempVb);
	} else {
		tempVb.set(origin);
	}

	tempVb.subtract(point, tempVa);
	const float len = tempVa.lengthSquared();
	return len;
}

std::string Ray::toString() const {
	return "Ray [Origin: " + origin.toString() + ", Direction: " + direction.toString() + "]";
}

} // namespace aion::gameserver::geoEngine::math
