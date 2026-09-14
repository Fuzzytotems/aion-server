#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INSTANCE_SCORE::SM_INSTANCE_SCORE(int32_t mapIdValue, instanceinfo::ArenaScoreWriter& arenaScoreInfo)
	: AionServerPacket(opcodeOf<SM_INSTANCE_SCORE>) {
	AION_UNPORTED();
}

SM_INSTANCE_SCORE::SM_INSTANCE_SCORE(int32_t mapIdValue, instanceinfo::InstanceScoreWriter& instanceScoreWriterValue)
	: SM_INSTANCE_SCORE(mapIdValue, instanceScoreWriterValue, 0) {
}

SM_INSTANCE_SCORE::SM_INSTANCE_SCORE(int32_t mapIdValue, instanceinfo::InstanceScoreWriter& instanceScoreWriterValue, int32_t instanceTimeValue)
	: AionServerPacket(opcodeOf<SM_INSTANCE_SCORE>), mapId(mapIdValue), instanceTime(instanceTimeValue),
	  instanceScoreWriter(instanceScoreWriterValue) {
}

SM_INSTANCE_SCORE::~SM_INSTANCE_SCORE() = default;

void SM_INSTANCE_SCORE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
