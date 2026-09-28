#pragma once

#include <any>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/AI.h"
#include "aion/gameserver/ai/event/fwd.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/model/animations/AttackTypeAnimation.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/npcshout/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::handlers {
struct AIHandlerEntry;
} // namespace aion::gameserver::handlers

namespace aion::gameserver::ai {

/**
 * Base of all AIs: event dispatch, state and substate, owner accessors and spawn helpers.
 * <p>
 * Hub header (docs/design/hub-headers.md). Java's generic AbstractAI&lt;T extends Creature&gt; is erased to one non-template class (§8.1): the
 * owner is a Creature; AITemplate&lt;T&gt; narrows getOwner() and exposes `using OwnerType = T;` for HandlerRegistry.h. An AI is a part of
 * its Creature (handlers-and-porting-plan.md amendment §2): runtime::OwnedPart bound at construction, held in `PartSlot<AbstractAI> ai`
 * (RetireTo::OWNER), created by the AI registry with `std::make_unique<C>(owner)` (HandlerRegistry.h AIFactory), hence the public destructor.
 * <p>
 * C++ differences:
 * - getName() reads the AI registry entry that AIEngine::newAI stores with setRegistryEntry() instead of the class's @AIName annotation.
 * - handleCustomEvent receives the `Object... args` of onCustomEvent as `std::span<const std::any>` (Java passes the array on, §7.4).
 * - Empty Java bodies and `return <literal or parameter>;` bodies are ported inline; every other body is AION_UNPORTED.
 *
 * @author ATracer
 */
class AbstractAI : public runtime::OwnedPart, public AI {
private:
	/** Java: ThreadLocal.withInitial(() -> 0) */
	static inline thread_local std::optional<int32_t> DEPTH{0};

	runtime::OwnerRef<model::gameobjects::Creature> owner;
	runtime::Field<AIState> currentState{};
	runtime::Field<AISubState> currentSubState{};
	runtime::Field<bool> thinking{};

	runtime::Field<bool> logging{false};

	runtime::Field<runtime::Ref<event::AIEventLog>> eventLog{};

	/** C++ only (replaces the @AIName annotation read by getName()): the registry entry AIEngine::newAI created this AI from, or null. */
	runtime::Field<const handlers::AIHandlerEntry*> registryEntry{};

protected:
	explicit AbstractAI(model::gameobjects::Creature& owner);

public:
	/** Owned by the Creature's PartSlot<AbstractAI> (or a std::unique_ptr from the AI factory); destroyed with its owner. */
	~AbstractAI() override;

	runtime::Ptr<event::AIEventLog> getEventLog() const { return eventLog.get(); }

	AIState getState() override { return currentState.get(); }

	bool isInState(AIState state) const { return currentState.get() == state; }

	AISubState getSubState() override { return currentSubState.get(); }

	bool isInSubState(AISubState subState) const { return currentSubState.get() == subState; }

	std::string getName() override;

	/** C++ only: stores the registry entry this AI was created from (AIEngine::newAI, handlers-and-porting-plan.md §1.6); used by getName(). */
	void setRegistryEntry(const handlers::AIHandlerEntry* entry) { registryEntry.set(entry); }

	/** C++ only: the registry entry this AI was created from, nullptr for AIs created outside the registry (DummyAI). */
	const handlers::AIHandlerEntry* getRegistryEntry() const { return registryEntry.get(); }

protected:
	virtual bool canHandleEvent(event::AIEventType eventType);

public:
	bool setStateIfNot(AIState newState); // synchronized

	bool setSubStateIfNot(AISubState newSubState); // synchronized

	void onGeneralEvent(event::AIEventType event) override final;

	void onCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature) override final;

	void onCustomEvent(int32_t eventId, std::initializer_list<std::any> args = {}) override final;

	/**
	 * Will be hidden for all AI's below NpcAI (AITemplate&lt;T&gt; narrows it to T&)
	 *
	 * @return
	 */
	model::gameobjects::Creature& getOwner() const { return owner; }

	int32_t getObjectId();

	runtime::Ptr<world::WorldPosition> getPosition();

	runtime::Ptr<model::gameobjects::VisibleObject> getTarget();

	bool isDead();

	bool setThinking(); // synchronized

	void unsetThinking(); // synchronized

	bool isLogging() override final { return logging.get(); }

	void setLogging(bool value) { logging.set(value); }

protected:
	virtual void handleActivate() = 0;

