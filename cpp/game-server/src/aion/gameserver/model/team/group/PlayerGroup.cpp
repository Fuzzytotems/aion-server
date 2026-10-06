#include "aion/gameserver/model/team/group/PlayerGroup.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/model/team/group/PlayerGroupStats.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::team::group {

PlayerGroup::PlayerGroup(PlayerGroupMember& leader, TeamType typeValue, int32_t id)
	: TemporaryPlayerTeam(id == 0 ? utils::idfactory::IDFactory::getInstance().nextId() : id, id == 0),
	  playerGroupStats(std::make_unique<PlayerGroupStats>(*this)), type(typeValue) {
	setLeader(leader);
}

PlayerGroup::~PlayerGroup() = default;

runtime::Ref<PlayerGroup> PlayerGroup::create(PlayerGroupMember& leader, TeamType typeValue, int32_t id) {
	return runtime::makeRef<PlayerGroup>(leader, typeValue, id);
}

void PlayerGroup::addMember(TeamMember& member) {
	PlayerGroupMember& groupMember = *runtime::cast<PlayerGroupMember>(member);
	TemporaryPlayerTeam::addMember(groupMember);
	playerGroupStats->onAddPlayer(groupMember);
	groupMember.getPlayer().setPlayerGroup(runtime::Ptr<PlayerGroup>(*this));
}

void PlayerGroup::onRemoveMember(TeamMember& member) {
	PlayerGroupMember& groupMember = *runtime::cast<PlayerGroupMember>(member);
	playerGroupStats->onRemovePlayer(groupMember);
	groupMember.getPlayer().setPlayerGroup(nullptr);
	// C++ cycle breaker (m5g-plan.md D7, docs/deviations/P5-10b.md): the last member left, so the stats drop the Players Java keeps referenced
	if (isDisbanded())
		playerGroupStats->releasePlayers();
}

int32_t PlayerGroup::getMaxMemberCount() {
	return 6;
}

int32_t PlayerGroup::getMinExpPlayerLevel() {
	return playerGroupStats->getMinExpPlayerLevel();
}

int32_t PlayerGroup::getMaxExpPlayerLevel() {
	return playerGroupStats->getMaxExpPlayerLevel();
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::getMember(int32_t value) {
	return runtime::cast<PlayerGroupMember>(TemporaryPlayerTeam::getMember(value));
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::removeMember(TeamMember& member) {
	return runtime::cast<PlayerGroupMember>(TemporaryPlayerTeam::removeMember(member));
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::removeMember(int32_t value) {
	return runtime::cast<PlayerGroupMember>(TemporaryPlayerTeam::removeMember(value));
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::getLeader() const {
	return runtime::cast<PlayerGroupMember>(TemporaryPlayerTeam::getLeader());
}

} // namespace aion::gameserver::model::team::group
