#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of Yume in the Nightmare Circus ("yume"): she does not think nor talk; 21463/21465 on her make her cast 21467; the first player
 * moving near her starts her heal round - every 15 s, while that player is hurt, 21466 (half the times announced).
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/YumeAI.java, @AIName("yume") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class YumeAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isStart{AION_LOCK_CLASS(YumeAI::isStart), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit YumeAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleDialogStart(Player& player) override;

public:
	void onEffectApplied(Effect& effect) override;

protected:
	void handleCreatureMoved(Creature& creature) override;

private:
	void cancelTask();

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
