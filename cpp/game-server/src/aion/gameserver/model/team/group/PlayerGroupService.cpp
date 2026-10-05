#include "aion/gameserver/model/team/group/PlayerGroupService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/common/events/TeamKinahDistributionEvent.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/model/team/group/events/ChangeGroupLeaderEvent.h"
#include "aion/gameserver/model/team/group/events/ChangeGroupLootRulesEvent.h"
#include "aion/gameserver/model/team/group/events/GroupDisbandEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerConnectedEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerDisconnectedEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupEnteredEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupInvite.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupLeavedEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupStopMentoringEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerGroupUpdateEvent.h"
#include "aion/gameserver/model/team/group/events/PlayerStartMentoringEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/TimeUtil.h"

namespace aion::gameserver::model::team::group {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

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
	for (const runtime::Ptr<PlayerGroup>& group : groups.values()) {
		PlayerGroup& groupValue = *group;
		groupValue.forEachTeamMember([&groupValue](TeamMember& teamMember) {
			PlayerGroupMember& member = *runtime::cast<PlayerGroupMember>(teamMember);
			if (!member.isOnline() &&
				utils::TimeUtil::isExpired(member.getLastOnlineTime() + static_cast<int64_t>(configs::main::GroupConfig::GROUP_REMOVE_TIME.load()) * 1000)) {
				events::PlayerGroupLeavedEvent event(groupValue, member.getPlayer(), events::PlayerGroupLeavedEvent::LeaveReson::LEAVE_TIMEOUT);
				groupValue.onEvent(event);
			}
		});
	}
}

void PlayerGroupService::inviteToGroup(Player& inviter, Player& invited) {
	if (restrictions::PlayerRestrictions::canInviteToGroup(inviter, invited)) {
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_PARTY_INVITED_HIM(invited.getName()));
		runtime::Ref<events::PlayerGroupInvite> invite = events::PlayerGroupInvite::create(inviter);
		if (invited.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION,
				runtime::Ptr<gameobjects::player::RequestResponseHandler>(invite))) {
			PacketSendUtility::sendPacket(invited, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION, 0, 0, inviter.getName()));
		}
	}
}

runtime::Ptr<PlayerGroup> PlayerGroupService::createGroup(Player& leader, Player& invited, TeamType type, int32_t id) {
	runtime::Ref<PlayerGroup> newGroup = PlayerGroup::create(*PlayerGroupMember::create(leader), type, id);
	groups.put(newGroup->getTeamId(), newGroup);
	addPlayer(*newGroup, leader);
	addPlayer(*newGroup, invited);
	if (offlineCheckStarted.compareAndSet(false, true)) {
		initializeOfflineCheck();
	}
	return runtime::Ptr<PlayerGroup>(newGroup);
}

// new OfflinePlayerChecker() at PlayerGroupService.java:62: a K3 task object without members, run every 30 s for the life of the server. It has
// no state, so the captureless body creates one per run instead of keeping Java's single instance (no observable difference)
void PlayerGroupService::initializeOfflineCheck() {
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate([] { OfflinePlayerChecker::create()->run(); }, 1000, 30 * 1000);
}

void PlayerGroupService::addPlayerToGroup(PlayerGroup& group, Player& invited) {
	group.addMember(*PlayerGroupMember::create(invited));
	services::findgroup::FindGroupService::getInstance().onJoinedTeam(invited);
}

void PlayerGroupService::changeGroupRules(PlayerGroup& group, common::legacy::LootGroupRules& lootRules) {
	events::ChangeGroupLootRulesEvent event(group, lootRules);
	group.onEvent(event);
}

void PlayerGroupService::onPlayerLogin(Player& player) {
	for (const runtime::Ptr<PlayerGroup>& group : groups.values()) {
		runtime::Ptr<PlayerGroupMember> member = group->getMember(player.getObjectId());
		if (member) {
			events::PlayerConnectedEvent event(*group, player);
			group->onEvent(event);
		}
	}
}

