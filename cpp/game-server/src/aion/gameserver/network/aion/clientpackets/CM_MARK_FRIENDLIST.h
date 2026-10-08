#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Received when the client asks whether the friend list is marked (C_REQUEST_MARK_FRIENDLIST).
 *
 * @author xTz, Rolandas
 */
class CM_MARK_FRIENDLIST : public AionClientPacket {
private:

public:
	CM_MARK_FRIENDLIST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
