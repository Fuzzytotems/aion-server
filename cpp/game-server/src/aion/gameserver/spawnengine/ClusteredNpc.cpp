#include "aion/gameserver/spawnengine/ClusteredNpc.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"

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
	runtime::Ptr<model::gameobjects::Npc> current = npc.get();
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = current->getSpawn();
	SpawnEngine::bringIntoWorld(*current, spawn->getWorldId(), instance, x.get(), y.get(), z, spawn->getHeading());
}

void ClusteredNpc::despawn() {
	runtime::Ptr<model::gameobjects::Npc> current = npc.get();
	current->getMoveController()->abortMove();
	current->getController().deleteIfAliveOrCancelRespawn();
}

void ClusteredNpc::setNpc(model::gameobjects::Npc& npcValue, const model::templates::walker::RouteStep* step) {
	npc.set(runtime::Ptr<model::gameobjects::Npc>(npcValue));
	x.set(step->getX());
	y.set(step->getY());
}

bool ClusteredNpc::hasSamePosition(runtime::Ptr<ClusteredNpc> other) {
	if (this == other.get())
		return true;
	if (!other)
		return false;
	return x.get() == other->x.get() && y.get() == other->y.get();
}

int32_t ClusteredNpc::getPositionHash() {
	constexpr uint32_t prime = 31;
	uint32_t result = 1;
	result = prime * result + static_cast<uint32_t>(geoEngine::math::JavaFloat::floatToIntBits(x.get()));
	result = prime * result + static_cast<uint32_t>(geoEngine::math::JavaFloat::floatToIntBits(y.get()));
	return static_cast<int32_t>(result);
}

float ClusteredNpc::getXDelta() {
	return walkTemplate->getRouteStep(0)->getX() - x.get();
}

float ClusteredNpc::getYDelta() {
	return walkTemplate->getRouteStep(0)->getY() - y.get();
}

std::optional<int32_t> ClusteredNpc::getWalkerIndex() {
	return npc.get()->getSpawn()->getWalkerIndex();
}

} // namespace aion::gameserver::spawnengine
