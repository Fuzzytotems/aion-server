#pragma once

#include <limits>
#include <string>

#include "aion/gameserver/geoEngine/math/Vector3f.h"

namespace aion::gameserver::geoEngine::math {

/**
 * Java: com.aionemu.gameserver.geoEngine.math.Ray (jMonkeyEngine) - a ray R(t) = origin + t * direction for t >= 0, limited to `limit`.
 * <p>
 * Porting notes
 * - Value type (trivially copyable) holding copies of origin and direction. Java's Ray(Vector3f, Vector3f) keeps references to the caller's
 *   vectors; the only callers (GeoMap, AbstractCollisionObserver, BIHNode) do not use those vectors while the ray is in use (BIHNode transforms
 *   the ray's own origin/direction in place through getOrigin()/getDirection() and restores them with setOrigin/setDirection afterwards).
 *   getOrigin()/getDirection() return references, so in-place updates like `inv.mult(r.getOrigin(), r.getOrigin())` work as in Java.
 * - The TempVars of the Java implementation are plain locals (same values, no thread-local pool).
 * - collideWith(Collidable, CollisionResults) is not part of this leaf library: it only dispatches to BoundingVolume.collideWith(Ray), which
 *   the geo engine port (P4-04) calls directly.
 *
 * @author Mark Powell
 * @author Joshua Slack
 */
class Ray {
public:
	/** The ray's beginning point. */
	Vector3f origin;
	/** The direction of the ray. */
	Vector3f direction;

	float limit = std::numeric_limits<float>::infinity();

	/** Origin (0,0,0) and direction (0,0,0). */
	constexpr Ray() noexcept = default;
	constexpr Ray(const Vector3f& rayOrigin, const Vector3f& rayDirection) noexcept : origin(rayOrigin), direction(rayDirection) {}

	/**
	 * Determines if the ray intersects the triangle v0, v1, v2 (from either side; parallel rays within FLT_EPSILON never hit) and if so stores
	 * the point of intersection in loc (loc is unchanged on a miss). The limit is not checked.
	 */
	bool intersectWhere(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f& loc) const noexcept;

	/** Java: intersectWhere(v0, v1, v2, null) - only the boolean result. */
	bool intersectWhere(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2) const noexcept;

	/**
	 * The ray parameter t of the intersection with the triangle v0, v1, v2, or positive infinity if there is none (the limit is not checked).
	 */
	float intersects(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2) const noexcept;

	/**
	 * Determines if the ray intersects the quad with the edges [v0,v1] and [v0,v2] and if so stores (t, u, v) in loc: the distance factor and
	 * the intersection point in terms of the quad plane.
	 */
	bool intersectWherePlanarQuad(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f& loc) const noexcept;

	/** The squared distance from point to the closest point of the (unlimited) ray. */
	float distanceSquared(const Vector3f& point) const noexcept;

	constexpr Vector3f& getOrigin() noexcept { return origin; }
	constexpr const Vector3f& getOrigin() const noexcept { return origin; }
	constexpr void setOrigin(const Vector3f& newOrigin) noexcept { this->origin.set(newOrigin); }

	/** The limit (length) of the ray; infinity for an unlimited ray. */
	constexpr float getLimit() const noexcept { return limit; }
	constexpr void setLimit(float newLimit) noexcept { this->limit = newLimit; }

	constexpr Vector3f& getDirection() noexcept { return direction; }
	constexpr const Vector3f& getDirection() const noexcept { return direction; }
	constexpr void setDirection(const Vector3f& newDirection) noexcept { this->direction.set(newDirection); }

	/** Copies origin and direction (not the limit, as in Java). */
	constexpr void set(const Ray& source) noexcept {
		origin.set(source.getOrigin());
		direction.set(source.getDirection());
	}

	/** "Ray [Origin: (x, y, z), Direction: (x, y, z)]" */
	std::string toString() const;

	constexpr Ray clone() const noexcept { return *this; }

private:
	bool intersects(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, Vector3f* store, bool doPlanar, bool quad) const noexcept;
};

} // namespace aion::gameserver::geoEngine::math
