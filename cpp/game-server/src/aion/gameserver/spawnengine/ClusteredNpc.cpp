#include "aion/gameserver/spawnengine/ClusteredNpc.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::spawnengine {

ClusteredNpc::ClusteredNpc(model::gameobjects::Npc& npcValue, int32_t instanceValue,
	const model::templates::walker::WalkerTemplate* walkTemplateValue)
	: WalkerGroupShift(0, 0), npc(runtime::Ref<model::gameobjects::Npc>(npcValue)), instance(instanceValue), walkTemplate(walkTemplateValue),
	  x(npcValue.getSpawn()->getX()), y(npcValue.getSpawn()->getY()) {
}

ClusteredNpc::~ClusteredNpc() = default;

runtime::Ref<ClusteredNpc> ClusteredNpc::create(model::gameobjects::Npc& npcValue, int32_t instanceValue,
	const model::templates::walker::WalkerTemplate* walkTemplateValue) {
	return runtime::makeRef<ClusteredNpc>(npcValue, instanceValue, walkTemplateValue);
}

void ClusteredNpc::spawn(float z) {
	AION_UNPORTED();
}

void ClusteredNpc::despawn() {
	AION_UNPORTED();
}

void ClusteredNpc::setNpc(model::gameobjects::Npc& npcValue, const model::templates::walker::RouteStep* step) {
	AION_UNPORTED();
}

bool ClusteredNpc::hasSamePosition(runtime::Ptr<ClusteredNpc> other) {
	AION_UNPORTED();
}

int32_t ClusteredNpc::getPositionHash() {
	AION_UNPORTED();
}

float ClusteredNpc::getXDelta() {
	AION_UNPORTED();
}

float ClusteredNpc::getYDelta() {
	AION_UNPORTED();
}

std::optional<int32_t> ClusteredNpc::getWalkerIndex() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::spawnengine
