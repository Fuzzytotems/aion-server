#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player drops an item onto another one of another storage, to swap the two (C_SWAP_ITEM_SLOT).
 * <p>
 * C++ only: `CM_REPLACE_ITEMTestAccess` (tests/cm_lz/ItemStoragePacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without getters.
 *
 * @author kosyachok
 */
class CM_REPLACE_ITEM : public AionClientPacket {
	friend struct CM_REPLACE_ITEMTestAccess;

private:
	int8_t sourceStorageType{};
	int32_t sourceItemObjId{};
	int8_t replaceStorageType{};
	int32_t replaceItemObjId{};

public:
	CM_REPLACE_ITEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
