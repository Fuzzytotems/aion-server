#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Invincible Shabokan in Tiamat Stronghold ("invincibleshabokan"): from the first attack every 30 s from 5 s an earthquake (20717 and,
 * unless one stands, 283082) or a sinking (20720 and 283083 + 283084 under every visible player within 30 m), at random; at 25 % HP 20941
 * once; back home the task stops and 20941 goes.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/InvincibleShabokanAI.java, @AIName("invincibleshabokan") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class InvincibleShabokanAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(InvincibleShabokanAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> skillTask{};
	runtime::Field<bool> isFinalBuff{};

public:
	explicit InvincibleShabokanAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startSkillTask();

	void cancelTask();

	void earthQuakeEvent();

	void sinkEvent();

	void chooseRandomEvent();

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
