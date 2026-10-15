#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client's choice on a teleporter's or a flight master's map (C_DESTINATION_AIRPORT): the npc and the location id.
 * <p>
 * C++ only: `CM_TELEPORT_SELECTTestAccess` (tests/cm_lz/TeleportSelectPacketTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without a getter.
 *
 * @author ATracer, orz, KID
 */
class CM_TELEPORT_SELECT : public AionClientPacket {
	friend struct CM_TELEPORT_SELECTTestAccess;

private:
	/** NPC object ID */
	int32_t targetObjId{};
	/** Destination of teleport */
	int32_t locId{};

public:
	CM_TELEPORT_SELECT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
