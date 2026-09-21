#include "aion/gameserver/controllers/ControllerStandIns.h"

#include <typeinfo>

#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"

namespace aion::gameserver::controllers::standins {

void aiLoggerMoveinfo(model::gameobjects::Creature& owner, std::string_view message) {
	AION_UNPORTED();
}

void targetEventHandlerOnTargetReached(ai::NpcAI& npcAI) {
	AION_UNPORTED();
}

void walkManagerStopWalking(ai::NpcAI& npcAI) {
	AION_UNPORTED();
}

void shoutEventHandlerOnEnemyAttack(ai::NpcAI& npcAI, model::gameobjects::Creature& attacker) {
	AION_UNPORTED();
}

int64_t statFunctionsCalculateExperienceReward(int32_t playerLevel, model::gameobjects::Npc& target) {
	AION_UNPORTED();
}

int32_t statFunctionsCalculateDPReward(model::gameobjects::player::Player& player, model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

int32_t statFunctionsCalculatePvEApGained(model::gameobjects::player::Player& player, model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

bool chargeSkillGetAndUse(model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel, int32_t chargeLevel,
	skillengine::model::Skill& startSkill) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<attack::AttackResult>> attackUtilCalculatePhysAttackResult(model::gameobjects::Creature& attacker,
	model::gameobjects::Creature& attacked, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<attack::AttackResult>> attackUtilCalculateMagAttackResult(model::gameobjects::Creature& attacker,
	model::gameobjects::Creature& attacked, model::SkillElement element,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

void playerTeamDistributionServiceDoReward(model::team::TemporaryPlayerTeam& team, float damagePercent, model::gameobjects::Npc& owner,
	model::gameobjects::AionObject& winner, attack::TeamDamageList& damageList) {
	AION_UNPORTED();
}

runtime::FutureRef followStartServiceNewFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading) {
	AION_UNPORTED();
}

void shoutEventHandlerOnAttack(ai::NpcAI& npcAI, model::gameobjects::Creature& attacked) {
	AION_UNPORTED();
}

void playerGroupServiceRemovePlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void playerAllianceServiceRemovePlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

RiftEnumData riftEnumData(services::rift::RiftEnum riftTemplate) {
	AION_UNPORTED();
}

void teamMoveUpdaterAdd(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void teamStatUpdaterAdd(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void gameServerUpdateRatio(model::Race race, int32_t i) {
	GameServer::updateRatio(race, i);
}

bool playerRestrictionsCanAttack(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& target) {
	AION_UNPORTED();
}

bool playerRestrictionsCanUseSkill(model::gameobjects::player::Player& player, skillengine::model::Skill& skill) {
	AION_UNPORTED();
}

bool pvpMapServiceIsOnPvPMap(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool isPvpMapHandler(instance::handlers::InstanceHandler& instanceHandler) {
	return utils::simpleClassName(typeid(instanceHandler)) == "PvpMapHandler";
}

} // namespace aion::gameserver::controllers::standins
