#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Kahrun at the refuge's start ("IDTiamat_2_Kahrun_Talk"): while no orb (730625) is out his dialog opens 1011; the first
 * choice (once) makes him peaceful, he walks to the orb's place 2 s later, the orb (static 4) comes 1.5 s after and he goes.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_KahrunTalkAI.java, @AIName("IDTiamat_2_Kahrun_Talk") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class IDTiamat_2_KahrunTalkAI : public NpcAI {
private:
	runtime::AtomicBoolean isActivated{AION_LOCK_CLASS(IDTiamat_2_KahrunTalkAI::isActivated)}; // Java: = new AtomicBoolean()

public:
	explicit IDTiamat_2_KahrunTalkAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void spawnOrb();
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
