#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a house's butler ("butler"): a dialog action opens its page, and a player the butler sees gets the house's scripts.
 * <p>
 * Java: data/handlers/ai/ButlerAI.java, @AIName("butler") (the marker is in the .cpp).
 *
 * @author Rolandas, Neon, Sykra
 */
class ButlerAI : public GeneralNpcAI {
public:
	explicit ButlerAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	bool kickDialog(Player& player, model::DialogPage page);

protected:
	void handleCreatureSee(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai
