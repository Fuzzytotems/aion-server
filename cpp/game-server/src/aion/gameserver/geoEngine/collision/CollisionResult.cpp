#include "aion/gameserver/geoEngine/collision/CollisionResult.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"

namespace aion::gameserver::geoEngine::collision {

CollisionResult::CollisionResult(const math::Vector3f& contactPointValue, float distanceValue)
	: contactPoint(contactPointValue), distance(distanceValue) {
}

int32_t CollisionResult::compareTo(const CollisionResult& other) const {
	return math::JavaFloat::compare(distance, other.distance);
}

void CollisionResult::setGeometry(runtime::Ptr<scene::Geometry> geom) {
	AION_UNPORTED();
}

bool CollisionResult::equals(const CollisionResult& obj) const {
	AION_UNPORTED();
}

int32_t CollisionResult::hashCode() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::collision
