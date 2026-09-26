#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Dns, ginho1, nrg, xTz
 */
class SM_INSTANCE_SCORE : public AionServerPacket {
private:
	int32_t mapId{};
	int32_t instanceTime{};
	runtime::Ref<instanceinfo::InstanceScoreWriter> instanceScoreWriter{};

public:
	SM_INSTANCE_SCORE(int32_t mapId, instanceinfo::ArenaScoreWriter& arenaScoreInfo);
	SM_INSTANCE_SCORE(int32_t mapId, instanceinfo::InstanceScoreWriter& instanceScoreWriter);
	SM_INSTANCE_SCORE(int32_t mapId, instanceinfo::InstanceScoreWriter& instanceScoreWriter, int32_t instanceTime);
	~SM_INSTANCE_SCORE() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
