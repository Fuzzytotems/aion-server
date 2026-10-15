#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * ascension quest's morph
 *
 * @author wylovech
 */
class SM_ASCENSION_MORPH : public AionServerPacket {
private:
	int32_t inascension{};

public:
	explicit SM_ASCENSION_MORPH(int32_t inascension);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
