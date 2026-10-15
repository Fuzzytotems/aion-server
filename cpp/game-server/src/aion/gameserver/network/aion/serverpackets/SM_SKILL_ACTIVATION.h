#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_SKILL_ACTIVATION : public AionServerPacket {
private:
	bool isActive{};
	int32_t unk{};
	int32_t skillId{};
public:
	/** For toggle skills */
	SM_SKILL_ACTIVATION(int32_t skillId, bool isActive);
	/** For stigma remove should work in 1.5.1.15 */
	explicit SM_SKILL_ACTIVATION(int32_t skillId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
