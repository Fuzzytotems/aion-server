#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::controllers {

/**
 * This class is for controlling players.
 * <p>
 * Hub header (docs/design/hub-headers.md). Binds CreatureController's type variable to Player: getOwner() returns `Player&` (§8.2). The
 * onAttack and useSkill overloads of CreatureController stay callable through `PlayerController&` by using-declarations (C++ name hiding).
 *
 * @author -Nemesiss-, ATracer, xavier, Sarynth, RotO, xTz, KID, Sippolo
 */
class PlayerController : public CreatureController {
private:
	runtime::Field<int64_t> lastAttackMillis{0};
	runtime::Field<int64_t> lastAttackedMillis{0};
	runtime::Field<runtime::Ref<observer::StanceObserver>> stanceObserver{};

public:
	PlayerController();
	~PlayerController() override;

	/** Narrowing accessor (Java: CreatureController<Player>.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::player::Player& getOwner() const;

	using CreatureController::onAttack;
	using CreatureController::useSkill;

	void see(model::gameobjects::VisibleObject& object) override;

private:
	void sendPlayerInfoPackets(model::gameobjects::player::Player& player);

public:
	void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) override;

	void onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
		runtime::Ptr<model::gameobjects::VisibleObject> newTarget) override;

	void onHide() override;

	void onHideEnd() override;

	void updateNearbyQuests();

	void updateRepeatableQuests();

	void onEnterZone(world::zone::ZoneInstance& zone) override;

	void onLeaveZone(world::zone::ZoneInstance& zone) override;

	/**
	 * Called when leaving a fly zone (like citadel of verteron) or a fly map (like the abyss).
	 */
	void onLeaveFlyArea();

	void onEnterFlyArea();

	/**
	 * Should only be triggered from one place (life stats)
	 */
	void onEnterWorld();

	void onDie(model::gameobjects::Creature& lastAttacker) override;

private:
	void setRebirthReviveInfo();

public:
	void onDespawn() override;

	void scheduleShowResurrectionOptions();

	void showResurrectionOptions();

private:
	bool isInvader(model::gameobjects::player::Player& player);

public:
	void doReward() override;

	void onBeforeSpawn() override;

	void attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) override;

	void onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
		network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> attackStatus,
		std::optional<skillengine::model::HopType> hopType) override;

	void useSkill(const skillengine::model::SkillTemplate* template_, int32_t targetType, float x, float y, float z, int32_t clientHitTime,
		int32_t skillLevel);

	void onStartMove() override;

	void onMove() override;

	void onStopMove() override;

protected:
	void notifyAIOnMove() override;

public:
	void cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker) override;

	/** message: nullable, see CreatureController::cancelCurrentSkill */
	void cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE* message) override;

	void cancelUseItem() override;

	void onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
		int32_t extendedRewardIndex) override;

	void onLevelChange(int32_t oldLevel, int32_t newLevel);

	void upgradePlayer();

	void onChangedPlayerAttributes();

	/**
	 * After entering game player char is "blinking" which means that it's in under some protection, after making an action char stops blinking. -
	 * Starts protection active - Schedules task to end protection
	 */
	void startProtectionActiveTask();

	/**
	 * Stops protection active task after first move or use skill
	 */
	void stopProtectionActiveTask();

	/**
	 * When player arrives at destination point of flying teleport
	 */
	void onFlyTeleportEnd();

	void startStance(int32_t skillId);

	void stopStance();

	int32_t getStanceSkillId();

	bool isUnderStance();

	void updateSoulSickness(int32_t skillId);

	/**
	 * Player is considered in combat if he's been attacked or has attacked less or equal 10s before
	 *
	 * @return true if the player is actively in combat
	 */
	bool isInCombat();

	/**
	 * @return The last time, when the player attacked someone or got attacked
	 */
	int64_t getLastCombatTime();

	/**
	 * Refreshes the combat timer (see {@link #isInCombat()}) and cancels a pending summon request, which combat invalidates.
	 *
	 * @param attacking
	 *          True, if the player attacked someone, false if he was attacked
	 */
	void enterCombat(bool attacking);

	/**
	 * C++ only (LogoutBreakers L4, cycles.toml PlayerController.stanceObserver, zombie-safe): sets stanceObserver to null without removing the
	 * stance effect, removing the observer or sending packets. Idempotent.
	 */
	void breakStanceObserver();
};

} // namespace aion::gameserver::controllers
