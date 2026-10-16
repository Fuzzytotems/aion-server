#include "aion/gameserver/ai/NpcAI.h"

#include <memory>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/handler/ActivateEventHandler.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"
#include "aion/gameserver/ai/handler/DiedEventHandler.h"
#include "aion/gameserver/ai/handler/FollowEventHandler.h"
#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/ai/handler/SpawnEventHandler.h"
#include "aion/gameserver/ai/manager/SimpleAttackManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/NpcShoutsService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldType.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::ai {

const std::set<model::Race> NpcAI::apRewardingRaces = {model::Race::ASMODIANS, model::Race::DARK, model::Race::DRAGON, model::Race::DRAGONET,
	model::Race::DRAKAN, model::Race::ELYOS, model::Race::GCHIEF_DARK, model::Race::GCHIEF_DRAGON, model::Race::GCHIEF_LIGHT,
	model::Race::GHENCHMAN_DARK, model::Race::GHENCHMAN_LIGHT, model::Race::LIGHT, model::Race::LIZARDMAN, model::Race::NAGA,
	model::Race::SIEGEDRAKAN};

NpcAI::NpcAI(model::gameobjects::Npc& ownerValue) : AITemplate(ownerValue) {
}

const model::templates::npc::NpcTemplate* NpcAI::getObjectTemplate() {
	return getOwner().getObjectTemplate();
}

runtime::Ptr<model::templates::spawns::SpawnTemplate> NpcAI::getSpawnTemplate() {
	return getOwner().getSpawn();
}

runtime::Ptr<model::stats::container::NpcLifeStats> NpcAI::getLifeStats() {
	return getOwner().getLifeStats();
}

model::Race NpcAI::getRace() {
	return getOwner().getRace();
}

std::optional<model::TribeClass> NpcAI::getTribe() {
	return getOwner().getTribe();
}

runtime::Ptr<controllers::effect::EffectController> NpcAI::getEffectController() {
	return getOwner().getEffectController();
}

world::knownlist::KnownList& NpcAI::getKnownList() {
	return getOwner().getKnownList();
}

controllers::attack::AggroList& NpcAI::getAggroList() {
	return getOwner().getAggroList();
}

runtime::Ptr<model::skill::NpcSkillList> NpcAI::getSkillList() {
	return getOwner().getSkillList();
}

runtime::Ptr<model::gameobjects::VisibleObject> NpcAI::getCreator() {
	return getOwner().getCreator();
}

runtime::Ptr<controllers::movement::NpcMoveController> NpcAI::getMoveController() {
	return getOwner().getMoveController();
}

int32_t NpcAI::getNpcId() {
	return getOwner().getNpcId();
}

int32_t NpcAI::getCreatorId() {
	return getOwner().getCreatorId();
}

bool NpcAI::isInRange(model::gameobjects::VisibleObject& object, int32_t range) {
	return utils::PositionUtil::isInRange(getOwner(), object, static_cast<float>(range));
}

void NpcAI::handleActivate() {
	handler::ActivateEventHandler::onActivate(*this);
}

void NpcAI::handleDeactivate() {
	if (configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load() && runtime::as<model::gameobjects::siege::SiegeNpc>(getOwner()) ||
		getOwner().isRaidMonster())
		return;
	handler::ActivateEventHandler::onDeactivate(*this);
}

void NpcAI::handleBeforeSpawned() {
	handler::SpawnEventHandler::onBeforeSpawn(*this);
}

void NpcAI::handleSpawned() {
	handler::SpawnEventHandler::onSpawn(*this);
	handler::ShoutEventHandler::onSpawn(*this);
}

void NpcAI::handleDespawned() {
	handler::ShoutEventHandler::onBeforeDespawn(*this);
	handler::SpawnEventHandler::onDespawn(*this);
}

void NpcAI::handleDied() {
	handler::DiedEventHandler::onDie(*this);
}

void NpcAI::handleMoveArrived() {
	handler::ShoutEventHandler::onReachedWalkPoint(*this);
}

void NpcAI::handleTargetChanged(model::gameobjects::Creature& creature) {
	handler::ShoutEventHandler::onSwitchedTarget(*this, creature);
}

bool NpcAI::ask(poll::AIQuestion question) {
	switch (question) {
		case poll::AIQuestion::CAN_SHOUT:
			return configs::main::AIConfig::SHOUTS_ENABLE.load() && services::NpcShoutsService::getInstance().mayShout(getOwner());
		case poll::AIQuestion::ALLOW_RESPAWN:
			return services::SiegeService::getInstance().isRespawnAllowed(getOwner());
		case poll::AIQuestion::ALLOW_DECAY:
		case poll::AIQuestion::REWARD_AP_XP_DP_LOOT:
		case poll::AIQuestion::REWARD_LOOT:
			return true;
		case poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return getOwner().isBoss() || getOwner().hasStatic();
		case poll::AIQuestion::REWARD_AP: {
			world::WorldType wt = getOwner().getWorldType();
			return wt == world::WorldType::ABYSS ||
				wt != world::WorldType::ELYSEA && wt != world::WorldType::ASMODAE && apRewardingRaces.contains(getRace());
		}
		case poll::AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE:
			return !getOwner().isInInstance();
		default:
			return false;
	}
}

bool NpcAI::isDestinationReached() {
	switch (getState()) {
		case AIState::CONFUSE:
		case AIState::FEAR:
			return utils::PositionUtil::isInRange(getOwner(), getOwner().getMoveController()->getTargetX2(),
				getOwner().getMoveController()->getTargetY2(), getOwner().getMoveController()->getTargetZ2(), 1);
		case AIState::FIGHT:
			return manager::SimpleAttackManager::isTargetInAttackRange(getOwner());
		case AIState::RETURNING: {
			runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = getOwner().getSpawn();
			return utils::PositionUtil::isInRange(getOwner(), spawn->getX(), spawn->getY(), spawn->getZ(), 1);
		}
		case AIState::FOLLOWING:
			return handler::FollowEventHandler::isInRange(*this, getOwner().getTarget());
		case AIState::WALKING:
		case AIState::FORCED_WALKING:
			return getSubState() == AISubState::TALK || manager::WalkManager::isArrivedAtPoint(*this);
		default:
			return true;
	}
}

void NpcAI::handleMoveValidate() {
	handler::MoveEventHandler::onMoveValidate(*this);
}

void NpcAI::handleCreatureMoved(model::gameobjects::Creature& creature) {
	handler::CreatureEventHandler::onCreatureMoved(*this, creature);
}

bool NpcAI::isMoveSupported() {
	return getOwner().getGameStats()->getMovementSpeed()->getCurrent() > 0 && !isInSubState(AISubState::FREEZE);
}

} // namespace aion::gameserver::ai
