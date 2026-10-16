#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The instance matchmaker's requests (C_MATCHMAKER_REQ): register (100), cancel the registration (101), enter (102), cancel the entry (103) and
 * the click on a periodic instance's icon (104).
 *
 * @author Shepper, Guapo, nrg
 */
class CM_AUTO_GROUP : public AionClientPacket {
public:
	CM_AUTO_GROUP(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t instanceMaskId = 0;
	int8_t windowId = 0;
	int8_t entryRequestId = 0;
};

} // namespace aion::gameserver::network::aion::clientpackets
