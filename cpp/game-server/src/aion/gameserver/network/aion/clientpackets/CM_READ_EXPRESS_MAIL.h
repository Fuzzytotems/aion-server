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
 * Summons or dismisses the express mail postman (C_MAIL_POSTMAN).
 *
 * @author antness thx to Guapo for sniffing
 */
class CM_READ_EXPRESS_MAIL : public AionClientPacket {
private:
	int8_t action = 0;

public:
	CM_READ_EXPRESS_MAIL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
