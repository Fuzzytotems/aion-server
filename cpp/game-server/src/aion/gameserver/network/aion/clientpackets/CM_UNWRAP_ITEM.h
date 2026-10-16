#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client unwraps a packaged item in the inventory (C_UNPACK_ITEM).
 *
 * @author xTz
 */
class CM_UNWRAP_ITEM : public AionClientPacket {
private:
	int32_t objectId{};

public:
	CM_UNWRAP_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
