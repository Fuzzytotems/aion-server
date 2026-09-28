#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is asking if given nickname is ok/free?.
 *
 * @author -Nemesiss-, cura
 */
class CM_CHECK_NICKNAME : public AionClientPacket {
private:
	std::string nick;

public:
	CM_CHECK_NICKNAME(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
