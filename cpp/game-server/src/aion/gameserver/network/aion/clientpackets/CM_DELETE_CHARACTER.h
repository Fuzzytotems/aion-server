#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is requesting deletion of character.
 *
 * @author -Nemesiss-
 */
class CM_DELETE_CHARACTER : public AionClientPacket {
private:
	/** PlayOk2 - we dont care... */
	int32_t playOk2{}; // Java: @SuppressWarnings("unused")
	/** ObjectId of character that should be deleted. */
	int32_t chaOid{};

public:
	CM_DELETE_CHARACTER(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
