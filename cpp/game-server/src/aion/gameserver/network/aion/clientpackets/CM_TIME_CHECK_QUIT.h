#pragma once

#include "aion/gameserver/network/aion/clientpackets/CM_TIME_CHECK.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * CM_TIME_CHECK under another opcode (C_ASK_GLOBAL_PLAYTIME_FATIGUE_INFO). Java's AionClientPacketFactory has its line commented out
 * (AionClientPacketFactory.java:237), so the class is registered under no opcode here either (no AION_CLIENT_PACKET marker).
 */
class CM_TIME_CHECK_QUIT : public CM_TIME_CHECK {
public:
	CM_TIME_CHECK_QUIT(int32_t opcode, const StateSet& validStates) : CM_TIME_CHECK(opcode, validStates) {}
};

} // namespace aion::gameserver::network::aion::clientpackets
