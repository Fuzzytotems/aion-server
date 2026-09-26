#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_BIDS.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseTypeInfo.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: AuctionEndTask.getInstance().getRemainingAuctionSeconds(houseObjectId) - taskmanager/tasks/housing/AuctionEndTask.h (P5-11) is not written yet */
int32_t remainingAuctionSeconds(int32_t houseObjectId) {
	static_cast<void>(houseObjectId);
	AION_UNPORTED();
}

} // namespace

namespace {

/** Java: the lambda of DYNAMIC_BODY_PART_SIZE_CALCULATOR (SM_HOUSE_BIDS.java:21, key SM_HOUSE_BIDS@L21:87) */
struct DynamicBodyPartSizeCalculator : runtime::TaskStruct {
	int32_t operator()(model::house::HouseBids&) const { return 44; }
};

} // namespace

const runtime::PinnedCallback<int32_t(model::house::HouseBids&)> SM_HOUSE_BIDS::DYNAMIC_BODY_PART_SIZE_CALCULATOR{DynamicBodyPartSizeCalculator{}};

SM_HOUSE_BIDS::SM_HOUSE_BIDS(bool isFirstPacket, bool isLastPacket, const std::vector<runtime::Ptr<model::house::HouseBids>>& houseBidsValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_BIDS>), isFirst(isFirstPacket), isLast(isLastPacket),
	  houseBids(houseBidsValue.begin(), houseBidsValue.end()) {
}

SM_HOUSE_BIDS::~SM_HOUSE_BIDS() = default;

void SM_HOUSE_BIDS::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_HOUSE_BIDS::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	runtime::Ptr<model::house::HouseBids::Bid> lastBid = isLast ? services::HousingBidService::getInstance().findLastBid(*player) : nullptr;
	runtime::Ptr<model::house::HouseBids> bidsForRegisteredHouse =
		isLast ? services::HousingBidService::getInstance().findBidsForRegisteredHouse(*player) : nullptr;
	writeC(isFirst ? 1 : 0);
	writeC(isLast ? 1 : 0);
	writeD(!lastBid ? 0 : lastBid->getListIndex());
	writeQ(!lastBid ? 0 : lastBid->getKinah());
	writeD(!bidsForRegisteredHouse ? 0 : bidsForRegisteredHouse->getListIndex());
	writeQ(!bidsForRegisteredHouse ? 0 : bidsForRegisteredHouse->getInitialOffer()->getKinah()); // starting price
	writeH(static_cast<int32_t>(houseBids.size()));
	for (const runtime::Ref<model::house::HouseBids>& bids : houseBids) {
		runtime::Ptr<model::house::House> house = services::HousingService::getInstance().findHouse(bids->getHouseObjectId());
		writeD(bids->getListIndex());
		writeD(house->getLand()->getId());
		writeD(house->getAddress()->getId());
		writeD(house->getBuilding()->getId());
		writeD(model::templates::housing::getId(house->getHouseType())); // client seems to ignore this
		writeQ(bids->getHighestBid()->getKinah());
		writeQ(100000); // what's this? the same static value is sent in CM_REGISTER_HOUSE
		writeD(bids->getBidCount());
		writeD(remainingAuctionSeconds(bids->getHouseObjectId()));
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
