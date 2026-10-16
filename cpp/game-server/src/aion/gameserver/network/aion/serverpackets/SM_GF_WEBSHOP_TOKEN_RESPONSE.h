#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Artur
 */
class SM_GF_WEBSHOP_TOKEN_RESPONSE : public AionServerPacket {
private:
	std::string token{};

public:
	explicit SM_GF_WEBSHOP_TOKEN_RESPONSE(std::string_view token);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
