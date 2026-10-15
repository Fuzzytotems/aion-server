#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/trade/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player opens his private store with the items and prices he lists, or closes it with an empty list (C_PERSONAL_SHOP).
 * <p>
 * C++ only: `CM_PRIVATE_STORETestAccess` (tests/cm_lz/PrivateStorePacketsTest.cpp) reads the items readImpl decoded, which Java keeps private
 * without a getter.
 *
 * @author Simple
 */
class CM_PRIVATE_STORE : public AionClientPacket {
	friend struct CM_PRIVATE_STORETestAccess;

private:
	std::vector<runtime::Ref<model::trade::TradePSItem>> tradePSItems;

public:
	CM_PRIVATE_STORE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
