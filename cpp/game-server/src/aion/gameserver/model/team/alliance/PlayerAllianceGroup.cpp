#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"

namespace aion::gameserver::model::team::alliance {

PlayerAllianceGroup::PlayerAllianceGroup(PlayerAlliance& allianceValue, int32_t objId) : TemporaryPlayerTeam(objId, false, teamlock::OfPlayerAllianceGroup{}), alliance(allianceValue) {
}

PlayerAllianceGroup::~PlayerAllianceGroup() = default;

runtime::Ref<PlayerAllianceGroup> PlayerAllianceGroup::create(PlayerAlliance& allianceValue, int32_t objId) {
	return runtime::makeRef<PlayerAllianceGroup>(allianceValue, objId);
}

void PlayerAllianceGroup::addMember(TeamMember& member) {
	PlayerAllianceMember& allianceMember = *runtime::cast<PlayerAllianceMember>(member);
	TemporaryPlayerTeam::addMember(allianceMember);
	allianceMember.setPlayerAllianceGroup(runtime::Ptr<PlayerAllianceGroup>(*this));
	allianceMember.setAllianceId(getTeamId());
}

void PlayerAllianceGroup::onRemoveMember(TeamMember& member) {
	runtime::cast<PlayerAllianceMember>(member)->setPlayerAllianceGroup(nullptr);
}

int32_t PlayerAllianceGroup::getMaxMemberCount() {
	return 6;
}

int32_t PlayerAllianceGroup::getMinExpPlayerLevel() {
	return 0;
}

int32_t PlayerAllianceGroup::getMaxExpPlayerLevel() {
	return 0;
}

runtime::Ptr<common::legacy::LootGroupRules> PlayerAllianceGroup::getLootGroupRules() {
	return alliance->getLootGroupRules();
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::getMember(int32_t value) {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::getMember(value));
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::removeMember(TeamMember& member) {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::removeMember(member));
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::removeMember(int32_t value) {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::removeMember(value));
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::getLeader() const {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::getLeader());
}

} // namespace aion::gameserver::model::team::alliance
