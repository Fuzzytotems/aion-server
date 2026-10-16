#include "aion/gameserver/model/autogroup/AutoPvpInstance.h"

#include <algorithm>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::autogroup {

using gameobjects::player::Player;

runtime::Ref<AutoPvpInstance> AutoPvpInstance::create(AutoGroupType agt) {
	return runtime::makeRef<AutoPvpInstance>(agt);
}

AutoPvpInstance::AutoPvpInstance(AutoGroupType value) : AutoInstance(value) {
}

AutoPvpInstance::~AutoPvpInstance() = default;

// Java AutoPvpInstance.java:24-35 (synchronized)
AGQuestion AutoPvpInstance::addLookingForParty(LookingForParty& lookingForParty) {
	SYNCHRONIZED(*this) {
		if (isRegistrationDisabled(lookingForParty) || static_cast<int32_t>(registeredAGPlayers.size()) >= getMaxPlayers())
			return AGQuestion::FAILED;
		std::vector<runtime::Ptr<AGPlayer>> playersByRace = getAGPlayersByRace(lookingForParty.getRace());
		if (static_cast<int32_t>(lookingForParty.getMembers().size() + playersByRace.size()) > getMaxPlayers(lookingForParty.getRace()))
			return AGQuestion::FAILED;
		registeredAGPlayers.putAll(lookingForParty.getMembers());
		return instance.get() == nullptr && static_cast<int32_t>(registeredAGPlayers.size()) == getMaxPlayers() ? AGQuestion::READY : AGQuestion::ADDED;
	}
}

// Java AutoPvpInstance.java:37-60
void AutoPvpInstance::onEnterInstance(Player& player) {
	AutoInstance::onEnterInstance(player);
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	if (mapInstance == nullptr) // Java: instance.register(...) on a null instance
		throw runtime::NullPointerException("AutoPvpInstance.instance is null");
	std::vector<runtime::Ptr<Player>> playersByRace = getPlayersByRace(player.getRace());
	std::erase(playersByRace, runtime::Ptr<Player>(player)); // Java: playersByRace.remove(player) - the one entry of the player
	if (playersByRace.empty()) {
		runtime::Ptr<team::TemporaryPlayerTeam> team;
		if (getMaxPlayers(player.getRace()) <= 6)
			team = team::group::PlayerGroupService::createGroup(player, player, team::TeamType::AUTO_GROUP, 0);
		else
			team = team::alliance::PlayerAllianceService::createAlliance(player, player, team::TeamType::AUTO_ALLIANCE);
		if (team == nullptr) // Java: team.getObjectId() of a refused creation
			throw runtime::NullPointerException("the auto team of " + player.getName() + " was not created");
		int32_t teamId = team->getObjectId();
		if (!mapInstance->isRegistered(teamId))
			mapInstance->register_(teamId);
	} else {
		if (playersByRace.front()->isInGroup())
			team::group::PlayerGroupService::addPlayer(*playersByRace.front()->getPlayerGroup(), player);
		else {
			runtime::Ptr<team::alliance::PlayerAlliance> alliance = playersByRace.front()->getPlayerAlliance();
			if (alliance == nullptr) // Java: PlayerAllianceService.addPlayer(null, player)
				throw runtime::NullPointerException("the first player of " + player.getName() + "'s race inside is in no team");
			team::alliance::PlayerAllianceService::addPlayer(*alliance, player);
		}
	}
	int32_t objectId = player.getObjectId();
	if (!mapInstance->isRegistered(objectId))
		mapInstance->register_(objectId);
}

// Java AutoPvpInstance.java:62-66
void AutoPvpInstance::onPressEnter(Player& player) {
	AutoInstance::onPressEnter(player);
	instance.get()->getInstanceHandler()->portToStartPosition(player);
}

// Java AutoPvpInstance.java:68-75
void AutoPvpInstance::onLeaveInstance(Player& player) {
	AutoInstance::unregister(player);
	if (player.isInGroup())
		team::group::PlayerGroupService::removePlayer(player);
	else if (player.isInAlliance())
		team::alliance::PlayerAllianceService::removePlayer(player);
}

// Java AutoPvpInstance.java:77-79
int32_t AutoPvpInstance::getMaxPlayers(Race race) {
	const AutoGroup* found = getTemplate(agt);
	if (found == nullptr) // Java: agt.getTemplate().getInstanceMapId() of a missing template
		throw runtime::NullPointerException("AutoGroupType without auto group template");
	return dataholders::DataManager::INSTANCE_COOLTIME_DATA->getMaxMemberCount(found->getInstanceMapId(), race);
}

// Java AutoPvpInstance.java:81-84
int32_t AutoPvpInstance::getMaxPlayers() {
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	return mapInstance == nullptr ? getMaxPlayers(Race::ASMODIANS) + getMaxPlayers(Race::ELYOS) : mapInstance->getMaxPlayers();
}

} // namespace aion::gameserver::model::autogroup
