#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::geoEngine::bounding {

BoundingVolume::BoundingVolume() = default;

BoundingVolume::BoundingVolume(const math::Vector3f& centerValue) : center(centerValue) {
}

BoundingVolume::~BoundingVolume() = default;

math::Vector3f BoundingVolume::getCenter(math::Vector3f& store) {
	AION_UNPORTED();
}

float BoundingVolume::distanceTo(const math::Vector3f& point) {
	AION_UNPORTED();
}

float BoundingVolume::distanceSquaredTo(const math::Vector3f& point) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::bounding
