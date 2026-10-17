#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of the Vault's guide in The Shugo Emperor's Vault ("idsweep_in_npc"): a simple talk with dialog 1011, whose teleport choice sends the
 * player to (423.7, 700.4, 399) of his instance.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/IDSweep_In_Npc.java, @AIName("idsweep_in_npc") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class IDSweep_In_Npc : public GeneralNpcAI {
public:
	explicit IDSweep_In_Npc(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
