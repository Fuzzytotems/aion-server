#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A godstone socketed into an equipped weapon at an npc. Java's AionClientPacketFactory has its line commented out
 * (AionClientPacketFactory.java:119: 4.8 sockets a godstone through CM_MANASTONE, with no npc). So the class is registered under no opcode
 * here either: it has no AION_CLIENT_PACKET marker, and no client can reach it.
 * <p>
 * C++ only: `CM_GODSTONE_SOCKETTestAccess` (the packet tests) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author ATracer
 */
class CM_GODSTONE_SOCKET : public AionClientPacket {
	friend struct CM_GODSTONE_SOCKETTestAccess;

private:
	int32_t npcObjectId{};
	int32_t weaponId{};
	int32_t stoneId{};

public:
	CM_GODSTONE_SOCKET(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
