#include "aion/gameserver/model/gameobjects/Servant.h"

#include <string>
#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::model::gameobjects {

Servant::Servant(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
	Creature& creator)
	: SummonedObject(key, std::move(controller), spawnTemplate, level, runtime::Ptr<VisibleObject>(creator)) {
	setMasterName("");
	setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*this));
	setEffectController(std::make_unique<controllers::effect::EffectController>(*this));
}

Servant::~Servant() = default;

void Servant::setupStatContainers() {
	// Java: setGameStats(new ServantGameStats(this)); setLifeStats(new NpcLifeStats(this)): stats/container/ServantGameStats.h has no declaration
	// header yet (P5-01)
	AION_UNPORTED();
}

NpcObjectType Servant::getNpcObjectType() {
	return objectType.get();
}

void Servant::setUpStats() {
	// Java: ((ServantGameStats) getGameStats()).setUpStats(): stats/container/ServantGameStats.h has no declaration header yet (P5-01)
	AION_UNPORTED();
}

std::optional<std::string> Servant::getMasterName() {
	return Npc::getMasterName().value_or(std::string()); // Npc's field: SummonedObject would substitute the creator's name
}

} // namespace aion::gameserver::model::gameobjects
