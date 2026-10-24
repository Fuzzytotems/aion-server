#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the birthday cake ("birthday_cake"): the first choice casts its two buffs on the player.
 * <p>
 * Java: data/handlers/ai/events/BirthdayCakeAI.java, @AIName("birthday_cake") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class BirthdayCakeAI : public GeneralNpcAI {
public:
	explicit BirthdayCakeAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::events
