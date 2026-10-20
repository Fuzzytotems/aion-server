#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

/**
 * Crucible Challenge's rifts ("cruciblerift"): 730459 announces itself and leads to the hidden boss room, 730460 brings the player
 * back to stage 6's record keeper.
 * <p>
 * Java: data/handlers/ai/instance/crucibleChallenge/CrucibleRiftAI.java, @AIName("cruciblerift") (the marker is in the .cpp).
 *
 * @author xTz
 */
class CrucibleRiftAI : public ActionItemNpcAI {
public:
	explicit CrucibleRiftAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

	void handleSpawned() override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void announceRift();
};

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
