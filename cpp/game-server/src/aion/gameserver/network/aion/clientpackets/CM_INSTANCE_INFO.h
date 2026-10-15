#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * @author nrg, Neon
 */
class CM_INSTANCE_INFO : public AionClientPacket {
private:
	/** 0 = reset to client default values and overwrite, 1 = update team member info, 2 = overwrite only */
	int8_t updateType{};

public:
	CM_INSTANCE_INFO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
