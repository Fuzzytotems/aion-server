#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::controllers {

/**
 * This class is for controlling Creatures [npc's, players etc]
 * <p>
 * Hub header (docs/design/hub-headers.md). Java `CreatureController<T extends Creature>` is one non-template class (erasure rule §8.1):
 * getOwner() is narrowed to `Creature&` here and again by NpcController/PlayerController (§8.2).
 * Java passes `null` for some enum arguments (`onAttack(creature, dmg, null)` in admincommands.Damage and TheHexwayInstance,
 * AttackShieldObserver's reflected hit with a null AttackStatus and HopType, `die(null, null, creature)`); those parameters are
 * `std::optional<Enum>`. The nullable SM_SYSTEM_MESSAGE of cancelCurrentSkill is a borrowed pointer (server packets are stack temporaries, §12).
 *
 * @author -Nemesiss-, ATracer(2009-09-29), Sarynth, Wakizashi
 */
class CreatureController : public VisibleObjectController {
private:
	/** Java: private static final class DelayedOnAttack implements Runnable (used only by attackTarget; defined in the .cpp, §9.3) */
	class DelayedOnAttack;

	runtime::Field<runtime::Ref<observer::TerrainZoneCollisionMaterialActor>> actor{};
	runtime::ConcurrentHashMap<int32_t, runtime::FutureRef> tasks{};

protected:
	/** Java: the implicit constructor of the abstract class. */
	CreatureController();

public:
	~CreatureController() override;

	/** Narrowing accessor (Java: T getOwner() with T extends Creature, hub-headers.md §8.2). */
	model::gameobjects::Creature& getOwner() const;

	void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) override;

	void notKnow(model::gameobjects::VisibleObject& object) override;

	/** Removes owner from the visualObjects lists of all known objects who can't see him anymore. */
	virtual void onHide();

	/** Re-adds owner to the visualObjects lists of all known objects. */
	virtual void onHideEnd();

	/** Perform tasks on Creature starting to move */
	virtual void onStartMove();

	/** Perform tasks on Creature move in progress */
	virtual void onMove();

	/** Perform tasks on Creature stop move */
	virtual void onStopMove();

protected:
	/** Notify everyone in knownlist about move event */
	virtual void notifyAIOnMove();

public:
	/** Zone update mask management */
	void updateZone();

	/** Will be called by ZoneManager when creature enters specific zone */
	virtual void onEnterZone(world::zone::ZoneInstance& zoneInstance) {}

	/** Will be called by ZoneManager when player leaves specific zone */
	virtual void onLeaveZone(world::zone::ZoneInstance& zoneInstance) {}

	/**
	 * Perform tasks on Creature death.<br>
	 * Should ONLY be called from {@link com.aionemu.gameserver.model.stats.container.CreatureLifeStats} to avoid duplicate death events.
	 */
	virtual void onDie(model::gameobjects::Creature& lastAttacker);

	/** Called when the creature gains or loses hate towards the attacker */
	virtual void onAddHate(model::gameobjects::Creature& attacker, bool isNewInAggroList);

	/** Perform tasks when Creature was attacked. attackStatus: Java callers pass null (admincommands.Damage, TheHexwayInstance). */
	void onAttack(model::gameobjects::Creature& creature, int32_t damage, std::optional<attack::AttackStatus> attackStatus);

	/** criticalProcEffect: null unless a stumble procced (attackTarget, DelayedOnAttack) */
	void onAttack(model::gameobjects::Creature& creature, int32_t damage, attack::AttackStatus attackStatus,
		runtime::Ptr<skillengine::model::Effect> criticalProcEffect);

	void onAttack(skillengine::model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, skillengine::model::HopType hopType);

	/**
	 * Perform tasks when Creature was attacked by a periodic effect. Its critical hits are only visible in the attack status packet, since the cast
	 * result was already sent when the effect started.
	 */
	void onAttack(skillengine::model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, skillengine::model::HopType hopType, bool criticalHit);

	/** effect: null for auto attacks. status, hopType: AttackShieldObserver passes null for both. */
	virtual void onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
		network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> status,
		std::optional<skillengine::model::HopType> hopType);

