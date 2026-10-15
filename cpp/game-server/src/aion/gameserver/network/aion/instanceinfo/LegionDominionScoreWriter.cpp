#include "aion/gameserver/network/aion/instanceinfo/LegionDominionScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/instancescore/LegionDominionScore.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java LegionDominionScoreWriter.java:12-14
LegionDominionScoreWriter::LegionDominionScoreWriter(model::instance::instancescore::LegionDominionScore& reward) : InstanceScoreWriter(reward) {
}

LegionDominionScoreWriter::~LegionDominionScoreWriter() = default;

runtime::Ref<LegionDominionScoreWriter> LegionDominionScoreWriter::create(model::instance::instancescore::LegionDominionScore& reward) {
	return runtime::makeRef<LegionDominionScoreWriter>(reward);
}

model::instance::instancescore::LegionDominionScore& LegionDominionScoreWriter::score() const {
	return static_cast<model::instance::instancescore::LegionDominionScore&>(*instanceScore); // constructed from a LegionDominionScore
}

// Java LegionDominionScoreWriter.java:17-32
void LegionDominionScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	writeD(buf, score().getPoints());
	writeD(buf, 0); // unk
	writeD(buf, 0); // unk
	writeD(buf, 0); // unk
	writeD(buf, score().getFinalGP());
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
