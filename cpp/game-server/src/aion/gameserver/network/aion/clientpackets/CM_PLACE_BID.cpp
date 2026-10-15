#include "aion/gameserver/network/aion/clientpackets/CM_PLACE_BID.h"

#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingBidService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PLACE_BID::CM_PLACE_BID(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PLACE_BID.java:24-27
void CM_PLACE_BID::readImpl() {
	listIndex = readD();
	bidOffer = readQ();
}

// Java CM_PLACE_BID.java:30-35
void CM_PLACE_BID::runImpl() {
	if (configs::main::HousingConfig::ENABLE_HOUSE_AUCTIONS.load()) {
		const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
		if (!player) // Java: NullPointerException in HousingBidService.bid
			throw runtime::NullPointerException("CM_PLACE_BID without an active player");
		services::HousingBidService::getInstance().bid(*player, listIndex, bidOffer);
	}
}

AION_CLIENT_PACKET(CM_PLACE_BID);

} // namespace aion::gameserver::network::aion::clientpackets
