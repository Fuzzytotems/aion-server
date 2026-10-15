#include "aion/gameserver/model/autogroup/AutoHarmonyInstance.h"

#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::autogroup {

using gameobjects::player::Player;

/**
 * Java AGPlayer is a record: List.remove(agp) and List.contains(agp) compare its four components. C++ compares them too (AGPlayer has no
 * equals: a player who registers again gets a new, equal AGPlayer).
 */
struct AutoHarmonyInstance_AGPlayerEquals {
	static bool equal(const AGPlayer& a, const AGPlayer& b) {
		return a.objectId() == b.objectId() && a.race() == b.race() && a.playerClass() == b.playerClass() && a.name() == b.name();
	}
};

runtime::Ref<AutoHarmonyInstance> AutoHarmonyInstance::create(AutoGroupType agt) {
	return runtime::makeRef<AutoHarmonyInstance>(agt);
}

// Java AutoHarmonyInstance.java:23-27
AutoHarmonyInstance::AutoHarmonyInstance(AutoGroupType value) : AutoInstance(value) {
	groups.put(0, Group::create(AION_LOCK_CLASS(AutoHarmonyInstance::groups)));
	groups.put(1, Group::create(AION_LOCK_CLASS(AutoHarmonyInstance::groups)));
}

AutoHarmonyInstance::~AutoHarmonyInstance() = default;

// Java AutoHarmonyInstance.java:29-34
void AutoHarmonyInstance::onInstanceCreate(world::WorldMapInstance& value) {
	AutoInstance::onInstanceCreate(value);
	runtime::Ptr<model::instance::instancescore::HarmonyArenaScore> score =
		runtime::cast<model::instance::instancescore::HarmonyArenaScore>(value.getInstanceHandler()->getInstanceScore()); // Java: (HarmonyArenaScore)
	score->setDifficultyId(getDifficultId(agt));
}

// Java AutoHarmonyInstance.java:36-45 (synchronized)
AGQuestion AutoHarmonyInstance::addLookingForParty(LookingForParty& lookingForParty) {
	SYNCHRONIZED(*this) {
		if (isRegistrationDisabled(lookingForParty) || static_cast<int32_t>(registeredAGPlayers.size()) >= getMaxPlayers())
			return AGQuestion::FAILED;
		AGQuestion question = canAddParty(*groups.get(0), lookingForParty);
		if (question == AGQuestion::FAILED)
			question = canAddParty(*groups.get(1), lookingForParty);
		return question;
	}
}

// Java AutoHarmonyInstance.java:47-60: the Arena of Harmony's ticket; without it the player leaves the registration (SM_AUTO_GROUP window 5)
// and an emptied instance is destroyed
void AutoHarmonyInstance::onPressEnter(Player& player) {
	if (isHarmonyArena(agt)) {
		if (!removeItem(player, 186000184, 1)) {
			registeredAGPlayers.remove(player.getObjectId());
			const AutoGroup* found = getTemplate(agt);
			if (found == nullptr) // Java: agt.getTemplate().getMaskId() of a missing template
				throw runtime::NullPointerException("AutoGroupType without auto group template");
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_AUTO_GROUP(found->getMaskId(), 5));
			if (registeredAGPlayers.isEmpty())
				services::AutoGroupService::getInstance().destroyIfPossible(*this);
			return;
		}
	}
	runtime::Ptr<world::WorldMapInstance> current = instance.get();
	runtime::cast<model::instance::instancescore::HarmonyArenaScore>(current->getInstanceHandler()->getInstanceScore())->portToPosition(player);
	current->register_(player.getObjectId());
}

