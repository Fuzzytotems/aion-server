#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

/**
 * Poppy of Crucible Challenge's bonus stage 2 ("poppyontherun"): never thinks, walks her route and closes every dialog.
 * <p>
 * Java: data/handlers/ai/instance/crucibleChallenge/PoppyOnTheRunAI.java, @AIName("poppyontherun") (the marker is in the .cpp).
 *
 * @author xTz
 */
class PoppyOnTheRunAI : public GeneralNpcAI {
public:
	explicit PoppyOnTheRunAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
