#include "aion/gameserver/network/aion/instanceinfo/DarkPoetaScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/instancescore/DarkPoetaScore.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java DarkPoetaScoreWriter.java:12-14
DarkPoetaScoreWriter::DarkPoetaScoreWriter(model::instance::instancescore::DarkPoetaScore& reward) : InstanceScoreWriter(reward) {
}

DarkPoetaScoreWriter::~DarkPoetaScoreWriter() = default;

runtime::Ref<DarkPoetaScoreWriter> DarkPoetaScoreWriter::create(model::instance::instancescore::DarkPoetaScore& reward) {
	return runtime::makeRef<DarkPoetaScoreWriter>(reward);
}

model::instance::instancescore::DarkPoetaScore& DarkPoetaScoreWriter::score() const {
	return static_cast<model::instance::instancescore::DarkPoetaScore&>(*instanceScore); // constructed from a DarkPoetaScore
}

// Java DarkPoetaScoreWriter.java:17-22
void DarkPoetaScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	writeD(buf, score().getPoints());
	writeD(buf, score().getNpcKills());
	writeD(buf, score().getGatherCollections()); // gathers
	writeD(buf, score().getRank()); // 7 for none, 8 for F, 5 for D, 4 C, 3 B, 2 A, 1 S
}

} // namespace aion::gameserver::network::aion::instanceinfo
