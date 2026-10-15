#pragma once

#include <string>
#include <vector>

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .easter: exchanges easter eggs for prizes.
 * <p>
 * C++: Java's `private static final List<Reward> rewards` and `randomRewards` (class initialization) are rewards() and randomRewards(),
 * function-local statics (Megaphone.h's form); the record Reward is a nested struct with const members.
 *
 * @author Neon, Estrayl, Farlon
 */
class Easter : public PlayerCommand {
public:
	Easter();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	static constexpr int32_t neededItem = 186000175;

	struct Reward {
		const int32_t requiredEggs;
		const int32_t itemId;
		const int64_t itemCount;
	};

	static const std::vector<Reward>& rewards();

	static const std::vector<Reward>& randomRewards();

	static std::string buildSyntaxInfo();
};

} // namespace aion::gameserver::handlers::playercommands
