#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * In this packet Server is sending response for CM_RESTORE_CHARACTER.
 *
 * @author -Nemesiss-
 */
class SM_RESTORE_CHARACTER : public AionServerPacket {
private:
	int32_t chaOid{};
	bool success{};
public:
	/** Constructs new <tt>SM_RESTORE_CHARACTER </tt> packet */
	SM_RESTORE_CHARACTER(int32_t chaOid, bool success);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
