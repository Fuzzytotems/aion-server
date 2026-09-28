#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author ginho1
 */
class CM_CHARACTER_PASSKEY : public AionClientPacket {
private:
	int16_t type{};
	/** Java: new String(readB(48), UTF_16LE) - 24 characters including the NUL padding */
	std::string passkey;
	std::string newPasskey;

public:
	CM_CHARACTER_PASSKEY(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	void checkBlock(int32_t accountId, int32_t wrongCount);
};

} // namespace aion::gameserver::network::aion::clientpackets
