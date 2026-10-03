#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when a player reports another player with /ReportAutoHunting (C_ACCUSE_CHARACTER).
 *
 * @author Jego, Neon
 */
class CM_REPORT_PLAYER : public AionClientPacket {
private:
	int32_t reportType{};
	std::string playerName;

public:
	CM_REPORT_PLAYER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