protected:
	/**
	 * Perform tasks when Creature was attacked
	 * <p>
	 * C++: private in Java. Protected so that NpcController and PlayerController can re-expose the public onAttack overloads with
	 * `using CreatureController::onAttack;` (MSVC C2876: a using-declaration must be able to access every overload it names; it then exposes this
	 * overload through the subclass as well). Only CreatureController calls it.
	 *
	 * @param attacker
	 *          creature the damage is credited to, which is not always the effector (reflected and protected damage)
	 * @param effect
	 *          effect which dealt the damage, null for auto attacks
	 * @param notifyAttack
	 *          whether the hit may interrupt casts and notify attack observers
	 * @param criticalProcEffect
	 *          stumble which procced from an earlier critical hit and is applied on top of this damage, null if none procced
	 * @param criticalHit
	 *          whether the attack status packet must mark this damage as a critical hit
	 */
	void onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
		network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> status,
		std::optional<skillengine::model::HopType> hopType, runtime::Ptr<skillengine::model::Effect> criticalProcEffect, bool criticalHit);

private:
	void calculateGodStoneEffects(model::gameobjects::player::Player& attacker);

	/** weapon: null when the hand is empty (Equipment.getMainHandWeapon/getOffHandWeapon) */
	void applyGodStoneEffect(model::gameobjects::player::Player& attacker, runtime::Ptr<model::gameobjects::Item> weapon, bool isMainHandWeapon);

public:
	/** Perform reward operation */
	virtual void doReward() {}

	virtual void onDialogRequest(model::gameobjects::player::Player& player) {}

	virtual void attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks);

	/**
	 * Handle dialog select: getOwner() is the target or dialog sender, the given player is the one who clicked the dialog
	 */
	virtual void onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
		int32_t extendedRewardIndex) {}

	bool hasTask(model::TaskId taskId);

	bool hasScheduledTask(model::TaskId taskId);

	/** @return the removed task (the map no longer holds it), or null */
	runtime::FutureRef getAndRemoveTask(model::TaskId taskId);

	/** @return the cancelled task, or null */
	runtime::FutureRef cancelTask(model::TaskId taskId);

	bool cancelTaskIfPresent(model::TaskId taskId, runtime::FutureRef task);

	/**
	 * If task already exist - it will be canceled
	 */
	void addTask(model::TaskId taskId, runtime::FutureRef task);

	/**
	 * Cancel all tasks associated with this controller
	 */
	void cancelAllTasks();

	void onDelete() override;

	/**
	 * Die by reducing HP to 0
	 */
	bool die();

	bool die(model::gameobjects::Creature& lastAttacker);

	/** type, log: die() and die(Creature) pass null */
	bool die(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, model::gameobjects::Creature& lastAttacker);

	/**
	 * Use skill with default level 1
	 */
	bool useSkill(int32_t skillId);

	/**
	 * @return true if successful usage
	 */
	virtual bool useSkill(int32_t skillId, int32_t skillLevel);

	bool useChargeSkill(skillengine::model::Skill& startSkill, int64_t chargeTimeMillis);

	runtime::Ptr<skillengine::model::Skill> abortCast();

	/** lastAttacker: null when the cast is cancelled without an attacker (movement, item use, packets) */
	virtual void cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker);

	/**
	 * Cancel current skill and remove cooldown
	 * <p>
	 * msg: nullable (Java passes null, e.g. Skill.java and cancelCurrentSkill(Creature)); a borrowed pointer to the caller's packet.
	 */
	virtual void cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker, network::aion::serverpackets::SM_SYSTEM_MESSAGE* msg);

	/**
	 * Cancel use Item
	 */
	virtual void cancelUseItem() {}

	void onAfterSpawn() override;

	void onDespawn() override;
};

} // namespace aion::gameserver::controllers
