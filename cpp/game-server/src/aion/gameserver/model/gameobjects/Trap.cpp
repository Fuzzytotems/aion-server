#include "aion/gameserver/model/gameobjects/Trap.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/items/NpcEquippedGear.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects {

namespace {
/** Java `DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId()).getLevel()` in the super(...) call (DataManager holder, not ported yet) */
[[noreturn]] int8_t levelOf(templates::spawns::SpawnTemplate& spawnTemplate) {
	static_cast<void>(spawnTemplate);
	AION_UNPORTED();
}
} // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // the base initializer never returns until the template lookup is ported
#endif
Trap::Trap(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	Creature& creator)
	: SummonedObject(key, std::move(controller), spawnTemplate, levelOf(spawnTemplate), runtime::Ptr<VisibleObject>(creator)) {
	setMasterName(""); // read back as "" by Trap::getMasterName (Npc's Field<std::string> cannot tell "" from null)
	// Java: setKnownlist(new NpcKnownList(this)): world/knownlist/NpcKnownList.h has no declaration header yet (P4-10)
	AION_UNPORTED();
	setEffectController(std::make_unique<controllers::effect::EffectController>(*this));
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

void Trap::setupStatContainers() {
	// Java: setGameStats(new TrapGameStats(this)); setLifeStats(new NpcLifeStats(this)): stats/container/TrapGameStats.h has no declaration header
	// yet (P5-01)
	AION_UNPORTED();
}

int8_t Trap::getLevel() {
	runtime::Ptr<Creature> creatorCreature = runtime::cast<Creature>(getCreator()); // Java: SummonedObject<Creature>.getCreator()
	return !creatorCreature ? int8_t{1} : creatorCreature->getLevel();
}

NpcObjectType Trap::getNpcObjectType() {
	return NpcObjectType::TRAP;
}

std::optional<std::string> Trap::getMasterName() {
	return Npc::getMasterName().value_or(std::string()); // Npc's field: SummonedObject would substitute the creator's name
}

Trap::~Trap() = default;

} // namespace aion::gameserver::model::gameobjects
