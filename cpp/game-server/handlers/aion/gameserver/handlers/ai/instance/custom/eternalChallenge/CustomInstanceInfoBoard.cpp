#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceInfoBoard.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

AION_AI(CustomInstanceInfoBoard, "custom_instance_info_board");

/** Java: private static final Map<Integer, Long> lastLeaderboardOpenTime = new HashMap<>() and its monitor */
struct CustomInstanceInfoBoard::LastLeaderboardOpenTimes {
	runtime::Monitor monitor{AION_LOCK_CLASS(CustomInstanceInfoBoard::lastLeaderboardOpenTime)};
	std::map<int32_t, int64_t> times;
};

CustomInstanceInfoBoard::LastLeaderboardOpenTimes& CustomInstanceInfoBoard::lastLeaderboardOpenTimes() {
	static auto* const times = new LastLeaderboardOpenTimes();
	return *times;
}


// Java CustomInstanceInfoBoard.java:22-25
CustomInstanceInfoBoard::CustomInstanceInfoBoard(Npc& owner) : GeneralNpcAI(owner) {
	owner.setMasterName("Eternal Challenge");
}

// Java CustomInstanceInfoBoard.java:27-37
void CustomInstanceInfoBoard::handleDialogStart(Player& player) {
	LastLeaderboardOpenTimes& lastLeaderboardOpenTime = lastLeaderboardOpenTimes();
	SYNCHRONIZED(lastLeaderboardOpenTime.monitor) {
		int64_t now = commons::utils::currentTimeMillis();
		auto lastOpenTime = lastLeaderboardOpenTime.times.find(player.getObjectId());
		if (lastOpenTime != lastLeaderboardOpenTime.times.end() && lastOpenTime->second + 3 * 1000 > now)
			return; // simple flood protection so we don't have to cache the whole leaderboard
		lastLeaderboardOpenTime.times[player.getObjectId()] = now;
	}
	gameserver::custom::instance::CustomInstanceService::getInstance().openLeaderboard(player, getOwner().getRace());
}

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
