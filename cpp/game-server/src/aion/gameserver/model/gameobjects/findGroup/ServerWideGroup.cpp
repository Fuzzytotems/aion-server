#include "aion/gameserver/model/gameobjects/findGroup/ServerWideGroup.h"

#include <algorithm>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::findGroup {

ServerWideGroup::ServerWideGroup(player::Player& recruiter, int32_t value, int32_t minMembersValue, std::string_view messageValue)
	: instanceMaskId(value), minMembers(minMembersValue), message(std::string(messageValue)) {
	members.add(runtime::Ref<player::Player>(recruiter));
	setLastUpdate();
}

runtime::Ref<ServerWideGroup> ServerWideGroup::create(player::Player& recruiter, int32_t value, int32_t minMembersValue,
	std::string_view messageValue) {
	return runtime::makeRef<ServerWideGroup>(recruiter, value, minMembersValue, messageValue);
}

std::vector<runtime::Ptr<player::Player>> ServerWideGroup::getMembers() {
	// custom: use regular teams for server-wide instance group recruitment (on official servers the recruiter must not be in a team)
	runtime::Ptr<team::TemporaryPlayerTeam> team = getRecruiter()->getCurrentTeam();
	if (!team)
		return members.snapshot();
	std::vector<runtime::Ptr<player::Player>> teamMembers;
	for (runtime::Ptr<AionObject> member : team->getMembers())
		teamMembers.push_back(runtime::cast<player::Player>(member));
	return teamMembers;
}

void ServerWideGroup::setLastUpdate() {
	lastUpdate.set(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000));
}

runtime::Ptr<player::Player> ServerWideGroup::getRecruiter() {
	return members.get(0);
}

int32_t ServerWideGroup::getId() {
	return getRecruiter()->getObjectId();
}

Race ServerWideGroup::getRace() {
	return getRecruiter()->getRace();
}

int32_t ServerWideGroup::getMinLevel() {
	// Java: members.stream().max(Comparator.comparing(Player::getLevel)).map(Player::getLevel).get(): the highest level despite the name
	std::vector<runtime::Ptr<player::Player>> current = members.snapshot();
	if (current.empty())
		throw runtime::NoSuchElementException("No value present");
	int32_t result = current.front()->getLevel();
	for (runtime::Ptr<player::Player> member : current)
		result = std::max<int32_t>(result, member->getLevel());
	return result;
}

int32_t ServerWideGroup::getMaxLevel() {
	// Java: max with the reversed level comparator: the lowest level despite the name
	std::vector<runtime::Ptr<player::Player>> current = members.snapshot();
	if (current.empty())
		throw runtime::NoSuchElementException("No value present");
	int32_t result = current.front()->getLevel();
	for (runtime::Ptr<player::Player> member : current)
		result = std::min<int32_t>(result, member->getLevel());
	return result;
}

ServerWideGroup::~ServerWideGroup() = default;

} // namespace aion::gameserver::model::gameobjects::findGroup
