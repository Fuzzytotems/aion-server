#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Brigade General Chantra in Tiamat Stronghold ("brigadegeneralchantra"): from her first attack, every 40 s (from 5 s) one of her two
 * rings (283092, 283094) appears unless it stands, and 5 s later gives way to its trap (283171, 283172); at 25 % HP she casts 20942 once
 * (again after a return home).
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralChantraAI.java, @AIName("brigadegeneralchantra") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class BrigadeGeneralChantraAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(BrigadeGeneralChantraAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> trapTask{};
	runtime::Field<bool> isFinalBuff{};

public:
	explicit BrigadeGeneralChantraAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startSkillTask();

	void cancelTask();

	void startTrapEvent();

protected:
	void handleDied() override;

	void handleDespawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
