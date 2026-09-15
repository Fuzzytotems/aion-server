#include "aion/gameserver/geoEngine/collision/CollisionResult.h"

#include <functional>

#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"

namespace aion::gameserver::geoEngine::collision {

CollisionResult::CollisionResult(const math::Vector3f& contactPointValue, float distanceValue)
	: contactPoint(contactPointValue), distance(distanceValue) {
}

int32_t CollisionResult::compareTo(const CollisionResult& other) const {
	return math::JavaFloat::compare(distance, other.distance);
}

void CollisionResult::setGeometry(runtime::Ptr<scene::Geometry> geom) {
	geometry = geom;
}

bool CollisionResult::equals(const CollisionResult& obj) const {
	if (this == &obj) {
		return true;
	}

	if (distance != obj.distance || !contactPoint.equals(obj.contactPoint)) {
		return false;
	}
	// Java: Objects.equals(geometry.getName(), obj.getGeometry().getName()) (NullPointerException without a geometry)
	return geometry->getName() == obj.getGeometry()->getName();
}

int32_t CollisionResult::hashCode() const {
	// Java: Objects.hash(contactPoint, distance, geometry) = 31 * (31 * (31 * 1 + contactPoint.hashCode()) + Float.hashCode(distance)) +
	// geometry.hashCode(); Geometry has identity hashing, so its part is any value consistent with identity
	uint32_t result = 1;
	result = 31 * result + static_cast<uint32_t>(contactPoint.hashCode());
	result = 31 * result + static_cast<uint32_t>(math::JavaFloat::floatToIntBits(distance));
	result = 31 * result + (geometry ? static_cast<uint32_t>(std::hash<const scene::Geometry*>()(geometry.get())) : 0u);
	return static_cast<int32_t>(result);
}

} // namespace aion::gameserver::geoEngine::collision
