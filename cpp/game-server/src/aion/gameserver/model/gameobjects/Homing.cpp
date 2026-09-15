#include "aion/gameserver/model/gameobjects/Homing.h"

#include <string>
#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable until the known list header exists
#endif
Homing::Homing(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
	Creature& creator, int32_t skillIdValue)
	: SummonedObject(key, std::move(controller), spawnTemplate, level, runtime::Ptr<VisibleObject>(creator)), skillId(skillIdValue),
	  attackType(findAttackType()) {
	setMasterName("");
	// Java: setKnownlist(new NpcKnownList(this)): world/knownlist/NpcKnownList.h has no declaration header yet (P4-10)
	AION_UNPORTED();
	setEffectController(std::make_unique<controllers::effect::EffectController>(*this));
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

Homing::~Homing() = default;

void Homing::setupStatContainers() {
	// Java: setGameStats(new HomingGameStats(this)); setLifeStats(new NpcLifeStats(this)): stats/container/HomingGameStats.h has no declaration
	// header yet (P5-01)
	AION_UNPORTED();
}

NpcObjectType Homing::getNpcObjectType() {
	return NpcObjectType::HOMING;
}

templates::item::ItemAttackType Homing::getAttackType() {
	return attackType;
}

std::optional<std::string> Homing::getMasterName() {
	return Npc::getMasterName().value_or(std::string()); // Npc's field: SummonedObject would substitute the creator's name
}

templates::item::ItemAttackType Homing::findAttackType() {
	std::string name = getName();
	if (name.contains("fire"))
		return templates::item::ItemAttackType::MAGICAL_FIRE;
	else if (name.contains("stone") || name == "gryphu")
		return templates::item::ItemAttackType::MAGICAL_EARTH;
	else if (name.contains("water"))
		return templates::item::ItemAttackType::MAGICAL_WATER;
	else if ((name.contains("wind")) || (name.contains("cyclone")) || (name.contains("elemental")))
		return templates::item::ItemAttackType::MAGICAL_WIND;
	return templates::item::ItemAttackType::PHYSICAL;
}

} // namespace aion::gameserver::model::gameobjects
