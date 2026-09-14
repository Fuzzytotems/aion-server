#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"

namespace aion::gameserver::network::aion::instanceinfo {

InstanceScoreWriter::InstanceScoreWriter(model::instance::instancescore::InstanceScore& value)
	: instanceScore(runtime::Ref<model::instance::instancescore::InstanceScore>(value)) {
}

void InstanceScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	AION_UNPORTED();
}

InstanceScoreWriter::~InstanceScoreWriter() = default;

} // namespace aion::gameserver::network::aion::instanceinfo
