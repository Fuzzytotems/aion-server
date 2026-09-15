#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OWNER_INFO.h"

#include <chrono>

#include "aion/gameserver/model/gameobjects/player/HouseOwnerState.h"
#include "aion/gameserver/model/gameobjects/player/HouseOwnerStateInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_OWNER_INFO::SM_HOUSE_OWNER_INFO(model::gameobjects::player::Player& player) : AionServerPacket(opcodeOf<SM_HOUSE_OWNER_INFO>) {
	using model::gameobjects::player::HouseOwnerState;
	for (runtime::Ptr<model::house::House> house : *player.getHouses()) {
		if (house->isInactive())
			inactiveHouse = house;
		else
			activeHouse = house;
	}
	if (!activeHouse) {
		playerHouseOwnerState = model::gameobjects::player::getId(HouseOwnerState::SINGLE_HOUSE);
		if (services::HousingService::getInstance().canOwnHouse(player, false))
			playerHouseOwnerState |= model::gameobjects::player::getId(HouseOwnerState::BIDDING_ALLOWED);
	} else {
		playerHouseOwnerState =
			model::gameobjects::player::getId(HouseOwnerState::HAS_OWNER) | model::gameobjects::player::getId(HouseOwnerState::BIDDING_ALLOWED);
	}
}

SM_HOUSE_OWNER_INFO::~SM_HOUSE_OWNER_INFO() = default;

void SM_HOUSE_OWNER_INFO::writeImpl(AionConnection* con) {
	writeD(!activeHouse ? 0 : activeHouse->getAddress()->getId());
	writeD(!activeHouse ? 0 : activeHouse->getBuilding()->getId());
	// (SINGLE_HOUSE | BIDDING_ALLOWED) enables studio buy button at Parrine and disables house icon in profile
	writeC(playerHouseOwnerState);
	writeC(!activeHouse ? 0 : activeHouse->getTownLevel());
	// controls date and color in player profile house icon tooltip, as well as overdue pay amount when paying
	writeD(calculateWeeksUntilNextPay());
	writeD(!inactiveHouse ? 0 : inactiveHouse->getAddress()->getId());
	writeD(!inactiveHouse ? 0 : inactiveHouse->getBuilding()->getId());
	// seconds until new house (inactiveHouse) will be activated / old one (activeHouse) gets removed
	writeD(!inactiveHouse ? 0 : inactiveHouse->secondsUntilGraceEnd());
}

int32_t SM_HOUSE_OWNER_INFO::calculateWeeksUntilNextPay() {
	int32_t weeks = 0;
	if (activeHouse) {
		std::optional<commons::database::Timestamp> nextPay = activeHouse->getNextPay();
		if (!nextPay) { // newly acquired houses have one week free, nextPay will be set on the next house maintenance
			weeks = 1;
		} else {
			const utils::time::ServerTime::ZonedDateTime now = utils::time::ServerTime::now();
			const std::chrono::local_time<std::chrono::milliseconds> local = now.get_local_time();
			const std::chrono::local_days day = std::chrono::floor<std::chrono::days>(local);
			const bool isSundayAfterAuction = std::chrono::weekday(day) == std::chrono::Sunday &&
				std::chrono::duration_cast<std::chrono::hours>(local - day).count() >= 12;
			// Java: Duration.between(now.toInstant(), nextPay.toInstant()).toDays(): the Duration's seconds (floored, its nanos are never negative)
			// divided by 86400, truncated towards zero
			const int64_t seconds = std::chrono::floor<std::chrono::seconds>(*nextPay - now.get_sys_time()).count();
			int64_t days = seconds / 86400;
			weeks = static_cast<int32_t>(days / 7);
			if (days < 0 && isSundayAfterAuction) // workaround for auction day, client counts sunday afternoon to new week
				weeks--;
			else if (days >= 0 && !isSundayAfterAuction)
				weeks++;
		}
	}
	return weeks;
}

} // namespace aion::gameserver::network::aion::serverpackets
