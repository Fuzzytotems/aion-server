#include "aion/gameserver/model/team/group/PlayerGroup.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/group/PlayerGroupStats.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::team::group {

PlayerGroup::PlayerGroup(PlayerGroupMember& leader, TeamType typeValue, int32_t id)
	: TemporaryPlayerTeam(id == 0 ? utils::idfactory::IDFactory::getInstance().nextId() : id, id == 0),
	  playerGroupStats(std::make_unique<PlayerGroupStats>(*this)), type(typeValue) {
	// Java: setLeader(leader)
	static_cast<void>(leader);
	AION_UNPORTED();
}

PlayerGroup::~PlayerGroup() = default;

runtime::Ref<PlayerGroup> PlayerGroup::create(PlayerGroupMember& leader, TeamType typeValue, int32_t id) {
	return runtime::makeRef<PlayerGroup>(leader, typeValue, id);
}

void PlayerGroup::addMember(TeamMember& member) {
	AION_UNPORTED();
}

void PlayerGroup::onRemoveMember(TeamMember& member) {
	AION_UNPORTED();
}

int32_t PlayerGroup::getMaxMemberCount() {
	AION_UNPORTED();
}

int32_t PlayerGroup::getMinExpPlayerLevel() {
	AION_UNPORTED();
}

int32_t PlayerGroup::getMaxExpPlayerLevel() {
	AION_UNPORTED();
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::getMember(int32_t value) {
	// port: runtime::cast<PlayerGroupMember> of the TemporaryPlayerTeam result, once model/team/group/PlayerGroupMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::removeMember(TeamMember& member) {
	// port: runtime::cast<PlayerGroupMember> of the TemporaryPlayerTeam result, once model/team/group/PlayerGroupMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::removeMember(int32_t value) {
	// port: runtime::cast<PlayerGroupMember> of the TemporaryPlayerTeam result, once model/team/group/PlayerGroupMember.h exists
	AION_UNPORTED();
}

runtime::Ptr<PlayerGroupMember> PlayerGroup::getLeader() const {
	// port: runtime::cast<PlayerGroupMember> of the TemporaryPlayerTeam result, once model/team/group/PlayerGroupMember.h exists
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::group
