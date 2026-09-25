#include "aion/gameserver/model/SellLimitInfo.h"

#include <string>

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model {

int64_t getSellLimit(gameobjects::player::Player& player) {
	int32_t playerLevel = player.getAccount()->getMaxPlayerLevel();
	for (const detail::SellLimitData& sellLimit : detail::SELL_LIMIT_DATA) {
		if (sellLimit.playerMinLevel <= playerLevel && sellLimit.playerMaxLevel >= playerLevel) {
			// Java: Rates.SELL_LIMIT.calcResult(player, sellLimit.limit) - the long overload, since the limit is a long
			return gameobjects::player::calcResult(gameobjects::player::Rates::SELL_LIMIT, player, sellLimit.limit);
		}
	}
	throw runtime::NoSuchElementException("Sell limit for player level: " + std::to_string(playerLevel) + " was not found");
}

} // namespace aion::gameserver::model
