#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include <vector>

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of the nightmare crates ("nightmare_crate"): a crate takes one key (a greater crate three), of either kind, and opens like a chest
 * for the player and his team members in range; without keys the treasure box message.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/NightmareCrateAI.java, @AIName("nightmare_crate") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class NightmareCrateAI : public ActionItemNpcAI {
public:
	explicit NightmareCrateAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

private:
	void analyzeOpening(Player& player);

	int32_t getHighestLevel(const std::vector<runtime::Ptr<Player>>& players);
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
