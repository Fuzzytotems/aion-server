#pragma once

#include <cstdint>
#include <map>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/**
 * The Eternal Challenge's leaderboard board ("custom_instance_info_board"): a dialog opens the leaderboard of the board's race, at most every
 * 3 seconds per player (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/instance/custom/eternalChallenge/CustomInstanceInfoBoard.java. C++: the static HashMap guarded by its own monitor
 * (Java synchronizes on it) is a plain map under a static immortal Monitor.
 */
class CustomInstanceInfoBoard : public GeneralNpcAI {
public:
	explicit CustomInstanceInfoBoard(Npc& owner);

protected:
	void handleDialogStart(Player& player) override;

private:
	struct LastLeaderboardOpenTimes;
	static LastLeaderboardOpenTimes& lastLeaderboardOpenTimes();
};

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
