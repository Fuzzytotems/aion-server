#include "aion/gameserver/ai/NpcAI.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::ai {

const std::set<model::Race> NpcAI::apRewardingRaces = {model::Race::ASMODIANS, model::Race::DARK, model::Race::DRAGON, model::Race::DRAGONET,
	model::Race::DRAKAN, model::Race::ELYOS, model::Race::GCHIEF_DARK, model::Race::GCHIEF_DRAGON, model::Race::GCHIEF_LIGHT,
	model::Race::GHENCHMAN_DARK, model::Race::GHENCHMAN_LIGHT, model::Race::LIGHT, model::Race::LIZARDMAN, model::Race::NAGA,
	model::Race::SIEGEDRAKAN};

NpcAI::NpcAI(model::gameobjects::Npc& ownerValue) : AITemplate(ownerValue) {
}

const model::templates::npc::NpcTemplate* NpcAI::getObjectTemplate() {
	AION_UNPORTED();
}

runtime::Ptr<model::templates::spawns::SpawnTemplate> NpcAI::getSpawnTemplate() {
	AION_UNPORTED();
}

runtime::Ptr<model::stats::container::NpcLifeStats> NpcAI::getLifeStats() {
	AION_UNPORTED();
}

model::Race NpcAI::getRace() {
	AION_UNPORTED();
}

model::TribeClass NpcAI::getTribe() {
	AION_UNPORTED();
}

runtime::Ptr<controllers::effect::EffectController> NpcAI::getEffectController() {
	AION_UNPORTED();
}

world::knownlist::KnownList& NpcAI::getKnownList() {
	AION_UNPORTED();
}

controllers::attack::AggroList& NpcAI::getAggroList() {
	AION_UNPORTED();
}

model::skill::NpcSkillList& NpcAI::getSkillList() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> NpcAI::getCreator() {
	AION_UNPORTED();
}

runtime::Ptr<controllers::movement::NpcMoveController> NpcAI::getMoveController() {
	AION_UNPORTED();
}

int32_t NpcAI::getNpcId() {
	AION_UNPORTED();
}

int32_t NpcAI::getCreatorId() {
	AION_UNPORTED();
}

bool NpcAI::isInRange(model::gameobjects::VisibleObject& object, int32_t range) {
	AION_UNPORTED();
}

void NpcAI::handleActivate() {
	AION_UNPORTED();
}

void NpcAI::handleDeactivate() {
	AION_UNPORTED();
}

void NpcAI::handleBeforeSpawned() {
	AION_UNPORTED();
}

void NpcAI::handleSpawned() {
	AION_UNPORTED();
}

void NpcAI::handleDespawned() {
	AION_UNPORTED();
}

void NpcAI::handleDied() {
	AION_UNPORTED();
}

void NpcAI::handleMoveArrived() {
	AION_UNPORTED();
}

void NpcAI::handleTargetChanged(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool NpcAI::ask(poll::AIQuestion question) {
	AION_UNPORTED();
}

bool NpcAI::isDestinationReached() {
	AION_UNPORTED();
}

void NpcAI::handleMoveValidate() {
	AION_UNPORTED();
}

void NpcAI::handleCreatureMoved(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool NpcAI::isMoveSupported() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::ai
