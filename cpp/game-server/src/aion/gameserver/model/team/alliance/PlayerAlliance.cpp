#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::team::alliance {

using gameobjects::player::Player;

PlayerAlliance::PlayerAlliance(PlayerAllianceMember& leader, TeamType typeValue)
	: TemporaryPlayerTeam(utils::idfactory::IDFactory::getInstance().nextId(), true, teamlock::OfPlayerAlliance{}), type(typeValue) {
	setLeader(leader);
	for (int32_t groupId = 1000; groupId <= 1003; groupId++) {
		groups.put(groupId, PlayerAllianceGroup::create(*this, groupId));
	}
}

runtime::Ref<PlayerAlliance> PlayerAlliance::create(PlayerAllianceMember& value, TeamType typeValue) {
	return runtime::makeRef<PlayerAlliance>(value, typeValue);
}

void PlayerAlliance::addMember(TeamMember& member) {
	PlayerAllianceMember& allianceMember = *runtime::cast<PlayerAllianceMember>(member);
	TemporaryPlayerTeam::addMember(allianceMember);
	runtime::Ptr<PlayerAllianceGroup> openAllianceGroup = getOpenAllianceGroup();
	openAllianceGroup->addMember(allianceMember);
}

void PlayerAlliance::onRemoveMember(TeamMember& member) {
	PlayerAllianceMember& allianceMember = *runtime::cast<PlayerAllianceMember>(member);
	runtime::Ptr<PlayerAllianceGroup> allianceGroup = allianceMember.getPlayerAllianceGroup();
	if (!allianceGroup)
		throw runtime::NullPointerException("PlayerAllianceMember.getPlayerAllianceGroup()");
	allianceGroup->removeMember(allianceMember);
}

int32_t PlayerAlliance::getMaxMemberCount() {
	return 24;
}

int32_t PlayerAlliance::getMinExpPlayerLevel() {
	int32_t minLvl = 99;
	for (const runtime::Ptr<gameobjects::AionObject>& object : getMembers()) {
		Player& member = *runtime::cast<Player>(*object);
		if (member.getLevel() < minLvl) {
			minLvl = member.getLevel();
		}
	}
	return minLvl;
}

int32_t PlayerAlliance::getMaxExpPlayerLevel() {
	int32_t maxLvl = 1;
	for (const runtime::Ptr<gameobjects::AionObject>& object : getMembers()) {
		Player& member = *runtime::cast<Player>(*object);
		if (member.getLevel() > maxLvl) {
			maxLvl = member.getLevel();
		}
	}
	return maxLvl;
}

runtime::Ptr<PlayerAllianceGroup> PlayerAlliance::getOpenAllianceGroup() {
	lock();
	{
		auto unlocker = runtime::finally([this] { unlock(); });
		for (int32_t groupId = 1000; groupId <= 1003; groupId++) {
			runtime::Ptr<PlayerAllianceGroup> playerAllianceGroup = groups.get(groupId);
			if (!playerAllianceGroup)
				throw runtime::NullPointerException("PlayerAlliance.groups.get(" + std::to_string(groupId) + ")");
			if (!playerAllianceGroup->isFull()) {
				return playerAllianceGroup;
			}
		}
	}
	throw commons::utils::IllegalStateException("All alliance groups are full.");
}

runtime::Ptr<PlayerAllianceGroup> PlayerAlliance::getAllianceGroup(std::optional<int32_t> allianceGroupId) {
	// Java: groups.get(Integer) - a null key finds nothing (HashMap.get(null) of a map without a null key)
	runtime::Ptr<PlayerAllianceGroup> allianceGroup = allianceGroupId ? groups.get(*allianceGroupId) : nullptr;
	if (!allianceGroup)
		throw runtime::NullPointerException("No such alliance group " + (allianceGroupId ? std::to_string(*allianceGroupId) : std::string("null")));
	return allianceGroup;
}

bool PlayerAlliance::isViceCaptain(Player& player) {
	return viceCaptainIds.contains(player.getObjectId());
}

bool PlayerAlliance::isSomeCaptain(Player& player) {
	return isLeader(player) || isViceCaptain(player);
}

void PlayerAlliance::setLeague(runtime::Ptr<team::league::League> value) {
	this->league.set(value);
}

bool PlayerAlliance::isInLeague() {
	return static_cast<bool>(this->league.get());
}

int32_t PlayerAlliance::groupSize() {
	return groups.size();
}

std::vector<runtime::Ptr<PlayerAllianceGroup>> PlayerAlliance::getGroups() {
	std::vector<runtime::Ptr<PlayerAllianceGroup>> result;
	for (const runtime::Ptr<PlayerAllianceGroup>& group : groups.values())
		result.push_back(group);
	return result;
}

runtime::Ptr<common::legacy::LootGroupRules> PlayerAlliance::getLootGroupRules() {
	runtime::Ptr<team::league::League> current = league.get();
	return !current ? TemporaryPlayerTeam::getLootGroupRules() : current->getLootGroupRules();
}

void PlayerAlliance::releaseGroups() {
	groups.clear();
}

PlayerAlliance::~PlayerAlliance() = default;

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::getMember(int32_t value) {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::getMember(value));
}

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::removeMember(TeamMember& member) {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::removeMember(member));
}

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::removeMember(int32_t value) {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::removeMember(value));
}

runtime::Ptr<PlayerAllianceMember> PlayerAlliance::getLeader() const {
	return runtime::cast<PlayerAllianceMember>(TemporaryPlayerTeam::getLeader());
}

} // namespace aion::gameserver::model::team::alliance
