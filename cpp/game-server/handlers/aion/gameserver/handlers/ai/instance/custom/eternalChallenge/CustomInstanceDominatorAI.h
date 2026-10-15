#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/**
 * The dominator protector of the Eternal Challenge ("custom_instance_dominator"): in the Roah handler's instance it debuffs its target with
 * the challenger's rank's skills every 30 ... 20 seconds and jumps to a target it cannot see (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/instance/custom/eternalChallenge/CustomInstanceDominatorAI.java.
 */
class CustomInstanceDominatorAI : public AggressiveNoLootNpcAI {
private:
	runtime::Field<int32_t> challengerObjId{};
	runtime::Field<int32_t> challengerRank{};
	runtime::Field<runtime::FutureRef> debuffTask{};

public:
	explicit CustomInstanceDominatorAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

	AttackIntention chooseAttackIntention() override;

protected:
	void handleSpawned() override;
	void handleDied() override;
	void handleDespawned() override;
	void handleBackHome() override;

private:
	void debuffTarget();
	bool isPhysicalTarget();
	void cancelTasks();
};

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
