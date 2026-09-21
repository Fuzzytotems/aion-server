#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/rift/fwd.h"

namespace aion::gameserver::controllers::standins {

/**
 * C++ only, included by .cpp files of P4-11b (and, until the change request below is done, by AccountService.cpp of P5-00): stand-ins for Java
 * methods whose classes have no C++ declaration header yet, so the controller bodies can be ported line by line against them. Each function
 * names the Java method it stands for and the chunk that writes the header; its body is the unported-stub macro (defined out of line in
 * ControllerStandIns.cpp, so callers never see a no-return call). When the header lands, the call sites switch to the real API and the stand-in
 * is deleted, as the AttackUtil, StatFunctions and GameServer stand-ins were in wave 5a stage 2 (m5a-plan.md W-06).
 */

// ------------------------------------------------------------------------------------------------------------------- P5-05 (aion_gs_ai)

/** Java: AILogger.moveinfo(owner, message) */
void aiLoggerMoveinfo(model::gameobjects::Creature& owner, std::string_view message);

/** Java: TargetEventHandler.onTargetReached(npcAI) */
void targetEventHandlerOnTargetReached(ai::NpcAI& npcAI);

/** Java: WalkManager.stopWalking(npcAI) */
void walkManagerStopWalking(ai::NpcAI& npcAI);

/** Java: FollowStartService.newFollowingToTargetCheckTask(follower, leading) */
runtime::FutureRef followStartServiceNewFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading);

/** Java: ShoutEventHandler.onAttack(npcAI, attacked) */
void shoutEventHandlerOnAttack(ai::NpcAI& npcAI, model::gameobjects::Creature& attacked);

/** Java: ShoutEventHandler.onEnemyAttack(npcAI, attacker) */
void shoutEventHandlerOnEnemyAttack(ai::NpcAI& npcAI, model::gameobjects::Creature& attacker);

// --------------------------------------------------------------------------------------------------------------- P5-02 (aion_gs_skills)

/**
 * Java: `ChargeSkill skill = SkillEngine.getInstance().getChargeSkill(creature, skillId, skillLevel, chargeLevel, startSkill); skill != null &&
 * skill.useSkill()` (ChargeSkill has no header, so its Ref cannot be released here)
 */
bool chargeSkillGetAndUse(model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel, int32_t chargeLevel,
	skillengine::model::Skill& startSkill);

// ---------------------------------------------------------------------------------------------------------------- P5-01 (aion_gs_stats)

/** Java: AttackUtil.calculatePhysAttackResult(attacker, attacked, calculationTypes) */
std::vector<runtime::Ref<attack::AttackResult>> attackUtilCalculatePhysAttackResult(model::gameobjects::Creature& attacker,
	model::gameobjects::Creature& attacked, const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

/** Java: AttackUtil.calculateMagAttackResult(attacker, attacked, element, calculationTypes) */
std::vector<runtime::Ref<attack::AttackResult>> attackUtilCalculateMagAttackResult(model::gameobjects::Creature& attacker,
	model::gameobjects::Creature& attacked, model::SkillElement element,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

/** Java: StatFunctions.calculateExperienceReward(playerLevel, target) */
int64_t statFunctionsCalculateExperienceReward(int32_t playerLevel, model::gameobjects::Npc& target);

/** Java: StatFunctions.calculateDPReward(player, target) */
int32_t statFunctionsCalculateDPReward(model::gameobjects::player::Player& player, model::gameobjects::Creature& target);

/** Java: StatFunctions.calculatePvEApGained(player, target) */
int32_t statFunctionsCalculatePvEApGained(model::gameobjects::player::Player& player, model::gameobjects::Creature& target);

// --------------------------------------------------------------------------------------------------------------- P5-10 (aion_gs_team)

/** Java: PlayerTeamDistributionService.doReward(team, damagePercent, owner, winner, damageList) */
void playerTeamDistributionServiceDoReward(model::team::TemporaryPlayerTeam& team, float damagePercent, model::gameobjects::Npc& owner,
	model::gameobjects::AionObject& winner, attack::TeamDamageList& damageList);

/** Java: PlayerGroupService.removePlayer(player) */
void playerGroupServiceRemovePlayer(model::gameobjects::player::Player& player);

/** Java: PlayerAllianceService.removePlayer(player) */
void playerAllianceServiceRemovePlayer(model::gameobjects::player::Player& player);

/** Java: TeamMoveUpdater.getInstance().add(player) */
void teamMoveUpdaterAdd(model::gameobjects::player::Player& player);

/** Java: TeamStatUpdater.getInstance().add(player) */
void teamStatUpdaterAdd(model::gameobjects::player::Player& player);

// ---------------------------------------------------------------------------------------------------------------- P5-14 (aion_gs_app)

/**
 * Java: GameServer.updateRatio(race, i). No longer a stand-in: GameServer.h exists and this forwards to it. It stays only for its one caller
 * outside P4-11b, AccountService.cpp:66 (P5-00).
 * TODO(change-request): the P5-00 owner calls `GameServer::updateRatio` directly, then this declaration and its body go.
 */
void gameServerUpdateRatio(model::Race race, int32_t i);

// ---------------------------------------------------------------------------------------------------------- P5-12b (aion_gs_worldevents)

/** Java RiftEnum constructor data read by RVController: getEntries, getMinLevel, getMaxLevel, getDestination, isVortex, canBeVolatile, isInvasionRift */
struct RiftEnumData {
	int32_t entries;
	int32_t minLevel;
	int32_t maxLevel;
	model::Race destination;
	bool vortex;
	bool canBeVolatile;
	bool isInvasionRift;
};

/** Java: the RiftEnum accessors above (the RiftEnum companion of P5-12b does not exist yet) */
RiftEnumData riftEnumData(services::rift::RiftEnum riftTemplate);

// ------------------------------------------------------------------------------------------------------------ P5-13 (aion_gs_instance)

/** Java: PlayerRestrictions.canAttack(player, target) */
bool playerRestrictionsCanAttack(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& target);

/** Java: PlayerRestrictions.canUseSkill(player, skill) */
bool playerRestrictionsCanUseSkill(model::gameobjects::player::Player& player, skillengine::model::Skill& skill);

/** Java: PvpMapService.getInstance().isOnPvPMap(creature) */
bool pvpMapServiceIsOnPvPMap(model::gameobjects::Creature& creature);

/**
 * Java: `instanceHandler instanceof PvpMapHandler`. Not unported: the check compares the simple class name of the dynamic type (the only Java
 * class of that name), so it works before PvpMapHandler.h exists.
 */
bool isPvpMapHandler(instance::handlers::InstanceHandler& instanceHandler);

} // namespace aion::gameserver::controllers::standins
