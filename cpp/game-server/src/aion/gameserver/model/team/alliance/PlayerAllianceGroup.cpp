#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::team::alliance {

PlayerAllianceGroup::PlayerAllianceGroup(PlayerAlliance& allianceValue, int32_t objId) : TemporaryPlayerTeam(objId, false), alliance(allianceValue) {
}

PlayerAllianceGroup::~PlayerAllianceGroup() = default;

runtime::Ref<PlayerAllianceGroup> PlayerAllianceGroup::create(PlayerAlliance& allianceValue, int32_t objId) {
	return runtime::makeRef<PlayerAllianceGroup>(allianceValue, objId);
}

void PlayerAllianceGroup::addMember(TeamMember& member) {
	AION_UNPORTED();
}

void PlayerAllianceGroup::onRemoveMember(TeamMember& member) {
	AION_UNPORTED();
}

int32_t PlayerAllianceGroup::getMaxMemberCount() {
	AION_UNPORTED();
}

int32_t PlayerAllianceGroup::getMinExpPlayerLevel() {
	AION_UNPORTED();
}

int32_t PlayerAllianceGroup::getMaxExpPlayerLevel() {
	AION_UNPORTED();
}

runtime::Ptr<common::legacy::LootGroupRules> PlayerAllianceGroup::getLootGroupRules() {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::getMember(int32_t value) {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::removeMember(TeamMember& member) {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::removeMember(int32_t value) {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceGroup::getLeader() const {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::alliance
