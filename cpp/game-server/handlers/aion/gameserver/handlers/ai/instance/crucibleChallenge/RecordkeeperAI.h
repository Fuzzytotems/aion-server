#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

/**
 * Crucible Challenge's record keepers ("recordkeeper"): each starts its stage through the instance handler or moves the player to the
 * next one; 205679 gives the score.
 * <p>
 * Java: data/handlers/ai/instance/crucibleChallenge/RecordkeeperAI.java, @AIName("recordkeeper") (the marker is in the .cpp).
 *
 * @author xTz
 */
class RecordkeeperAI : public NpcAI {
public:
	explicit RecordkeeperAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
