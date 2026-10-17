#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

/**
 * Crucible Challenge's arbiters ("arbiter"): for one Crucible Insignia of Courage (186000134) each arbiter takes the player back to its
 * stage.
 * <p>
 * Java: data/handlers/ai/instance/crucibleChallenge/ArbiterAI.java, @AIName("arbiter") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ArbiterAI : public NpcAI {
public:
	explicit ArbiterAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
