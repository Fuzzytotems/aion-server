#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author ginho1
 */
class CM_CHECK_PAK : public AionClientPacket {
private:
	/** Java: @SuppressWarnings("unused") private byte unk (always 2) */
	int8_t unk{};
	std::string pakStatus;

public:
	CM_CHECK_PAK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
