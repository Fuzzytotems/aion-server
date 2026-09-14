#pragma once

#include <any>
#include <cstdint>
#include <span>
#include <string_view>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/AttackIntention.h"
#include "aion/gameserver/ai/poll/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/npcshout/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::ai {

/**
 * AI base with empty default implementations of every event hook.
 * <p>
 * Hub header (docs/design/hub-headers.md §8.1): stays a class template over the owner type (Java AITemplate&lt;T extends Creature&gt;), so the
 * AI registry knows each handler's owner type: `using OwnerType = T;` (HandlerRegistry.h AIHandlerClass: a registered AI is constructible from
 * `OwnerType&`, and AIEngine creates it only for owners of that dynamic type). getOwner() narrows AbstractAI::getOwner() to T& (§8.2); the
 * cast needs T complete where it is called. All Java bodies are empty or return a literal and are ported inline.
 *
 * @author ATracer
 */
template <class T>
class AITemplate : public AbstractAI {
public:
	/** C++ only: the owner type of this AI (HandlerRegistry.h AIHandlerClass, AIEngine owner check) */
	using OwnerType = T;

protected:
	explicit AITemplate(T& owner) : AbstractAI(owner) {}

public:
	/** C++ only: Java's AbstractAI&lt;T&gt;.getOwner() returning T (narrowing accessor, hub-headers.md §8.2) */
	T& getOwner() const { return static_cast<T&>(AbstractAI::getOwner()); }

	void think() override {}

	bool canThink() override { return true; }

	bool ask(poll::AIQuestion question) override { return false; }

protected:
	void handleActivate() override {}

	void handleDeactivate() override {}

	void handleMoveValidate() override {}

	void handleMoveArrived() override {}

	void handleAttack(runtime::Ptr<model::gameobjects::Creature> creature) override {}

	bool handleCreatureNeedsSupport(model::gameobjects::Creature& creature) override { return false; }

	bool handleCreatureNeedsSupportByGuard(model::gameobjects::Creature& creature) override { return false; }

	void handleCreatureSee(model::gameobjects::Creature& creature) override {}

	void handleCreatureNotSee(model::gameobjects::Creature& creature) override {}

	void handleCreatureMoved(model::gameobjects::Creature& creature) override {}

	void handleCreatureAggro(model::gameobjects::Creature& creature) override {}

	void handleFollowMe(model::gameobjects::Creature& creature) override {}

	void handleStopFollowMe(model::gameobjects::Creature& creature) override {}

	void handleDialogStart(model::gameobjects::player::Player& player) override {}

	void handleDialogFinish(model::gameobjects::player::Player& player) override {}

	void handleCustomEvent(int32_t eventId, std::span<const std::any> args) override {}

	void handleBeforeSpawned() override {}

	void handleSpawned() override {}

	void handleDespawned() override {}

	void handleDied() override {}

	void handleAttackComplete() override {}

	void handleFinishAttack() override {}

	void handleTargetTooFar() override {}

	void handleTargetGiveup() override {}

	void handleTargetChanged(model::gameobjects::Creature& creature) override {}

	void handleNotAtHome() override {}

	void handleBackHome() override {}

	void handleDropRegistered() override {}

public:
	bool onPatternShout(model::templates::npcshout::ShoutEventType event, std::string_view pattern, int32_t skillNumber) override { return false; }

protected:
	void creatureNeedsHelp(model::gameobjects::Creature& creature) override {}

public:
	AttackIntention chooseAttackIntention() override { return AttackIntention::SIMPLE_ATTACK; }

	void onStartUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override {}

	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override {}

	void onEffectApplied(skillengine::model::Effect& effect) override {}

	void onEffectEnd(runtime::Ptr<skillengine::model::Effect> effect) override {}

	bool isDestinationReached() override { return false; }
};

} // namespace aion::gameserver::ai
