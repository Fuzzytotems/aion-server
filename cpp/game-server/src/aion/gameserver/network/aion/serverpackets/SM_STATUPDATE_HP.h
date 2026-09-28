#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is used to update current hp and max hp values.
 *
 * @author Luno
 */
class SM_STATUPDATE_HP : public AionServerPacket {
private:
	int32_t currentHp{};
	int32_t maxHp{};
public:
	SM_STATUPDATE_HP(int32_t currentHp, int32_t maxHp);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
