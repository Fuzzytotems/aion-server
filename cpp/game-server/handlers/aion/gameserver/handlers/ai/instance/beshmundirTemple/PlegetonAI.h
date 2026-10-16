#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the Plegeton boatmen ("plegeton"): the first choice plays the npc's movie and ports the player in the temple; 799517 also
 * starts the 7 minute timer once, after which a living Temadaro (216586) goes and his door opens.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/PlegetonAI.java, @AIName("plegeton") (the marker is in the .cpp).
 *
 * @author unknown
 */
class PlegetonAI : public NpcAI {
private:
	runtime::Field<bool> isStartTimer{false};

public:
	explicit PlegetonAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void sendTimer();

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
