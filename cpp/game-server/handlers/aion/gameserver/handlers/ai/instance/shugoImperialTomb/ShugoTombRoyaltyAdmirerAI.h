#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's admirers ("shugo_tomb_royalty_admirer"): dialog 1011. The three admirers' first choice starts their
 * stage (the instance handler's stage list) and deletes them; the delighted and disappointed admirers' first choice sends the player on to the
 * next stage, their second gives the next stage's transformation (dialog 1012).
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombRoyaltyAdmirerAI.java, @AIName("shugo_tomb_royalty_admirer") (the marker is in the
 * .cpp).
 *
 * @author Ritsu
 */
class ShugoTombRoyaltyAdmirerAI : public NpcAI {
public:
	explicit ShugoTombRoyaltyAdmirerAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void changeStageList(Player& player, StageList stageList);
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
