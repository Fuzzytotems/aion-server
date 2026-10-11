#pragma once

#include <array>
#include <string>

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .symphony: exchanges the Legendary Symphony event item for prizes.
 * <p>
 * C++: Java's `private static final int[][] REWARDS` is rewards(), a function-local constexpr array (lint L2: no array member).
 *
 * @author Pad
 */
class Symphony : public PlayerCommand {
public:
	Symphony();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	static constexpr int32_t REQUIRED_ITEM_ID = 182007170;

	/** Java: private static final int[][] REWARDS (rows {COLLECTION_COUNT, REWARD_ID, REWARD_COUNT}) */
	static const std::array<std::array<int32_t, 3>, 15>& rewards();

	static std::string buildSyntaxInfo();
};

} // namespace aion::gameserver::handlers::playercommands
