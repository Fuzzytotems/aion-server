#include "aion/gameserver/model/gameobjects/SummonedHouseNpc.h"

#include <string>
#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::gameobjects {

namespace {

/** Java `DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId()).getLevel()` in the super(...) call (NullPointerException without a template) */
int8_t levelOf(templates::spawns::SpawnTemplate& spawnTemplate) {
	const templates::npc::NpcTemplate* npcTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(spawnTemplate.getNpcId());
	if (npcTemplate == nullptr) // Java: NullPointerException on getLevel()
		throw runtime::NullPointerException("No npc template for house npc " + std::to_string(spawnTemplate.getNpcId()));
	return npcTemplate->getLevel();
}

} // namespace

SummonedHouseNpc::SummonedHouseNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	house::House& house)
	: SummonedObject(key, std::move(controller), spawnTemplate, levelOf(spawnTemplate), runtime::Ptr<VisibleObject>(house)) {
	std::optional<std::string> ownerName = house.getOwnerName();
	setMasterName(!ownerName ? "" : *ownerName);
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
	setEffectController(std::make_unique<controllers::effect::EffectController>(*this));
}

SummonedHouseNpc::~SummonedHouseNpc() = default;

int32_t SummonedHouseNpc::getCreatorId() {
	return runtime::cast<house::House>(getCreator())->getAddress()->getId();
}

bool SummonedHouseNpc::isEnemy(Creature& creature) {
	return false;
}

bool SummonedHouseNpc::isEnemyFrom(Npc& npc) {
	return false;
}

bool SummonedHouseNpc::isEnemyFrom(player::Player& player) {
	return false;
}

CreatureType SummonedHouseNpc::getType(Creature& creature) {
	return CreatureType::FRIEND;
}

std::optional<std::string> SummonedHouseNpc::getMasterName() {
	return Npc::getMasterName().value_or(std::string()); // Npc's field: SummonedObject would substitute the creator's name
}

} // namespace aion::gameserver::model::gameobjects