// Java AutoHarmonyInstance.java:62-96: a player not in a group yet founds or joins his party's group, whose HarmonyGroupReward the arena's
// HarmonyArenaScore keeps
void AutoHarmonyInstance::onEnterInstance(Player& player) {
	AutoInstance::onEnterInstance(player);
	if (player.isInGroup()) {
		return;
	}
	int32_t playerId = player.getObjectId();
	std::optional<std::pair<int32_t, runtime::Ref<Group>>> groupEntry = getGroupEntry(playerId);
	if (!groupEntry)
		return;

	runtime::Ptr<world::WorldMapInstance> current = instance.get();
	runtime::Ptr<model::instance::instancescore::HarmonyArenaScore> score =
		runtime::cast<model::instance::instancescore::HarmonyArenaScore>(current->getInstanceHandler()->getInstanceScore()); // Java: (HarmonyArenaScore)
	std::vector<runtime::Ptr<Player>> players = findPlayersInInstance(*groupEntry->second);
	std::erase_if(players, [&player](const runtime::Ptr<Player>& p) { return p.get() == &player; }); // Java: players.remove(player) (the first)

	if (players.empty()) { // Create Group
		runtime::Ptr<team::group::PlayerGroup> newGroup = team::group::PlayerGroupService::createGroup(player, player, team::TeamType::AUTO_GROUP, 0);
		int32_t groupId = newGroup->getObjectId();
		if (!current->isRegistered(groupId)) {
			current->register_(groupId);
			runtime::Ref<model::instance::playerreward::HarmonyGroupReward> reward =
				model::instance::playerreward::HarmonyGroupReward::create(groupEntry->first, 12000, int8_t{7}, groupId);
			reward->addPlayer(*registeredAGPlayers.get(player.getObjectId()));
			score->addHarmonyGroup(*reward);
		}
	} else { // Add To Group
		runtime::Ptr<team::group::PlayerGroup> pg = players.front()->getPlayerGroup();
		team::group::PlayerGroupService::addPlayer(*pg, player);
		runtime::Ptr<model::instance::playerreward::HarmonyGroupReward> reward = score->getGroupReward(pg->getLeader()->getObjectId());
		reward->addPlayer(*registeredAGPlayers.get(player.getObjectId()));
	}

	if (!current->isRegistered(playerId)) {
		current->register_(playerId);
	}
}

// Java AutoHarmonyInstance.java:98-102
void AutoHarmonyInstance::onLeaveInstance(Player& player) {
	unregister(player);
	team::group::PlayerGroupService::removePlayer(player);
}

// Java AutoHarmonyInstance.java:104-112
void AutoHarmonyInstance::unregister(Player& player) {
	runtime::Ptr<AGPlayer> agp = registeredAGPlayers.get(player.getObjectId());
	if (agp != nullptr) {
		for (int32_t key : {0, 1}) { // Java: groups.get(0).remove(agp); groups.get(1).remove(agp) - the first equal element of each
			Group& group = *groups.get(key);
			for (const runtime::Ptr<AGPlayer>& member : group.snapshot()) {
				if (AutoHarmonyInstance_AGPlayerEquals::equal(*member, *agp)) {
					group.remove(member);
					break;
				}
			}
		}
	}
	AutoInstance::unregister(player);
}

// Java AutoHarmonyInstance.java:114-125
std::vector<runtime::Ptr<Player>> AutoHarmonyInstance::findPlayersInInstance(Group& group) {
	std::vector<runtime::Ptr<Player>> players;
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	if (mapInstance == nullptr) // Java: instance.getPlayersInside() on a null instance
		throw runtime::NullPointerException("AutoHarmonyInstance.instance is null");
	for (const runtime::Ptr<AGPlayer>& agp : group.snapshot()) {
		for (const runtime::Ptr<Player>& p : mapInstance->getPlayersInside()) {
			if (p->getObjectId() == agp->objectId()) {
				players.push_back(p);
				break;
			}
		}
	}
	return players;
}

// Java AutoHarmonyInstance.java:127-137 (HashMap.entrySet() of the keys 0 and 1: ascending)
std::optional<std::pair<int32_t, runtime::Ref<AutoHarmonyInstance::Group>>> AutoHarmonyInstance::getGroupEntry(int32_t playerObjId) {
	runtime::Ptr<AGPlayer> agp = registeredAGPlayers.get(playerObjId);
	if (agp != nullptr) {
		for (int32_t key : {0, 1}) {
			runtime::Ref<Group> group(groups.get(key));
			for (const runtime::Ptr<AGPlayer>& member : group->snapshot())
				if (AutoHarmonyInstance_AGPlayerEquals::equal(*member, *agp))
					return std::make_pair(key, group);
		}
	}
	return std::nullopt;
}

// Java AutoHarmonyInstance.java:139-150
AGQuestion AutoHarmonyInstance::canAddParty(Group& group, LookingForParty& lfp) {
	if (group.size() + lfp.getMembers().size() > 3)
		return AGQuestion::FAILED;
	if (!group.isEmpty() && group.get(0)->race() != lfp.getRace())
		return AGQuestion::FAILED;
	for (const auto& [objectId, member] : lfp.getMembers().snapshot()) {
		group.add(member);
		registeredAGPlayers.put(objectId, member);
	}
	return instance.get() != nullptr ? AGQuestion::ADDED
	       : static_cast<int32_t>(registeredAGPlayers.size()) == getMaxPlayers() ? AGQuestion::READY
	                                                                            : AGQuestion::ADDED;
}

} // namespace aion::gameserver::model::autogroup
