#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is requesting cancellation of character deleting.
 *
 * @author -Nemesiss-
 */
class CM_RESTORE_CHARACTER : public AionClientPacket {
private:
	/** PlayOk2 - we dont care... */
	int32_t playOk2{}; // Java: @SuppressWarnings("unused")
	/** ObjectId of character that deletion should be canceled */
	int32_t chaOid{};

public:
	/** Constructs new instance of <tt>CM_RESTORE_CHARACTER </tt> packet */
	CM_RESTORE_CHARACTER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
