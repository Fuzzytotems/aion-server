#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .buy: exchanges the custom instance's reward coins for the listed rewards (D12, owner 2026-10-13).
 * <p>
 * C++ (m5j-plan §18.6, .buy's fieldmap answers): Java's static rewards map (item id -> {"cost", "amount"}, filled by the constructor through
 * addReward) is an immortal list in insertion order, built once like Easter's rewards; its {"cost", "amount"} maps are a Reward record. The
 * anonymous RequestResponseHandler runs a lambda that keeps the immortal command (this) and the player by Ref.
 *
 * @author Estrayl
 */
class Buy : public PlayerCommand {
public:
	Buy();

	void execute(Player& player, std::span<const std::string> params) override;

	/** Java: the {"cost", "amount"} map of an item id */
	struct Reward {
		int32_t itemId;
		int32_t cost;
		int32_t amount;
	};

	/** Java: rewards.get(itemId), or null */
	static const Reward* findReward(int32_t itemId);

private:
	/** Java: private static final Map<Integer, Map<String, Integer>> rewards (see the class comment) */
	static const std::vector<Reward>& rewards();

	void showRewards(Player& player);

	void buyItem(Player& player, const std::string& itemLink);
};

} // namespace aion::gameserver::handlers::playercommands
