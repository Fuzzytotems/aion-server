#include "aion/gameserver/services/player/PlayerLimitService.h"

#include <algorithm>
#include <optional>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/SellLimitInfo.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::player {

namespace {

/** Java long multiplication (two's complement wrap-around; a signed overflow is undefined in C++) */
int64_t javaMultiply(int64_t a, int64_t b) {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

} // namespace

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.player.PlayerLimitService@L52:38

// Java PlayerLimitService.java:22-49
int64_t PlayerLimitService::updateSellLimit(model::gameobjects::player::Player& player, int64_t itemPrice, int64_t itemCount) {
	if (!configs::main::CustomConfig::LIMITS_ENABLED.load() || itemPrice == 0)
		return itemCount;

	int32_t accountId = player.getAccount()->getId();
	std::optional<int64_t> cachedLimit = sellLimit.get(accountId);
	int64_t limit;
	if (!cachedLimit) {
		limit = model::getSellLimit(player);
		// java-race: Java keeps its own getSellLimit value when a concurrent sale of the same account put one first; so does the port
		sellLimit.putIfAbsent(accountId, limit);
	} else {
		limit = *cachedLimit;
	}

	if (itemPrice < 0 || itemCount <= 0)
		return 0;

	int64_t possibleCount = std::max<int64_t>(0, limit / itemPrice);
	if (configs::main::CustomConfig::LIMITS_ENABLE_DYNAMIC_CAP.load() && possibleCount < itemCount)
		possibleCount += 1;

	if (possibleCount == 0 || limit == 0) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DAY_CANNOT_SELL_NPC(limit));
		return 0;
	} else {
		int64_t useCount = std::min(possibleCount, itemCount);
		limit -= std::min(limit, javaMultiply(itemPrice, useCount));
		sellLimit.put(accountId, limit);
		return useCount;
	}
}

void PlayerLimitService::scheduleUpdate() {
	const cron::CronExpression* limitsUpdate = configs::main::CustomConfig::LIMITS_UPDATE.load();
	if (limitsUpdate == nullptr)
		throw runtime::NullPointerException("CustomConfig.LIMITS_UPDATE"); // Java: CronService.schedule with a null expression
	cron::CronService::getInstance().schedule([] { sellLimit.clear(); }, *limitsUpdate, true);
}

PlayerLimitService& PlayerLimitService::getInstance() {
	static PlayerLimitService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services::player
