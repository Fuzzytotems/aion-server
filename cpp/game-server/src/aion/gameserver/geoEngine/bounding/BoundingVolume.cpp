#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"

#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::bounding {

BoundingVolume::BoundingVolume() = default;

BoundingVolume::BoundingVolume(const math::Vector3f& centerValue) : center(centerValue) {
}

BoundingVolume::~BoundingVolume() = default;

math::Vector3f BoundingVolume::getCenter(math::Vector3f& store) {
	store.set(center.get());
	return store;
}

float BoundingVolume::distanceTo(const math::Vector3f& point) {
	return center.get().distance(point);
}

float BoundingVolume::distanceSquaredTo(const math::Vector3f& point) {
	return center.get().distanceSquared(point);
}

} // namespace aion::gameserver::geoEngine::bounding
