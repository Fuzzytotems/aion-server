#include "aion/gameserver/model/team/league/LeagueMember.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"

namespace aion::gameserver::model::team::league {

LeagueMember::LeagueMember(alliance::PlayerAlliance& allianceValue, int32_t position) : alliance(allianceValue), leaguePosition(position) {
}

LeagueMember::~LeagueMember() = default;

runtime::Ref<LeagueMember> LeagueMember::create(alliance::PlayerAlliance& allianceValue, int32_t position) {
	return runtime::makeRef<LeagueMember>(allianceValue, position);
}

int32_t LeagueMember::getObjectId() {
	return alliance->getObjectId();
}

std::string LeagueMember::getName() {
	return alliance->getName();
}

runtime::Ptr<gameobjects::AionObject> LeagueMember::getObject() {
	return runtime::Ptr<gameobjects::AionObject>(alliance);
}

} // namespace aion::gameserver::model::team::league
