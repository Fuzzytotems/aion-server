#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player moves part of a stack to another slot or storage, or merges two stacks
 * (C_MOVE_STACKABLE_ITEM).
 * <p>
 * C++ notes: Java's fields are package-private; no other class of the package reads them, so they are private here.
 * `CM_SPLIT_ITEMTestAccess` (tests/cm_lz/ItemStoragePacketsTest.cpp) reads the fields readImpl decoded.
 *
 * @author kosyak
 */
class CM_SPLIT_ITEM : public AionClientPacket {
	friend struct CM_SPLIT_ITEMTestAccess;

private:
	int32_t sourceItemObjId{};
	int8_t sourceStorageType{};
	int64_t itemAmount{};
	int32_t destinationItemObjId{};
	int8_t destinationStorageType{};
	int16_t slotNum{};

public:
	CM_SPLIT_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
