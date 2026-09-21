#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Client sends this only once.
 *
 * @author Luno
 */
class CM_CHAT_AUTH : public AionClientPacket {
private:
	int32_t objectId{};              // Java: @SuppressWarnings("unused") local of readImpl
	std::vector<uint8_t> macAddress; // Java: @SuppressWarnings("unused") local of readImpl

public:
	/** Constructor */
	CM_CHAT_AUTH(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
