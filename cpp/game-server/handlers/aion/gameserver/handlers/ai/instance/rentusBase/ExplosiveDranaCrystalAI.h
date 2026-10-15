#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the explosive Drana crystals in Rentus Base ("explosive_drana_crystal"): used once, it takes one of the boss's three shields,
 * explodes (two helpers cast) and goes; unused it goes after a minute.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/ExplosiveDranaCrystalAI.java, @AIName("explosive_drana_crystal") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ExplosiveDranaCrystalAI : public ActionItemNpcAI {
private:
	runtime::AtomicBoolean isUsed{AION_LOCK_CLASS(ExplosiveDranaCrystalAI::isUsed), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::FutureRef> lifeTask{};

public:
	explicit ExplosiveDranaCrystalAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleUseItemFinish(Player& player) override;

private:
	void cancelLifeTask();

	void startLifeTask();

protected:
	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