void PlayerGroupService::onPlayerLogout(Player& player) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		runtime::Ptr<PlayerGroupMember> member = group->getMember(player.getObjectId());
		member->updateLastOnlineTime();
		events::PlayerDisconnectedEvent event(*group, player);
		group->onEvent(event);
	}
}

void PlayerGroupService::updateGroup(Player& player, common::legacy::GroupEvent groupEvent) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		events::PlayerGroupUpdateEvent event(*group, player, groupEvent);
		group->onEvent(event);
	}
}

void PlayerGroupService::updateGroupEffects(Player& player, int32_t slot) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		events::PlayerGroupUpdateEvent event(*group, player, common::legacy::GroupEvent::UPDATE_EFFECTS, slot);
		group->onEvent(event);
	}
}

void PlayerGroupService::addPlayer(PlayerGroup& group, Player& player) {
	// Java: Objects.requireNonNull(group, "Group should not be null") - a reference is never null
	events::PlayerGroupEnteredEvent event(group, player);
	group.onEvent(event);
}

void PlayerGroupService::removePlayer(Player& player) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		events::PlayerGroupLeavedEvent event(*group, player);
		group->onEvent(event);
	}
}

void PlayerGroupService::banPlayer(Player& bannedPlayer, Player& banGiver) {
	// Java: Objects.requireNonNull of both players - references are never null
	runtime::Ptr<PlayerGroup> group = banGiver.getPlayerGroup();
	if (group) {
		if (banGiver.equals(bannedPlayer)) {
			PacketSendUtility::sendPacket(banGiver, SM_SYSTEM_MESSAGE::STR_PARTY_CANT_BAN_SELF());
		} else if (!group->isLeader(banGiver)) {
			PacketSendUtility::sendPacket(banGiver, SM_SYSTEM_MESSAGE::STR_FORCE_ONLY_LEADER_CAN_BANISH());
		} else if (group->getTeamType() == TeamType::AUTO_GROUP) {
			PacketSendUtility::sendPacket(banGiver, SM_SYSTEM_MESSAGE::STR_MSG_PARTY_FORCE_NO_RIGHT_TO_DECIDE());
		} else if (group->hasMember(bannedPlayer.getObjectId())) {
			events::PlayerGroupLeavedEvent event(*group, bannedPlayer, events::PlayerGroupLeavedEvent::LeaveReson::BAN, banGiver.getName());
			group->onEvent(event);
		} else {
			// Java formats the List<Player> of getMembers(): "[" + each toString() joined by ", " + "]"
			std::string memberNames;
			for (const runtime::Ptr<gameobjects::AionObject>& member : group->getMembers())
				memberNames += (memberNames.empty() ? "" : ", ") + member->toString();
			log.warn("TEAM: banning {} not in group [{}]", bannedPlayer.toString(), memberNames);
		}
	}
}

void PlayerGroupService::disband(PlayerGroup& group) {
	services::findgroup::FindGroupService::getInstance().removeRecruitment(group);
	groups.remove(group.getTeamId());
	events::GroupDisbandEvent event(group);
	group.onEvent(event);
}

void PlayerGroupService::distributeKinah(Player& player, int64_t kinah) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		common::events::TeamKinahDistributionEvent event(*group, player, kinah);
		group->onEvent(event);
	}
}

void PlayerGroupService::changeLeader(Player& player) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		events::ChangeGroupLeaderEvent event(*group, player);
		group->onEvent(event);
	}
}

void PlayerGroupService::startMentoring(Player& player) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		events::PlayerStartMentoringEvent event(*group, player);
		group->onEvent(event);
	}
}

void PlayerGroupService::stopMentoring(Player& player) {
	runtime::Ptr<PlayerGroup> group = player.getPlayerGroup();
	if (group) {
		events::PlayerGroupStopMentoringEvent event(*group, player);
		group->onEvent(event);
	}
}

runtime::Ptr<PlayerGroup> PlayerGroupService::searchGroup(int32_t playerObjId) {
	for (const runtime::Ptr<PlayerGroup>& group : groups.values()) {
		if (group->hasMember(playerObjId)) {
			return group;
		}
	}
	return nullptr;
}

} // namespace aion::gameserver::model::team::group
