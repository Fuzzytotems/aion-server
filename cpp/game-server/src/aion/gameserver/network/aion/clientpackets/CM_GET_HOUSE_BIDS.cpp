#include "aion/gameserver/network/aion/clientpackets/CM_GET_HOUSE_BIDS.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_BIDS.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GET_HOUSE_BIDS::CM_GET_HOUSE_BIDS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_GET_HOUSE_BIDS.java:27-29
void CM_GET_HOUSE_BIDS::readImpl() {
}

// Java CM_GET_HOUSE_BIDS.java:32-38
void CM_GET_HOUSE_BIDS::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player) // Java: NullPointerException on player.getRace()
		throw runtime::NullPointerException("CM_GET_HOUSE_BIDS without an active player");
	std::vector<runtime::Ptr<model::house::HouseBids>> houseBids = services::HousingBidService::getInstance().getBidInfo(player->getRace());
	std::vector<runtime::Ref<model::house::HouseBids>> refs;
	refs.reserve(houseBids.size());
	for (const runtime::Ptr<model::house::HouseBids>& bids : houseBids)
		refs.emplace_back(bids.get());
	utils::collections::DynamicServerPacketBodySplitList<model::house::HouseBids> bidsSplitList(std::move(refs), true,
		serverpackets::SM_HOUSE_BIDS::STATIC_BODY_SIZE, serverpackets::SM_HOUSE_BIDS::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
	for (utils::collections::ListPart<model::house::HouseBids>& part : bidsSplitList)
		utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_HOUSE_BIDS(part.isFirst(), part.isLast(), part.borrowed()));
}

AION_CLIENT_PACKET(CM_GET_HOUSE_BIDS);

} // namespace aion::gameserver::network::aion::clientpackets
