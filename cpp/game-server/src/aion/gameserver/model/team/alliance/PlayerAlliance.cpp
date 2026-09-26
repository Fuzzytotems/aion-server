#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/league/League.h"

namespace aion::gameserver::model::team::alliance {

PlayerAlliance::PlayerAlliance(PlayerAllianceMember& value, TeamType typeValue)
	: TemporaryPlayerTeam(int32_t{}, bool{}), type(typeValue) {
	// Java: super(IDFactory.getInstance().nextId(), true); setLeader(leader); for (int groupId = 1000; groupId <= 1003; groupId++) {
	// groups.put(groupId, new PlayerAllianceGroup(this, groupId)); }; super(...) arguments
	AION_UNPORTED();
}

runtime::Ref<PlayerAlliance> PlayerAlliance::create(PlayerAllianceMember& value, TeamType typeValue) {
	return runtime::makeRef<PlayerAlliance>(value, typeValue);
}

void PlayerAlliance::addMember(TeamMember& member) {
	AION_UNPORTED();
}

void PlayerAlliance::onRemoveMember(TeamMember& member) {
	AION_UNPORTED();
}

int32_t PlayerAlliance::getMaxMemberCount() {
	AION_UNPORTED();
}

int32_t PlayerAlliance::getMinExpPlayerLevel() {
	AION_UNPORTED();
}

int32_t PlayerAlliance::getMaxExpPlayerLevel() {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceGroup> PlayerAlliance::getOpenAllianceGroup() {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceGroup> PlayerAlliance::getAllianceGroup(std::optional<int32_t> allianceGroupId) {
	AION_UNPORTED();
}

bool PlayerAlliance::isViceCaptain(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerAlliance::isSomeCaptain(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerAlliance::setLeague(runtime::Ptr<team::league::League> value) {
	this->league.set(value);
}

bool PlayerAlliance::isInLeague() {
	AION_UNPORTED();
}

int32_t PlayerAlliance::groupSize() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<PlayerAllianceGroup>> PlayerAlliance::getGroups() {
	AION_UNPORTED();
}

runtime::Ptr<common::legacy::LootGroupRules> PlayerAlliance::getLootGroupRules() {
	AION_UNPORTED();
}

PlayerAlliance::~PlayerAlliance() = default;

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::getMember(int32_t value) {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::removeMember(TeamMember& member) {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::removeMember(int32_t value) {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::getLeader() const {
	// port: runtime::cast<PlayerAllianceMember> of the TemporaryPlayerTeam result, once model/team/alliance/PlayerAllianceMember.h exists
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::alliance
