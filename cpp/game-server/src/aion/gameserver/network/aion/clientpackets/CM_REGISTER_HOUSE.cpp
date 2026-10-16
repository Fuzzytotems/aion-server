#include "aion/gameserver/network/aion/clientpackets/CM_REGISTER_HOUSE.h"

#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECEIVE_BIDS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingBidService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_REGISTER_HOUSE::CM_REGISTER_HOUSE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_REGISTER_HOUSE.java:31-34
void CM_REGISTER_HOUSE::readImpl() {
	bidKinah = readQ();
	readQ(); // always 100000
}

// Java CM_REGISTER_HOUSE.java:37-73
void CM_REGISTER_HOUSE::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!services::HousingBidService::getInstance().isRegisteringAllowed()) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_AUCTION_TIMEOUT());
		return;
	}
	if (!player) // Java: NullPointerException on player.getActiveHouse()
		throw runtime::NullPointerException("CM_REGISTER_HOUSE without an active player");

	const runtime::Ptr<model::house::House> house = player->getActiveHouse();
	if (!house || house->getHouseType() == model::templates::housing::HouseType::STUDIO)
		return; // should not happen

	if (house->getBids()) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_FAIL_ALREADY_REGISTED());
		return;
	}

	if (!house->isFeePaid() && configs::main::HousingConfig::ENABLE_HOUSE_PAY.load()) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_AUCTION_OVERDUE());
		return;
	}

	// Java: (long) (bidKinah * HousingConfig.AUCTION_REGISTRATION_FEE_PERCENT) - long * float in float
	const int64_t fee = static_cast<int64_t>(static_cast<float>(bidKinah) * configs::main::HousingConfig::AUCTION_REGISTRATION_FEE_PERCENT.load());

	if (!player->getInventory().tryDecreaseKinah(fee)) {
		// client has it's own validation, so we only get here if AUCTION_REGISTRATION_FEE_PERCENT is higher than the default in the client (30%)
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(fee));
		return;
	}
	if (services::HousingBidService::getInstance().auction(*house, bidKinah)) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_AUCTION_MY_HOUSE(house->getAddress()->getId()));
		sendPacket(serverpackets::SM_RECEIVE_BIDS(0));
	} else {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_AUCTION_TIMEOUT());
		player->getInventory().increaseKinah(fee);
	}
}

AION_CLIENT_PACKET(CM_REGISTER_HOUSE);

} // namespace aion::gameserver::network::aion::clientpackets
