#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's mountable weapons ("eternal_bastion_mountable"): the first choice takes the key (185000136 for the cannons,
 * 185000137 for the tank), puts the player on the weapon and applies its transformation.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionMountableAI.java, @AIName("eternal_bastion_mountable") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class EternalBastionMountableAI : public ActionItemNpcAI {
public:
	explicit EternalBastionMountableAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void useNpc(Player& player);

	void tryMountNpc(Player& player, int32_t keyItemId, int32_t skillId);
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
