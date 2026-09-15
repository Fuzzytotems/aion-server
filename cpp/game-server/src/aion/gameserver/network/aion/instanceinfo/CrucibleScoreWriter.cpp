#include "aion/gameserver/network/aion/instanceinfo/CrucibleScoreWriter.h"

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::instanceinfo {

CrucibleScoreWriter::CrucibleScoreWriter(model::instance::instancescore::InstanceScore& reward) : InstanceScoreWriter(reward) {}

CrucibleScoreWriter::~CrucibleScoreWriter() = default;

runtime::Ref<CrucibleScoreWriter> CrucibleScoreWriter::create(model::instance::instancescore::InstanceScore& reward) {
	return runtime::makeRef<CrucibleScoreWriter>(reward);
}

void CrucibleScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::instanceinfo
