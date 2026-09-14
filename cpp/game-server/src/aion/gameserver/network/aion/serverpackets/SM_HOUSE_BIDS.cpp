#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_BIDS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

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
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
