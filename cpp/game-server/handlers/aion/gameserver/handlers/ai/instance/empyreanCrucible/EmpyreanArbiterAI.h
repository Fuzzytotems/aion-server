#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * Empyrean Crucible's arbiters ("empyreanarbiter"): for a worthiness ticket (186000124) a defeated player goes back to the fight of the
 * arbiter's stage.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/EmpyreanArbiterAI.java, @AIName("empyreanarbiter") (the marker is in the .cpp).
 *
 * @author xTz
 */
class EmpyreanArbiterAI : public NpcAI {
public:
	explicit EmpyreanArbiterAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
