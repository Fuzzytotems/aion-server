#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Ritsu
 */
class SM_AFTER_SIEGE_LOCINFO_475 : public AionServerPacket {
public:
	/** Java: implicit default constructor */
	SM_AFTER_SIEGE_LOCINFO_475();

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
