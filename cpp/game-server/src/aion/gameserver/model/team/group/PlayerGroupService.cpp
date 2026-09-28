#include "aion/gameserver/model/team/group/PlayerGroupService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::model::team::group {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.team.group.PlayerGroupService");

// Java implements Runnable; scheduled at a fixed rate by initializeOfflineCheck (no members: a K3 task)
class PlayerGroupService::OfflinePlayerChecker : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<OfflinePlayerChecker> create() { return runtime::makeRef<OfflinePlayerChecker>(); }
	void run(); // @Override of a Java library type
protected:
	OfflinePlayerChecker() = default;
	~OfflinePlayerChecker() override = default;
};

void PlayerGroupService::OfflinePlayerChecker::run() {
	AION_UNPORTED();
}

void PlayerGroupService::inviteToGroup(gameobjects::player::Player& inviter, gameobjects::player::Player& invited) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerGroup> PlayerGroupService::createGroup(gameobjects::player::Player& leader, gameobjects::player::Player& invited, TeamType type, int32_t id) {
	AION_UNPORTED();
}

void PlayerGroupService::initializeOfflineCheck() {
	AION_UNPORTED();
}

void PlayerGroupService::addPlayerToGroup(PlayerGroup& group, gameobjects::player::Player& invited) {
	AION_UNPORTED();
}

void PlayerGroupService::changeGroupRules(PlayerGroup& group, common::legacy::LootGroupRules& lootRules) {
	AION_UNPORTED();
}

void PlayerGroupService::onPlayerLogin(gameobjects::player::Player& player) {
	for (const runtime::Ptr<PlayerGroup>& group : groups.values().toVector()) {
		runtime::Ptr<PlayerGroupMember> member = group->getMember(player.getObjectId());
		if (member) {
			// Java: group.onEvent(new PlayerConnectedEvent(group, player)); the group events are ported with the teams (M5b)
			AION_UNPORTED();
		}
	}
}

void PlayerGroupService::onPlayerLogout(gameobjects::player::Player& player) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		// Java: member.updateLastOnlineTime(); group.onEvent(new PlayerDisconnectedEvent(group, player)); ported with the teams (M5b)
		AION_UNPORTED();
	}
}

void PlayerGroupService::updateGroup(gameobjects::player::Player& player, common::legacy::GroupEvent groupEvent) {
	AION_UNPORTED();
}

void PlayerGroupService::updateGroupEffects(gameobjects::player::Player& player, int32_t slot) {
	AION_UNPORTED();
}

void PlayerGroupService::addPlayer(PlayerGroup& group, gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerGroupService::removePlayer(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerGroupService::banPlayer(gameobjects::player::Player& bannedPlayer, gameobjects::player::Player& banGiver) {
	AION_UNPORTED();
}

void PlayerGroupService::disband(PlayerGroup& group) {
	AION_UNPORTED();
}

void PlayerGroupService::distributeKinah(gameobjects::player::Player& player, int64_t kinah) {
	AION_UNPORTED();
}

void PlayerGroupService::changeLeader(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerGroupService::startMentoring(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerGroupService::stopMentoring(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerGroup> PlayerGroupService::searchGroup(int32_t playerObjId) {
	for (const runtime::Ptr<PlayerGroup>& group : groups.values().toVector()) {
		if (group->hasMember(playerObjId)) {
			return group;
		}
	}
	return nullptr;
}

} // namespace aion::gameserver::model::team::group
