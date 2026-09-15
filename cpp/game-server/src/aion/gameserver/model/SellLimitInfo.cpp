#include "aion/gameserver/model/SellLimitInfo.h"

#include <string>

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model {

int64_t getSellLimit(gameobjects::player::Player& player) {
	int32_t playerLevel = player.getAccount()->getMaxPlayerLevel();
	for (const detail::SellLimitData& sellLimit : detail::SELL_LIMIT_DATA) {
		if (sellLimit.playerMinLevel <= playerLevel && sellLimit.playerMaxLevel >= playerLevel) {
			// Java: return Rates.SELL_LIMIT.calcResult(player, sellLimit.limit); the Rates companion (P4-12) does not exist yet
			AION_UNPORTED();
		}
	}
	throw runtime::NoSuchElementException("Sell limit for player level: " + std::to_string(playerLevel) + " was not found");
}

} // namespace aion::gameserver::model
