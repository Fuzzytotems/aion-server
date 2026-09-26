#pragma once

#include <cstdint>

#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"

namespace aion::gameserver::geoEngine::collision {

/**
 * Interface for collidable objects.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5, §9.2): implemented by Spatial and BoundingVolume. C++ difference: the `other`
 * parameter is `math::Ray&`. Every Java caller passes a Ray (GeoMap, AbstractCollisionObserver, BIHTree, Mesh), and Ray is a value type
 * (fieldmap.toml value_types, geoEngine/math/Ray.h) that cannot implement this interface; the `other instanceof Ray` checks and
 * UnsupportedCollisionException branches of the implementations have no counterpart.
 *
 * @author Kirill
 */
class Collidable {
public:
	/**
	 * Check collision with another collidable
	 *
	 * @return how many collisions were found
	 */
	virtual int32_t collideWith(math::Ray& other, CollisionResults& results) = 0;

	virtual ~Collidable() = default;

protected:
	Collidable() = default;
	Collidable(const Collidable&) = default;
	Collidable& operator=(const Collidable&) = default;
};

} // namespace aion::gameserver::geoEngine::collision
