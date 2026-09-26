#include "aion/gameserver/model/gameobjects/SummonedObject.h"

#include <optional>
#include <string>
#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::model::gameobjects {

namespace {
/** Java `DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId())` in the super(...) call (a null result is Java's null template) */
const templates::npc::NpcTemplate* npcTemplateOf(templates::spawns::SpawnTemplate& spawnTemplate) {
	return dataholders::DataManager::NPC_DATA->getNpcTemplate(spawnTemplate.getNpcId());
}
} // namespace

SummonedObject::SummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	int8_t levelValue, runtime::Ptr<VisibleObject> creatorValue)
	: Npc(key, std::move(controller), spawnTemplate, npcTemplateOf(spawnTemplate)), level(levelValue), creator(creatorValue) {
}

SummonedObject::~SummonedObject() = default;

void SummonedObject::setupStatContainers() {
	// Java: setGameStats(new SummonedObjectGameStats(this)); setLifeStats(new NpcLifeStats(this))
	setGameStats(std::make_unique<stats::container::SummonedObjectGameStats>(*this));
	setLifeStats(std::make_unique<stats::container::NpcLifeStats>(*this));
}

int8_t SummonedObject::getLevel() {
	return level;
}

runtime::Ptr<VisibleObject> SummonedObject::getCreator() {
	return creator;
}

std::optional<std::string> SummonedObject::getMasterName() {
	return !Npc::getMasterName() && creator ? std::optional<std::string>(creator->getName()) : Npc::getMasterName();
}

int32_t SummonedObject::getCreatorId() {
	return Npc::getCreatorId() == 0 && creator ? creator->getObjectId() : Npc::getCreatorId();
}

runtime::Ptr<Creature> SummonedObject::getMaster() {
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(creator)))
		return runtime::cast<Creature>(getCreator());
	return *this;
}

CreatureType SummonedObject::getType(Creature& creature) {
	return creature.isEnemy(*getMaster()) ? CreatureType::ATTACKABLE : CreatureType::SUPPORT;
}

bool SummonedObject::isEnemy(Creature& creature) {
	if (runtime::Ptr<Creature> creatorCreature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(creator)))
		return creatorCreature->isEnemy(creature);
	return Npc::isEnemy(creature);
}

bool SummonedObject::isEnemyFrom(Npc& npc) {
	if (runtime::Ptr<Creature> creatorCreature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(creator)))
		return creatorCreature->isEnemyFrom(npc);
	return Npc::isEnemyFrom(npc);
}

bool SummonedObject::isEnemyFrom(player::Player& player) {
	if (runtime::Ptr<Creature> creatorCreature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(creator)))
		return creatorCreature->isEnemyFrom(player);
	return Npc::isEnemyFrom(player);
}

Race SummonedObject::getRace() {
	runtime::Ptr<Creature> creatorCreature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(creator));
	return creatorCreature ? creatorCreature->getRace() : Npc::getRace();
}

bool SummonedObject::isPvpTarget(Creature& creature) {
	return runtime::as<player::Player>(getActingCreature()) && runtime::as<player::Player>(creature.getActingCreature());
}

} // namespace aion::gameserver::model::gameobjects
