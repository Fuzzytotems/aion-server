#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player picks the reward of a selectable decomposable item (C_SELECT_DISASSEMBLY_ITEM), the answer to SM_FIRST_SHOW_DECOMPOSABLE: the
 * index into the rewards he can obtain (the item's selectable list without the other race's and class's rewards).
 * <p>
 * C++ only: `CM_SELECT_DECOMPOSABLETestAccess` (tests/cm_lz/SelectDecomposablePacketTest.cpp) reads the fields readImpl decoded, which Java keeps
 * private without getters.
 *
 * @author xTz
 */
class CM_SELECT_DECOMPOSABLE : public AionClientPacket {
	friend struct CM_SELECT_DECOMPOSABLETestAccess;

private:
	int32_t objectId{};
	int32_t unk{}; // Java: @SuppressWarnings("unused")
	int32_t index{};

public:
	CM_SELECT_DECOMPOSABLE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
