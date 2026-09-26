#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_MARK_FRIENDLIST : public AionServerPacket {
public:
	SM_MARK_FRIENDLIST();

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
