#include "aion/gameserver/network/aion/instanceinfo/EternalBastionScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/instancescore/NormalScore.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java EternalBastionScoreWriter.java:12-14
EternalBastionScoreWriter::EternalBastionScoreWriter(model::instance::instancescore::NormalScore& reward) : InstanceScoreWriter(reward) {
}

EternalBastionScoreWriter::~EternalBastionScoreWriter() = default;

runtime::Ref<EternalBastionScoreWriter> EternalBastionScoreWriter::create(model::instance::instancescore::NormalScore& reward) {
	return runtime::makeRef<EternalBastionScoreWriter>(reward);
}

model::instance::instancescore::NormalScore& EternalBastionScoreWriter::score() const {
	return static_cast<model::instance::instancescore::NormalScore&>(*instanceScore); // constructed from a NormalScore
}

// Java EternalBastionScoreWriter.java:17-31
void EternalBastionScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	writeD(buf, score().getPoints());
	writeQ(buf, 0); // unk
	writeD(buf, score().getRank());
	writeD(buf, 0); // unk
	writeD(buf, score().getFinalAp());
	writeD(buf, score().getRewardItem1());
	writeD(buf, score().getRewardItem1Count());
	writeD(buf, score().getRewardItem2());
	writeD(buf, score().getRewardItem2Count());
	writeD(buf, score().getRewardItem3());
	writeD(buf, score().getRewardItem3Count());
	writeD(buf, score().getRewardItem4());
	writeD(buf, score().getRewardItem4Count());
}

} // namespace aion::gameserver::network::aion::instanceinfo
