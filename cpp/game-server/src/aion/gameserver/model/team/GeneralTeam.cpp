#include "aion/gameserver/model/team/GeneralTeam.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::model::team {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.team.GeneralTeam");

std::string TeamEvent::toString() {
	return utils::simpleClassName(typeid(*this));
}

GeneralTeam::GeneralTeam(int32_t objId, bool autoReleaseObjectId) : AionObject(objId, autoReleaseObjectId) {
}

GeneralTeam::~GeneralTeam() = default;

void GeneralTeam::onEvent(TeamEvent& event) {
	lock();
	auto unlocker = runtime::finally([this] { unlock(); });
	if (event.checkCondition()) {
		event.handleEvent();
	} else {
		log.warn("[TEAM] skipped event: {} group: {}", event.toString(), toString());
	}
}

runtime::Ptr<TeamMember> GeneralTeam::getMember(int32_t objectIdValue) {
	return members.get(objectIdValue);
}

bool GeneralTeam::hasMember(int32_t objectIdValue) {
	return static_cast<bool>(members.get(objectIdValue));
}

void GeneralTeam::addMember(TeamMember& member) {
	// Java: Objects.requireNonNull(member, "Team member should be not null") - a reference is never null
	if (members.put(member.getObjectId(), runtime::Ref<TeamMember>(member)))
		throw commons::utils::IllegalStateException("Team member is already added");
}

runtime::Ptr<TeamMember> GeneralTeam::removeMember(TeamMember& member) {
	// Java: Objects.requireNonNull(member, "Team member should be not null") - a reference is never null
	return removeMember(member.getObjectId());
}

runtime::Ptr<TeamMember> GeneralTeam::removeMember(int32_t objectIdValue) {
	runtime::Ptr<TeamMember> removedMember = members.remove(objectIdValue);
	if (!removedMember)
		throw commons::utils::IllegalStateException("Team member is already removed");
	{
		// C++ cycle breaker (cycles.toml GeneralTeam.leader, DEVIATION 10; docs/deviations/P5-10a.md): the team keeps no member once the last one
		// left, so a holder that outlives it (a corpse's DropNpc.lootingTeam, WorldMapInstance.registeredTeam) keeps no Player. Java keeps the last
		// leader. It runs before onRemoveMember, so the subclass sees the team empty and releases its own references (PlayerGroupStats, D7).
		if (members.isEmpty())
			leader.set(nullptr);
	}
	onRemoveMember(*removedMember);
	return removedMember;
}

void GeneralTeam::forEachTeamMember(const std::function<void(TeamMember&)>& consumer) {
	lock();
	auto unlocker = runtime::finally([this] { unlock(); });
	for (const runtime::Ptr<TeamMember>& member : members.values()) {
		consumer(*member);
	}
}

void GeneralTeam::forEach(const std::function<void(gameobjects::AionObject&)>& consumer) {
	lock();
	auto unlocker = runtime::finally([this] { unlock(); });
	for (const runtime::Ptr<TeamMember>& member : members.values())
		consumer(*member->getObject());
}

void GeneralTeam::applyOnMembers(const std::function<bool(gameobjects::AionObject&)>& function) {
	lock();
	auto unlocker = runtime::finally([this] { unlock(); });
	for (const runtime::Ptr<TeamMember>& member : members.values()) {
		if (!function(*member->getObject())) {
			return;
		}
	}
}

std::vector<runtime::Ptr<TeamMember>> GeneralTeam::filter(const std::function<bool(TeamMember&)>& predicate) {
	std::vector<runtime::Ptr<TeamMember>> result;
	for (const runtime::Ptr<TeamMember>& member : members.values()) {
		if (predicate(*member))
			result.push_back(member);
	}
	return result;
}

std::vector<runtime::Ptr<gameobjects::AionObject>> GeneralTeam::filterMembers(const std::function<bool(gameobjects::AionObject&)>& predicate) {
	std::vector<runtime::Ptr<gameobjects::AionObject>> result;
	for (const runtime::Ptr<TeamMember>& member : members.values()) {
		runtime::Ptr<gameobjects::AionObject> object = member->getObject();
		if (predicate(*object))
			result.push_back(std::move(object));
	}
	return result;
}

std::vector<runtime::Ptr<gameobjects::AionObject>> GeneralTeam::getMembers() {
	std::vector<runtime::Ptr<gameobjects::AionObject>> result;
	for (const runtime::Ptr<TeamMember>& member : members.values())
		result.push_back(member->getObject());
	return result;
}

int32_t GeneralTeam::size() {
	return members.size();
}

bool GeneralTeam::isDisbanded() {
	return size() == 0;
}

bool GeneralTeam::shouldDisband() {
	return size() == 1; // teams always contain at least two members
}

bool GeneralTeam::isFull() {
	return size() == getMaxMemberCount();
}

int32_t GeneralTeam::getTeamId() {
	return getObjectId();
}

std::string GeneralTeam::getName() {
	runtime::Ptr<TeamMember> current = leader.get();
	// C++: the leader is null only after the last-leave breaker (removeMember), where Java still prints the last leader; a late event on a disbanded
	// team is logged with "Leader: null" instead of throwing inside the warning (m5g-plan.md risk 12, docs/deviations/P5-10a.md)
	if (!current)
		return "Leader: null";
	return "Leader: " + current->getObject()->toString();
}

runtime::Ptr<gameobjects::AionObject> GeneralTeam::getLeaderObject() {
	runtime::Ptr<TeamMember> current = leader.get();
	if (!current)
		throw runtime::NullPointerException("GeneralTeam.leader");
	return current->getObject();
}

bool GeneralTeam::isLeader(gameobjects::AionObject& member) {
	return getLeaderObject()->equals(member);
}

void GeneralTeam::changeLeader(TeamMember& member) {
	runtime::Ptr<TeamMember> current = leader.get();
	if (!current)
		throw runtime::NullPointerException("Leader should already be set");
	// Java: Objects.requireNonNull(member, "New leader should not be null") - a reference is never null
	if (current.get() == &member) // Java: leader.equals(member) - TeamMember does not override equals (identity)
		throw commons::utils::IllegalArgumentException(member.getName() + " is already the team leader");
	leader.set(runtime::Ptr<TeamMember>(member));
}

void GeneralTeam::setLeader(TeamMember& member) {
	if (leader.get())
		throw commons::utils::IllegalStateException("Leader should be not initialized");
	// Java: Objects.requireNonNull(member, "Leader should not be null") - a reference is never null
	leader.set(runtime::Ptr<TeamMember>(member));
}

void GeneralTeam::lock() {
	teamLock.lock();
}

void GeneralTeam::unlock() {
	teamLock.unlock();
}

} // namespace aion::gameserver::model::team
