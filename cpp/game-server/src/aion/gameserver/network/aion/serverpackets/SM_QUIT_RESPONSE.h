#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is response for CM_QUIT
 *
 * @author -Nemesiss-
 */
class SM_QUIT_RESPONSE : public AionServerPacket {
private:
	bool edit_mode{}; // Java: = false
public:
	SM_QUIT_RESPONSE();
	explicit SM_QUIT_RESPONSE(bool edit_mode);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
