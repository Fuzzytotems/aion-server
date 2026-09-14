#include "aion/gameserver/model/gameobjects/findGroup/ServerWideGroup.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::gameobjects::findGroup {

ServerWideGroup::ServerWideGroup(player::Player& recruiter, int32_t value, int32_t minMembersValue, std::string_view messageValue)
	: instanceMaskId(value), minMembers(minMembersValue), message(std::string(messageValue)) {
	// Java: this.members.add(recruiter); setLastUpdate()
	AION_UNPORTED();
}

runtime::Ref<ServerWideGroup> ServerWideGroup::create(player::Player& recruiter, int32_t value, int32_t minMembersValue,
	std::string_view messageValue) {
	return runtime::makeRef<ServerWideGroup>(recruiter, value, minMembersValue, messageValue);
}

std::vector<runtime::Ptr<player::Player>> ServerWideGroup::getMembers() {
	AION_UNPORTED();
}

void ServerWideGroup::setLastUpdate() {
	AION_UNPORTED();
}

runtime::Ptr<player::Player> ServerWideGroup::getRecruiter() {
	AION_UNPORTED();
}

int32_t ServerWideGroup::getId() {
	AION_UNPORTED();
}

Race ServerWideGroup::getRace() {
	AION_UNPORTED();
}

int32_t ServerWideGroup::getMinLevel() {
	AION_UNPORTED();
}

int32_t ServerWideGroup::getMaxLevel() {
	AION_UNPORTED();
}

ServerWideGroup::~ServerWideGroup() = default;

} // namespace aion::gameserver::model::gameobjects::findGroup
