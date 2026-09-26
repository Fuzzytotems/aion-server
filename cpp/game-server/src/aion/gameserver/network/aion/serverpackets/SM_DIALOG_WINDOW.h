#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author alexa026
 */
class SM_DIALOG_WINDOW : public AionServerPacket {
private:
	int32_t targetObjectId{};
	int32_t dialogPageId{};
	int32_t questId{};

public:
	SM_DIALOG_WINDOW(int32_t targetObjectId, int32_t dialogPageId);
	SM_DIALOG_WINDOW(int32_t targetObjectId, int32_t dialogPageId, int32_t questId);
	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
