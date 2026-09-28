#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is used to update current exp / recoverable exp / max exp values.
 *
 * @author Luno
 * @updated by alexa026
 */
class SM_STATUPDATE_EXP : public AionServerPacket {
private:
	int64_t currentExp{};
	int64_t recoverableExp{};
	int64_t maxExp{};
	int64_t curBoostExp{}; // Java: = 0
	int64_t maxBoostExp{}; // Java: = 0
public:
	SM_STATUPDATE_EXP(int64_t currentExp, int64_t recoverableExp, int64_t maxExp, int64_t rep1, int64_t rep2);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
