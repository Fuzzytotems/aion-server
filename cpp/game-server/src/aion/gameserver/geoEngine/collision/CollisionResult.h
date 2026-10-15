#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::collision {

/**
 * A collision found by a ray test: contact point, distance and the collided geometry.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the element type of CollisionResults. K5 confined value class (fieldmap), copied
 * by value; the contact point is a Vector3f value, so getContactPoint() returns a copy (Java returns the shared vector, which its callers
 * modify only after they are done with the results). compareTo (Float.compare of the distances) is ported for sorting.
 *
 * @author Kirill
 */
class CollisionResult {
private:
	math::Vector3f contactPoint;
	float distance;
	runtime::Ptr<scene::Geometry> geometry;

public:
	CollisionResult(const math::Vector3f& contactPoint, float distance);

	int32_t compareTo(const CollisionResult& other) const;

	void setGeometry(runtime::Ptr<scene::Geometry> geom);

	math::Vector3f getContactPoint() const { return contactPoint; }

	runtime::Ptr<scene::Geometry> getGeometry() const { return geometry; }

	float getDistance() const { return distance; }

	bool equals(const CollisionResult& obj) const;

	int32_t hashCode() const;
};

} // namespace aion::gameserver::geoEngine::collision
