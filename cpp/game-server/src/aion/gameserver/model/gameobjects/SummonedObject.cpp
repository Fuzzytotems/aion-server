#include "aion/gameserver/model/gameobjects/SummonedObject.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::model::gameobjects {

namespace {
/** Java `DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId())` in the super(...) call (DataManager holder, not ported yet) */
[[noreturn]] const templates::npc::NpcTemplate* npcTemplateOf(templates::spawns::SpawnTemplate& spawnTemplate) {
	static_cast<void>(spawnTemplate);
	AION_UNPORTED();
}
} // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // the base initializer never returns until the template lookup is ported
#endif
SummonedObject::SummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	int8_t levelValue, runtime::Ptr<VisibleObject> creatorValue)
	: Npc(key, std::move(controller), spawnTemplate, npcTemplateOf(spawnTemplate)), level(levelValue), creator(creatorValue) {
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

SummonedObject::~SummonedObject() = default;

void SummonedObject::setupStatContainers() {
	AION_UNPORTED();
}

int8_t SummonedObject::getLevel() {
	return level;
}

runtime::Ptr<VisibleObject> SummonedObject::getCreator() {
	return creator;
}

std::optional<std::string> SummonedObject::getMasterName() {
	AION_UNPORTED();
}

int32_t SummonedObject::getCreatorId() {
	AION_UNPORTED();
}

runtime::Ptr<Creature> SummonedObject::getMaster() {
	AION_UNPORTED();
}

CreatureType SummonedObject::getType(Creature& creature) {
	AION_UNPORTED();
}

bool SummonedObject::isEnemy(Creature& creature) {
	AION_UNPORTED();
}

bool SummonedObject::isEnemyFrom(Npc& npc) {
	AION_UNPORTED();
}

bool SummonedObject::isEnemyFrom(player::Player& player) {
	AION_UNPORTED();
}

Race SummonedObject::getRace() {
	AION_UNPORTED();
}

bool SummonedObject::isPvpTarget(Creature& creature) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
