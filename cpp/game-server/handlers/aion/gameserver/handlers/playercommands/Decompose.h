#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

#include <cstdint>
#include <string_view>

namespace aion::gameserver::handlers::playercommands {

struct Decompose_Task;

/**
 * .decompose: opens decomposable items, one per casting delay, until the count is reached or the player is disturbed.
 * <p>
 * C++: Java's anonymous Runnable of startTask (its fields remainingCount, totalCount, observer) and the anonymous ItemUseObserver it creates
 * are one callback struct, Decompose_Task of the .cpp: an ItemUseObserver that also holds the runnable's state and run(). The observer field
 * is the struct itself, so the scheduled task and the player's ObserveController hold the same object and no reference cycle forms.
 */
class Decompose : public PlayerCommand {
	friend struct Decompose_Task;

public:
	Decompose();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void startTask(Player& player, int32_t itemId, int64_t count, const DecomposeAction& decomposeAction);

	void cancelTask(Player& player, ItemUseObserver& observer, std::string_view message);
};

} // namespace aion::gameserver::handlers::playercommands
