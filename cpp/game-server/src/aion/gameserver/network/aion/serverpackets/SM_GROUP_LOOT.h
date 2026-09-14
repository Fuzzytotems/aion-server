#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rhys2002, Sykra
 */
class SM_GROUP_LOOT : public AionServerPacket {
private:
	int32_t groupId{};
	int32_t index{};
	int32_t itemCount{};
	int32_t itemId{};
	int32_t unk3{};
	int32_t lootCorpseId{};
	int32_t distributionId{};
	int32_t playerId{};
	int64_t luck{};

public:
	SM_GROUP_LOOT(int32_t groupId, int32_t playerId, int32_t itemId, int32_t itemCount, int32_t lootCorpseId, int32_t distributionId, int64_t luck,
		int32_t index);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
