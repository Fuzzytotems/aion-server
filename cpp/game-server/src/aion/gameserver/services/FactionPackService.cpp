#include "aion/gameserver/services/FactionPackService.h"

#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

// Java LocalDateTime.of(year, month, day, hour, minute, second)
static std::chrono::local_time<std::chrono::milliseconds> localDateTime(int year, unsigned month, unsigned day, int hour, int minute, int second) {
	return std::chrono::local_days{std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}} + std::chrono::hours{hour} +
		std::chrono::minutes{minute} + std::chrono::seconds{second};
}

FactionPackService::FactionPackService()
	: elyosMinCreationTime(localDateTime(2020, 9, 14, 0, 0, 0)), elyosMaxCreationTime(localDateTime(2020, 9, 26, 23, 59, 59)),
	  asmodianMinCreationTime(localDateTime(2022, 6, 18, 0, 0, 0)), asmodianMaxCreationTime(localDateTime(2022, 7, 19, 23, 59, 59)) {
	AION_UNPORTED();
}

FactionPackService::~FactionPackService() = default;

FactionPackService& FactionPackService::getInstance() {
	static FactionPackService instance; // Java SingletonHolder
	return instance;
}

void FactionPackService::addPlayerCustomReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void FactionPackService::sendRewards(model::gameobjects::player::Player& player, std::chrono::local_time<std::chrono::milliseconds> minCreationTime, std::chrono::local_time<std::chrono::milliseconds> maxCreationTime) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
