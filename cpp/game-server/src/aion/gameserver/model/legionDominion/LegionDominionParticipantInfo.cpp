#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::legionDominion {

LegionDominionParticipantInfo::LegionDominionParticipantInfo() {
}

runtime::Ref<LegionDominionParticipantInfo> LegionDominionParticipantInfo::create() {
	return runtime::makeRef<LegionDominionParticipantInfo>();
}

int64_t LegionDominionParticipantInfo::getDate() {
	AION_UNPORTED();
}

std::string LegionDominionParticipantInfo::getLegionName() {
	AION_UNPORTED();
}

LegionDominionParticipantInfo::~LegionDominionParticipantInfo() = default;

} // namespace aion::gameserver::model::legionDominion
