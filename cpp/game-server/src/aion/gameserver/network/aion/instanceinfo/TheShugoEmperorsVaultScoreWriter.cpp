#include "aion/gameserver/network/aion/instanceinfo/TheShugoEmperorsVaultScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/instancescore/NormalScore.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java TheShugoEmperorsVaultScoreWriter.java:12-14
TheShugoEmperorsVaultScoreWriter::TheShugoEmperorsVaultScoreWriter(model::instance::instancescore::NormalScore& reward) : InstanceScoreWriter(reward) {
}

TheShugoEmperorsVaultScoreWriter::~TheShugoEmperorsVaultScoreWriter() = default;

runtime::Ref<TheShugoEmperorsVaultScoreWriter> TheShugoEmperorsVaultScoreWriter::create(model::instance::instancescore::NormalScore& reward) {
	return runtime::makeRef<TheShugoEmperorsVaultScoreWriter>(reward);
}

model::instance::instancescore::NormalScore& TheShugoEmperorsVaultScoreWriter::score() const {
	return static_cast<model::instance::instancescore::NormalScore&>(*instanceScore); // constructed from a NormalScore
}

// Java TheShugoEmperorsVaultScoreWriter.java:17-32
void TheShugoEmperorsVaultScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	writeD(buf, score().getPoints());
	writeD(buf, score().getBasicAp());
	writeD(buf, 0);
	writeD(buf, score().getRank());
	writeD(buf, score().getFinalAp());
	writeD(buf, 0);
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
