#include "aion/gameserver/model/legionDominion/LegionDominionParticipantInfo.h"

#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/LegionService.h"

namespace aion::gameserver::model::legionDominion {

LegionDominionParticipantInfo::LegionDominionParticipantInfo() {
}

runtime::Ref<LegionDominionParticipantInfo> LegionDominionParticipantInfo::create() {
	return runtime::makeRef<LegionDominionParticipantInfo>();
}

int64_t LegionDominionParticipantInfo::getDate() {
	std::optional<commons::database::Timestamp> value = date.get();
	return value ? value->time_since_epoch().count() / 1000 : 0;
}

std::string LegionDominionParticipantInfo::getLegionName() {
	runtime::Ptr<team::legion::Legion> legion = services::LegionService::getInstance().getLegion(legionId.get());
	if (legion)
		return legion->getName();
	return "NOT AVAILABLE";
}

LegionDominionParticipantInfo::~LegionDominionParticipantInfo() = default;

} // namespace aion::gameserver::model::legionDominion
