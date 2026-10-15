#include "aion/gameserver/model/team/league/League.h"

#include <algorithm>

#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/utils/collections/Predicates.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::team::league {

using alliance::PlayerAlliance;
using gameobjects::player::Player;

namespace {

/** Java: a league's members are PlayerAlliances (GeneralTeam<PlayerAlliance, LeagueMember>) */
PlayerAlliance& allianceOf(gameobjects::AionObject& object) {
	return *runtime::cast<PlayerAlliance>(object);
}

} // namespace

League::League(LeagueMember& leader)
	: GeneralTeam(utils::idfactory::IDFactory::getInstance().nextId(), true, teamlock::OfLeague{}),
	  lootGroupRules(common::legacy::LootGroupRules::create()) {
	setLeader(leader);
}

runtime::Ref<League> League::create(LeagueMember& value) {
	return runtime::makeRef<League>(value);
}

std::vector<runtime::Ptr<Player>> League::getOnlineMembers() {
	std::vector<runtime::Ptr<Player>> online;
	for (const runtime::Ptr<gameobjects::AionObject>& object : getMembers())
		for (const runtime::Ptr<Player>& player : allianceOf(*object).getOnlineMembers())
			online.push_back(player);
	return online;
}

void League::addMember(TeamMember& member) {
	LeagueMember& leagueMember = *runtime::cast<LeagueMember>(member);
	GeneralTeam::addMember(leagueMember);
	leagueMember.getAlliance().setLeague(runtime::Ptr<League>(*this));
}

void League::onRemoveMember(TeamMember& member) {
	runtime::cast<LeagueMember>(member)->getAlliance().setLeague(nullptr);
}

int32_t League::getMaxMemberCount() {
	return 8;
}

void League::sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	for (const runtime::Ptr<gameobjects::AionObject>& object : getMembers())
		allianceOf(*object).sendPackets(packets);
}

void League::sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
	std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) {
	for (const runtime::Ptr<gameobjects::AionObject>& object : getMembers()) {
		if (predicate(*object))
			allianceOf(*object).sendPackets(packets);
	}
}

Race League::getRace() {
	return getLeaderObject()->getRace();
}

runtime::Ptr<Player> League::getCaptain() {
	return getLeaderObject()->getLeaderObject();
}

void League::setLootGroupRules(runtime::Ptr<common::legacy::LootGroupRules> value) {
	this->lootGroupRules.set(value);
}

std::vector<runtime::Ptr<LeagueMember>> League::getSortedMembers() {
	std::vector<runtime::Ptr<LeagueMember>> memberList;
	for (const runtime::Ptr<TeamMember>& member : members.values())
		memberList.push_back(runtime::cast<LeagueMember>(member));
	// Java: List.sort(Comparator.comparing(LeagueMember::getLeaguePosition)) - a stable sort
	std::ranges::stable_sort(memberList, {}, [](const runtime::Ptr<LeagueMember>& member) { return member->getLeaguePosition(); });
	return memberList;
}

runtime::Ptr<Player> League::reorganize() {
	int32_t position = 0;
	runtime::Ptr<Player> newLeader = nullptr;
	for (const runtime::Ptr<LeagueMember>& alliance : getSortedMembers()) {
		if (alliance->getLeaguePosition() > position) {
			if (position == 0) {
				newLeader = alliance->getAlliance().getLeaderObject();
				changeLeader(*alliance);
			}
			alliance->setLeaguePosition(position);
		}
		position++;
	}
	return newLeader;
}

runtime::Ptr<Player> League::getPlayerMember(int32_t playerObjId) {
	for (const runtime::Ptr<gameobjects::AionObject>& object : getMembers()) {
		runtime::Ptr<alliance::PlayerAllianceMember> playerMember = allianceOf(*object).getMember(playerObjId);
		if (playerMember) {
			return runtime::Ptr<Player>(playerMember->getPlayer());
		}
	}
	return nullptr;
}

void League::broadcast() {
	broadcast(nullptr, nullptr);
}

void League::broadcast(Player& skippedPlayer) {
	broadcast(nullptr, runtime::Ptr<Player>(skippedPlayer));
}

void League::broadcast(PlayerAlliance& skippedAlliance) {
	broadcast(runtime::Ptr<PlayerAlliance>(skippedAlliance), nullptr);
}

void League::broadcast(runtime::Ptr<PlayerAlliance> skippedAlliance, runtime::Ptr<Player> skippedPlayer) {
	lock();
	auto unlocker = runtime::finally([this] { unlock(); });
	for (const runtime::Ptr<TeamMember>& member : members.values()) {
		PlayerAlliance& targetAlliance = runtime::cast<LeagueMember>(member)->getAlliance();
		// Java: !targetAlliance.equals(skippedAlliance) - AionObject.equals (object ids), false for null
		if (!skippedAlliance || !targetAlliance.equals(*skippedAlliance)) {
			network::aion::serverpackets::SM_ALLIANCE_INFO packet(targetAlliance, 0, "", skippedAlliance);
			if (skippedPlayer) {
				const auto allExcept = utils::collections::Predicates::Players::allExcept(*skippedPlayer);
				targetAlliance.sendPacket([&allExcept](gameobjects::AionObject& object) { return allExcept(*runtime::cast<Player>(object)); }, {packet});
			} else {
				targetAlliance.sendPacket([](gameobjects::AionObject&) { return true; }, {packet}); // Java: Predicates.alwaysTrue()
			}
		}
	}
}

std::vector<runtime::Ptr<Player>> League::getCaptains() {
	std::vector<runtime::Ptr<Player>> captains;
	for (const runtime::Ptr<LeagueMember>& member : getSortedMembers()) {
		runtime::Ptr<Player> allianceLeader = member->getAlliance().getLeaderObject(); // Java: leader
		// Java: List.contains - AionObject.equals (object ids)
		if (std::ranges::none_of(captains, [&allianceLeader](const runtime::Ptr<Player>& captain) { return captain->equals(*allianceLeader); })) {
			captains.push_back(allianceLeader);
		}
	}
	return captains;
}

League::~League() = default;

runtime::Ptr<LeagueMember> League::getMember(int32_t value) {
	return runtime::cast<LeagueMember>(GeneralTeam::getMember(value));
}

runtime::Ptr<LeagueMember> League::removeMember(TeamMember& member) {
	return runtime::cast<LeagueMember>(GeneralTeam::removeMember(member));
}

runtime::Ptr<LeagueMember> League::removeMember(int32_t value) {
	return runtime::cast<LeagueMember>(GeneralTeam::removeMember(value));
}

runtime::Ptr<LeagueMember> League::getLeader() const {
	return runtime::cast<LeagueMember>(GeneralTeam::getLeader());
}

runtime::Ptr<PlayerAlliance> League::getLeaderObject() {
	return runtime::cast<PlayerAlliance>(GeneralTeam::getLeaderObject());
}

} // namespace aion::gameserver::model::team::league
