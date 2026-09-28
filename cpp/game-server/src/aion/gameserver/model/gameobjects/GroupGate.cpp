#include "aion/gameserver/model/gameobjects/GroupGate.h"

#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::gameobjects {

GroupGate::GroupGate(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	Creature& creator)
	: SummonedObject(key, std::move(controller), spawnTemplate, int8_t{1}, runtime::Ptr<VisibleObject>(creator)) {
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
	setEffectController(std::make_unique<controllers::effect::EffectController>(*this));
}

GroupGate::~GroupGate() = default;

NpcObjectType GroupGate::getNpcObjectType() {
	return NpcObjectType::GROUPGATE;
}

} // namespace aion::gameserver::model::gameobjects
