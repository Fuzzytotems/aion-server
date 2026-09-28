#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author Rolandas
 */
class CM_SUBZONE_CHANGE : public AionClientPacket {
private:
	int8_t unk{};

public:
	CM_SUBZONE_CHANGE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
