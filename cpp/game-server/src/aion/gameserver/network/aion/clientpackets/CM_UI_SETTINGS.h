#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author ATracer
 */
class CM_UI_SETTINGS : public AionClientPacket {
private:
	int8_t settingsType{};
	std::vector<uint8_t> data;
	int32_t size{}; // Java: @SuppressWarnings("unused")

public:
	CM_UI_SETTINGS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
