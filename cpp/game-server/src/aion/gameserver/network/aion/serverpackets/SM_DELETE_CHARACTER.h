#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * In this packet Server is sending response for CM_DELETE_CHARACTER.
 *
 * @author -Nemesiss-
 */
class SM_DELETE_CHARACTER : public AionServerPacket {
private:
	int32_t playerObjId{};
	int32_t deletionTime{};

public:
	/** Constructs new <tt>SM_DELETE_CHARACTER </tt> packet */
	SM_DELETE_CHARACTER(int32_t playerObjId, int32_t deletionTime);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
