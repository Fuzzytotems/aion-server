#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
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

// m5b-1 E-01a (header-request m5b-2): `aiLoggerMoveinfo`, `targetEventHandlerOnTargetReached`, `walkManagerStopWalking`,
// `shoutEventHandlerOnAttack` and `shoutEventHandlerOnEnemyAttack` are gone - `ai/AILogger.h`, `ai/handler/TargetEventHandler.h`,
// `ai/manager/WalkManager.h` and `ai/handler/ShoutEventHandler.h` exist, and NpcMoveController, NpcController and PlayerController call them
// the way Java writes them.
//
// m5b-1 E-01b, stage 1 (header-request m5b-3, and m5b-plan.md §4 E-01b moved forward from stage 2): `attackUtilCalculatePhysAttackResult`,
// `statFunctionsCalculateExperienceReward`, `statFunctionsCalculateDPReward`, `statFunctionsCalculatePvEApGained` and
// `playerRestrictionsCanAttack` are gone too - B-01, B-02/B-03 and C-01 landed in the same stage, and A-06's registered AggressiveNpcAI made the
// first of them live (a Poeta monster aggroes the gate's character and swings, so `CreatureController::attackTarget` reaches the physical half).
// The plan scheduled them for stage 2 on the reasoning that "none of these is reachable before its owner lands"; that reasoning held for the
// owners and not for the caller. Each stand-in left below now names the milestone that deletes it.

/** Java: FollowStartService.newFollowingToTargetCheckTask(follower, leading) */
runtime::FutureRef followStartServiceNewFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading);

// --------------------------------------------------------------------------------------------------------------- P5-02 (aion_gs_skills)

/**
 * Java: `ChargeSkill skill = SkillEngine.getInstance().getChargeSkill(creature, skillId, skillLevel, chargeLevel, startSkill); skill != null &&
 * skill.useSkill()` (ChargeSkill has no header, so its Ref cannot be released here)
 * <p>
 * **Closed by M5b-2** (m5b-plan.md O-01; m5b2-plan.md P-04's second half, after S-01 ports `SkillEngine::getChargeSkill`). Unreachable
 * still: its only call site, `CreatureController::useChargeSkill`, is reached from `CM_USE_CHARGE_SKILL` alone, which is not registered (m5b2-plan.md
 * P-03, W), and needs a charge skill that is casting, which needs `Skill::useSkill` (S-02).
 */
bool chargeSkillGetAndUse(model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel, int32_t chargeLevel,
	skillengine::model::Skill& startSkill);

// m5b-2 stage 0 (header-request m5b2-1): `attackUtilCalculateMagAttackResult` is gone - `AttackUtil::calculateMagAttackResult` and the three
// bodies below it (`calculateMagicalStatus`, `StatFunctions::calculateMagicalResistRate` and `calculateMagicalCriticalRate`) are ported, and
// `CreatureController::attackTarget` calls the real function the way its PHYSICAL sibling already does. That closes
// m5b-client-session.md S-3: a character whose main-hand weapon is magical can auto-attack.

// --------------------------------------------------------------------------------------------------------------- P5-10 (aion_gs_team)

/**
 * Java: PlayerTeamDistributionService.doReward(team, damagePercent, owner, winner, damageList)
 * <p>
 * **Closed by P5-10** (m5b-plan.md E-01b): the whole `services/teleport`-sibling team package is unported. `NpcController::doReward` reaches it
 * only for a `TemporaryPlayerTeam` attacker, which needs a group or an alliance; a solo character never makes one.
 */
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

// m5b-2 stage 1 (m5b2-plan.md P-04, first half): `playerRestrictionsCanUseSkill` is gone - `PlayerRestrictions::canUseSkill` is ported (P-01),
// so `PlayerController::useSkill` calls it the way `attackTarget` calls `canAttack`. `CM_CASTSPELL` exists now (P-02) and is the caller.

/** Java: PvpMapService.getInstance().isOnPvPMap(creature) */
bool pvpMapServiceIsOnPvPMap(model::gameobjects::Creature& creature);

/**
 * Java: `instanceHandler instanceof PvpMapHandler`. Not unported: the check compares the simple class name of the dynamic type (the only Java
 * class of that name), so it works before PvpMapHandler.h exists.
 */
bool isPvpMapHandler(instance::handlers::InstanceHandler& instanceHandler);

} // namespace aion::gameserver::controllers::standins
