#pragma once

#include <cstdint>

#include "aion/gameserver/model/trade/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client's trade with a shop, a private store or a merchant pet (C_BUY_SELL): buy from an npc, sell to it, buy back from it, buy from a
 * player's private store (the item indices of the seller's list), sell to a pet. readImpl builds the trade list (or, for a buy-back, the
 * repurchase list) and audits an impossible item amount, count or item id; runImpl hands the list to the service of the arm.
 * <p>
 * C++ only: `CM_BUY_ITEMTestAccess` (tests/cm_ak/BuyItemPacketTest.cpp) reads the fields readImpl decoded, which Java keeps private without
 * getters.
 *
 * @author orz, ATracer, Simple, xTz
 */
class CM_BUY_ITEM : public AionClientPacket {
	friend struct CM_BUY_ITEMTestAccess;

private:
	int32_t sellerObjId{};
	int16_t tradeActionId{};
	int32_t amount{};
	int32_t itemId{};
	int64_t count{};
	bool isAudit{};
	runtime::Ref<model::trade::TradeList> tradeList;
	runtime::Ref<model::trade::RepurchaseList> repurchaseList;

public:
	CM_BUY_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
