#include "aion/gameserver/controllers/SiegeWeaponController.h"

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers {

using ai::event::AIEventType;
using model::Race;
using model::TaskId;
using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using runtime::Ptr;

SiegeWeaponController::SiegeWeaponController(int32_t npcId) : skills(dataholders::DataManager::NPC_SKILL_DATA->getNpcSkillList(npcId)) {
}

SiegeWeaponController::~SiegeWeaponController() = default;

void SiegeWeaponController::release(model::summons::UnsummonType unsummonType) {
	getMaster()->getController().cancelTask(TaskId::SUMMON_FOLLOW);
	getOwner().getMoveController()->abortMove();
	SummonController::release(unsummonType);
}

void SiegeWeaponController::restMode() {
	getMaster()->getController().cancelTask(TaskId::SUMMON_FOLLOW);
	SummonController::restMode();
	getOwner().getAi().onCreatureEvent(AIEventType::STOP_FOLLOW_ME, *getMaster());
}

void SiegeWeaponController::setUnkMode() {
	SummonController::setUnkMode();
	getMaster()->getController().cancelTask(TaskId::SUMMON_FOLLOW);
}

void SiegeWeaponController::guardMode() {
	SummonController::guardMode();
	getMaster()->getController().cancelTask(TaskId::SUMMON_FOLLOW);
	getOwner().setTarget(getMaster());
	getOwner().getAi().onCreatureEvent(AIEventType::FOLLOW_ME, *getMaster());
	getOwner().getMoveController()->moveToTargetObject();
	getMaster()->getController().addTask(TaskId::SUMMON_FOLLOW, standins::followStartServiceNewFollowingToTargetCheckTask(getOwner(), *getMaster()));
}

void SiegeWeaponController::attackMode(int32_t targetObjId) {
	Ptr<Creature> target = runtime::cast<Creature>(getOwner().getKnownList().getObject(targetObjId));
	if (!target || !world::geo::GeoService::getInstance().canSee(getOwner(), *target)) {
		return;
	}
	if (isValidTarget(*target)) {
		SummonController::attackMode(targetObjId);
		getOwner().setTarget(target);
		getOwner().getAi().onCreatureEvent(AIEventType::FOLLOW_ME, *target);
		getOwner().getMoveController()->moveToTargetObject();
		getMaster()->getController().addTask(TaskId::SUMMON_FOLLOW, standins::followStartServiceNewFollowingToTargetCheckTask(getOwner(), *target));
	}
}

bool SiegeWeaponController::isValidTarget(model::gameobjects::Creature& target) {
	Ptr<Player> master = runtime::cast<Player>(getOwner().getMaster());
	if (!master) {
		return false;
	}
	Race masterRace = master->getRace();
	if (!isBalaurBoss(target)) {
		if (masterRace == Race::ASMODIANS && target.getRace() != Race::PC_LIGHT_CASTLE_DOOR && target.getRace() != Race::DRAGON_CASTLE_DOOR &&
			target.getRace() != Race::GCHIEF_LIGHT && target.getRace() != Race::GCHIEF_DRAGON) {
			return false;
		} else if (masterRace == Race::ELYOS && target.getRace() != Race::PC_DARK_CASTLE_DOOR && target.getRace() != Race::DRAGON_CASTLE_DOOR &&
			target.getRace() != Race::GCHIEF_DARK && target.getRace() != Race::GCHIEF_DRAGON) {
			return false;
		}
	}
	return true;
}

bool SiegeWeaponController::isBalaurBoss(model::gameobjects::Creature& creature) {
	Ptr<model::gameobjects::siege::SiegeNpc> siegeNpc;
	return creature.getRace() == Race::DRAKAN && (siegeNpc = runtime::as<model::gameobjects::siege::SiegeNpc>(creature)) &&
		siegeNpc->getObjectTemplate()->getRating() == model::templates::npc::NpcRating::LEGENDARY;
}

void SiegeWeaponController::onDie(model::gameobjects::Creature& lastAttacker) {
	getMaster()->getController().cancelTask(TaskId::SUMMON_FOLLOW);
	SummonController::onDie(lastAttacker);
}

} // namespace aion::gameserver::controllers
