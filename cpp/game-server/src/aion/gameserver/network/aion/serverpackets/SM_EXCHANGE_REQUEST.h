#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author -Avol-
 */
class SM_EXCHANGE_REQUEST : public AionServerPacket {
private:
	std::string receiver{};

public:
	explicit SM_EXCHANGE_REQUEST(std::string_view receiver);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