	virtual void handleDeactivate() = 0;

	virtual void handleBeforeSpawned() = 0;

	virtual void handleSpawned() = 0;

	virtual void handleDespawned() = 0;

	virtual void handleDied() = 0;

	virtual void handleMoveValidate() = 0;

	virtual void handleMoveArrived() = 0;

	virtual void handleAttackComplete() = 0;

	virtual void handleFinishAttack() = 0;

	virtual void handleTargetTooFar() = 0;

	virtual void handleTargetGiveup() = 0;

	virtual void handleNotAtHome() = 0;

	virtual void handleBackHome() = 0;

	virtual void handleDropRegistered() = 0;

	virtual void handleAttack(runtime::Ptr<model::gameobjects::Creature> creature) = 0;

	virtual void creatureNeedsHelp(model::gameobjects::Creature& creature) = 0;

	virtual bool handleCreatureNeedsSupport(model::gameobjects::Creature& creature) = 0;

	virtual bool handleCreatureNeedsSupportByGuard(model::gameobjects::Creature& creature) = 0;

	virtual void handleCreatureSee(model::gameobjects::Creature& creature) = 0;

	virtual void handleCreatureNotSee(model::gameobjects::Creature& creature) = 0;

	virtual void handleCreatureMoved(model::gameobjects::Creature& creature) = 0;

	virtual void handleCreatureAggro(model::gameobjects::Creature& creature) = 0;

	virtual void handleTargetChanged(model::gameobjects::Creature& creature) = 0;

	virtual void handleFollowMe(model::gameobjects::Creature& creature) = 0;

	virtual void handleStopFollowMe(model::gameobjects::Creature& creature) = 0;

	virtual void handleDialogStart(model::gameobjects::player::Player& player) = 0;

	virtual void handleDialogFinish(model::gameobjects::player::Player& player) = 0;

	/** Java: handleCustomEvent(int eventId, Object... args), called with onCustomEvent's argument array */
	virtual void handleCustomEvent(int32_t eventId, std::span<const std::any> args) = 0;

public:
	virtual bool onPatternShout(model::templates::npcshout::ShoutEventType event, std::string_view pattern, int32_t skillNumber) = 0;

protected:
	void handleGeneralEvent(event::AIEventType event);

	void logEvent(event::AIEventType event);

private:
	void handleCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature);

public:
	virtual AttackIntention chooseAttackIntention() = 0;

	bool onDialogSelect(model::gameobjects::player::Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override {
		return false;
	}

protected:
	/**
	 * Spawn object in the same world and instance as AI's owner
	 */
	runtime::Ptr<model::gameobjects::VisibleObject> spawn(int32_t npcId, float x, float y, float z, int8_t heading);

	/**
	 * Spawn object with staticId in the same world and instance as AI's owner
	 */
	runtime::Ptr<model::gameobjects::VisibleObject> spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId);

	/** aiName: std::nullopt (Java null) spawns with the template's AI */
	runtime::Ptr<model::gameobjects::VisibleObject> spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId,
		std::optional<std::string_view> aiName);

	runtime::Ptr<model::gameobjects::VisibleObject> rndSpawnInRange(int32_t npcId, float distance);

	runtime::Ptr<model::gameobjects::VisibleObject> rndSpawnInRange(int32_t npcId, float minDistance, float maxDistance);

public:
	float modifyDamage(model::gameobjects::Creature& attacker, float damage, runtime::Ptr<skillengine::model::Effect> effect) override {
		return damage;
	}

	float modifyOwnerDamage(float damage, model::gameobjects::Creature& effected, runtime::Ptr<skillengine::model::Effect> effect) override {
		return damage;
	}

	void modifyOwnerStat(model::stats::calc::Stat2& stat) override {}

	model::templates::item::ItemAttackType modifyAttackType(model::templates::item::ItemAttackType type) override { return type; }

	int32_t modifyAggroRange(int32_t value) override { return value; }

	int32_t modifyAggroAngle(int32_t value) override { return value; }

	model::animations::AttackHandAnimation modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) override {
		return attackHandAnimation;
	}

	model::animations::AttackTypeAnimation getAttackTypeAnimation(model::gameobjects::Creature& target) override {
		return model::animations::AttackTypeAnimation::MELEE;
	}

	int32_t modifyInitialSkillDelay(int32_t delay) override { return delay; }
};

} // namespace aion::gameserver::ai
