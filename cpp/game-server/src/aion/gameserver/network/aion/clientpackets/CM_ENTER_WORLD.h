#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packets aion client is asking if given char [by oid] may login into game [ie start playing].
 *
 * @author -Nemesiss-, Avol, Neon
 */
class CM_ENTER_WORLD : public AionClientPacket {
private:
	/** Object Id of player that is entering world */
	int32_t objectId{};

public:
	CM_ENTER_WORLD(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
