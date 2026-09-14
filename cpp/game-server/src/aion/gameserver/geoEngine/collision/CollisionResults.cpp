#include "aion/gameserver/geoEngine/collision/CollisionResults.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::geoEngine::collision {

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue, runtime::Ptr<IgnoreProperties> ignorePropertiesValue)
	: CollisionResults(intentionsValue, instanceIdValue, false, ignorePropertiesValue) {
}

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue)
	: CollisionResults(intentionsValue, instanceIdValue, false, nullptr) {
}

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue, bool searchFirst)
	: CollisionResults(intentionsValue, instanceIdValue, searchFirst, nullptr) {
}

CollisionResults::CollisionResults(int8_t intentionsValue, int32_t instanceIdValue, bool searchFirst,
	runtime::Ptr<IgnoreProperties> ignorePropertiesValue)
	: intentions(intentionsValue), instanceId(instanceIdValue), onlyFirst(searchFirst), ignoreProperties(ignorePropertiesValue) {
}

void CollisionResults::clear() {
	AION_UNPORTED();
}

runtime::JavaIterator<CollisionResult> CollisionResults::iterator() {
	AION_UNPORTED();
}

std::vector<CollisionResult>::const_iterator CollisionResults::begin() {
	AION_UNPORTED();
}

void CollisionResults::addCollision(const CollisionResult& result) {
	AION_UNPORTED();
}

int32_t CollisionResults::size() {
	AION_UNPORTED();
}

std::optional<CollisionResult> CollisionResults::getClosestCollision() {
	AION_UNPORTED();
}

std::optional<CollisionResult> CollisionResults::getFarthestCollision() {
	AION_UNPORTED();
}

CollisionResult CollisionResults::getCollision(int32_t index) {
	AION_UNPORTED();
}

CollisionResult CollisionResults::getCollisionDirect(int32_t index) {
	AION_UNPORTED();
}

std::string CollisionResults::toString() const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::collision
