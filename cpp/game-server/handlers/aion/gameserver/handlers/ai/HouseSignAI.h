#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a house's sign ("housesign"): a dialog action opens its page.
 * <p>
 * Java: data/handlers/ai/HouseSignAI.java, @AIName("housesign") (the marker is in the .cpp).
 *
 * @author Rolandas
 */
class HouseSignAI : public GeneralNpcAI {
public:
	explicit HouseSignAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai
