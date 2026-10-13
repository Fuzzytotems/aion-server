#include "aion/gameserver/network/aion/instanceinfo/CrucibleScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/InstanceProgressionTypeInfo.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"

namespace aion::gameserver::network::aion::instanceinfo {

CrucibleScoreWriter::CrucibleScoreWriter(model::instance::instancescore::InstanceScore& reward) : InstanceScoreWriter(reward) {}

CrucibleScoreWriter::~CrucibleScoreWriter() = default;

runtime::Ref<CrucibleScoreWriter> CrucibleScoreWriter::create(model::instance::instancescore::InstanceScore& reward) {
	return runtime::makeRef<CrucibleScoreWriter>(reward);
}

// Java CrucibleScoreWriter.java:20-33 (Java InstanceScore<CruciblePlayerReward>: a reward of another type is a ClassCastException)
void CrucibleScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	int32_t playerCount = 0;
	for (const runtime::Ptr<model::instance::playerreward::InstancePlayerReward>& reward : instanceScore->getPlayerRewards()) {
		runtime::Ptr<model::instance::playerreward::CruciblePlayerReward> playerReward =
			runtime::cast<model::instance::playerreward::CruciblePlayerReward>(reward);
		writeD(buf, playerReward->getOwnerId()); // obj
		writeD(buf, playerReward->getPoints()); // points
		writeD(buf, model::instance::isEndProgress(instanceScore->getInstanceProgressionType()) ? 3 : 1);
		writeD(buf, playerReward->getInsignia());
		playerCount++;
	}
	if (playerCount < 6) {
		skip(buf, 16 * (6 - playerCount)); // spaces (Java: writeB(buf, new byte[...]))
	}
}

} // namespace aion::gameserver::network::aion::instanceinfo
