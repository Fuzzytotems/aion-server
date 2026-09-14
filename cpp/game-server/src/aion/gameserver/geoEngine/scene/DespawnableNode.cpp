#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"

namespace aion::gameserver::geoEngine::scene {

DespawnableNode::DespawnableNode() = default;

DespawnableNode::~DespawnableNode() = default;

runtime::Ref<DespawnableNode> DespawnableNode::create() {
	return runtime::makeRef<DespawnableNode>();
}

void DespawnableNode::setActive(int32_t instanceId, bool active) {
	AION_UNPORTED();
}

bool DespawnableNode::isActive(int32_t instanceId) {
	AION_UNPORTED();
}

void DespawnableNode::copyFrom(Node& node) {
	AION_UNPORTED();
}

int32_t DespawnableNode::collideWith(math::Ray& other, collision::CollisionResults& results) {
	AION_UNPORTED();
}

runtime::Ref<Spatial> DespawnableNode::clone() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::scene
