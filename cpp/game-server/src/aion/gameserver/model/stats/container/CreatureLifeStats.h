#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * HP and MP of a creature, their changes and the restore task.
 * <p>
 * Hub header (docs/design/hub-headers.md). Java `CreatureLifeStats<T extends Creature>` is one non-template class (§8.1 erasure rule):
 * `owner` and getOwner() are `Creature&`; subclasses (NpcLifeStats, PlayerLifeStats, SummonLifeStats) redeclare a narrowing getOwner(). A part
 * of Creature (OwnedPart, fieldmap `Creature.lifeStats`) with a non-retaining `OwnerRef`, bound in the constructor.
 * - SM_ATTACK_STATUS.TYPE and .LOG are the generated enums `SM_ATTACK_STATUS_TYPE`/`SM_ATTACK_STATUS_LOG`. Java passes null for both
 *   ("if null, no packet") through increaseMp(int) and through CreatureController.die()/die(Creature) (`die(null, null, ...)` forwards to
 *   reduceHp), so reduceHp, increaseMp(type, ...) and sendAttackStatusPacketUpdate take `std::optional`; reduceMp and increaseHp take the enums
 *   by value (no caller passes null) and convert implicitly when they forward.
 * - `restoreLock` is the Monitor that replaces `new Object()`; the `synchronized (this)` blocks are SYNCHRONIZED(*this) on the part's own Monitor.
 *
 * @author ATracer
 */
class CreatureLifeStats : public runtime::OwnedPart {
private:
	runtime::Field<int32_t> currentHp{};
	runtime::Field<int32_t> currentMp{};
	/** for long animation skills that will kill - last damage */
	runtime::Field<int32_t> killingBlow{};

protected:
	runtime::OwnerRef<gameobjects::Creature> owner;
	runtime::Monitor restoreLock{AION_LOCK_CLASS(CreatureLifeStats::restoreLock)};
	runtime::Field<runtime::FutureRef> lifeRestoreTask{};

	/** Java abstract class: only the subclasses (NpcLifeStats, PlayerLifeStats, SummonLifeStats, ...) are constructed */
	CreatureLifeStats(gameobjects::Creature& owner, int32_t currentHp, int32_t currentMp);

public:
	~CreatureLifeStats() override;

	/** Subclasses narrow it (NpcLifeStats: Npc&). */
	gameobjects::Creature& getOwner() const { return owner; }

	int32_t getCurrentHp() const { return currentHp.get(); }

	int32_t getCurrentMp() const { return currentMp.get(); }

	int32_t getMaxHp();

	int32_t getMaxMp();

	bool isDead();

	bool isAboutToDie();

	void setKillingBlow(int32_t value) { killingBlow.set(value); }

private:
	void unsetIsAboutToDie();

public:
	/**
	 * This method is called whenever caller wants to absorb creatures' HP
	 *
	 * @param type attack type (see SM_ATTACK_STATUS.TYPE), if nullopt (Java null, CreatureController::die()), no SM_ATTACK_STATUS packet will be sent
	 * @param value hp to subtract
	 * @param skillId skillId (0 if none)
	 * @param log log type (see SM_ATTACK_STATUS.LOG) for the attack status packet to be sent, nullopt together with type
	 * @param attacker attacking creature or self (Java Objects.requireNonNull)
	 * @return The HP that this creature has left. If 0, the creature died.
	 */
	int32_t reduceHp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, gameobjects::Creature& attacker);

	int32_t reduceHp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, gameobjects::Creature& attacker, bool criticalHit);

	/**
	 * This method is called whenever caller wants to absorb creatures's MP
	 *
	 * @return The MP that this creature has left.
	 */
	int32_t reduceMp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, int32_t skillId,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log);

protected:
	/** @param type if nullopt (Java null), no SM_ATTACK_STATUS packet will be sent */
	void sendAttackStatusPacketUpdate(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log);

	/** @param type if nullopt (Java null), no SM_ATTACK_STATUS packet will be sent */
	void sendAttackStatusPacketUpdate(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, bool criticalHit);

public:
	/**
	 * This method is called whenever caller wants to restore creatures's HP
	 *
	 * @return currentHp
	 */
	int32_t increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value);

	int32_t increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, gameobjects::Creature& effector);

	int32_t increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, skillengine::model::Effect& effect,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log);

private:
	/** @param effector nullable (the body falls back to the owner) */
	int32_t increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, runtime::Ptr<gameobjects::Creature> effector, int32_t skillId,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log);

public:
	/**
	 * This method is called whenever caller wants to restore creatures's MP
	 *
	 * @return currentMp
	 */
	int32_t increaseMp(int32_t value);

	/** @param type, log nullopt when called from increaseMp(int) (Java null) */
	int32_t increaseMp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log);

	/** Restores HP with value set as HP_RESTORE_TICK */
	void restoreHp();

	/** Restores HP with value set as MP_RESTORE_TICK */
	void restoreMp();

	/** Will trigger restore task if not already */
	virtual void triggerRestoreTask();

	/** Cancel currently running restore task */
	void cancelRestoreTask();

	bool isFullyRestoredHpMp();

	bool isFullyRestoredHp();

	bool isFullyRestoredMp();

	/**
	 * The purpose of this method is synchronize current HP and MP with updated MAXHP and MAXMP stats This method should be called only on creature load
	 * to game or player level up
	 */
	virtual void synchronizeWithMaxStats();

	/**
	 * The purpose of this method is synchronize current HP and MP with MAXHP and MAXMP when max stats were decreased below current level
	 */
	virtual void updateCurrentStats();

	/** @return HP percentage 0 - 100 (minimum 1% while alive) */
	int32_t getHpPercentage();

	/** @return MP percentage 0 - 100 */
	int32_t getMpPercentage();

protected:
	/** @param effector Ptr as skeleton.py drafts the method family (hub-headers.md §5.1); the Java callers never pass null */
	virtual void onHpChanged(int32_t previousHp, int32_t newHp, runtime::Ptr<gameobjects::Creature> effector);

	virtual void onMpChanged(int32_t previousMp, int32_t newMp);

public:
	virtual int32_t getMaxFp() { return 0; }

	virtual int32_t getCurrentFp() { return 0; }

	/** Cancel all tasks when player logout */
	virtual void cancelAllTasks();

	/** This method can be used to fully restore owners HP and remove dead state of lifestats */
	void setCurrentHpPercent(int32_t hpPercent);

	/** Sets the current HP without notifying observers */
	void setCurrentHp(int32_t hp);

	void setCurrentHp(int32_t hp, gameobjects::Creature& effector);

	void setCurrentMp(int32_t value);

	/** This method can be used to fully restore owners MP */
	void setCurrentMpPercent(int32_t mpPercent);
};

} // namespace aion::gameserver::model::stats::container
