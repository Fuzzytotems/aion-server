#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is response for CM_CHECK_NICKNAME.<br>
 * It sends client information if name can be used or not
 *
 * @author -Nemesiss-
 */
class SM_NICKNAME_CHECK_RESPONSE : public AionServerPacket {
private:
	int32_t value{};
public:
	/** Constructs new <tt>SM_NICKNAME_CHECK_RESPONSE</tt> packet */
	explicit SM_NICKNAME_CHECK_RESPONSE(int32_t value);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
