#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: InstanceProgressionType.getId() (REINFORCE_MEMBER 12 MiB, PREPARING 1 MiB, START_PROGRESS 2 MiB, END_PROGRESS 3 MiB) */
int32_t progressionTypeId(model::instance::InstanceProgressionType type) {
	switch (type) {
		case model::instance::InstanceProgressionType::REINFORCE_MEMBER:
			return 12 * 1024 * 1024;
		case model::instance::InstanceProgressionType::PREPARING:
			return 1 * 1024 * 1024;
		case model::instance::InstanceProgressionType::START_PROGRESS:
			return 2 * 1024 * 1024;
		case model::instance::InstanceProgressionType::END_PROGRESS:
			break;
	}
	return 3 * 1024 * 1024;
}

} // namespace

SM_INSTANCE_SCORE::SM_INSTANCE_SCORE(int32_t mapIdValue, instanceinfo::ArenaScoreWriter& arenaScoreInfo)
	: AionServerPacket(opcodeOf<SM_INSTANCE_SCORE>) {
	// Java: this(mapId, arenaScoreInfo, arenaScoreInfo.getInstanceScore().getTime()) - PvPArenaScore.getTime needs
	// model/instance/instancescore/PvPArenaScore.h (P5-13), which is not written yet
	static_cast<void>(mapIdValue);
	static_cast<void>(arenaScoreInfo);
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
	writeD(mapId);
	writeD(instanceTime);
	writeD(progressionTypeId(instanceScoreWriter->getInstanceScore()->getInstanceProgressionType()));
	instanceScoreWriter->writeMe(getBuf());
}

} // namespace aion::gameserver::network::aion::serverpackets
