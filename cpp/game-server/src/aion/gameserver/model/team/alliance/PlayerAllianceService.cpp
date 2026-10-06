#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/TeamTypeInfo.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/alliance/events/AllianceDisbandEvent.h"
#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent.h"
#include "aion/gameserver/model/team/alliance/events/ChangeAllianceLeaderEvent.h"
#include "aion/gameserver/model/team/alliance/events/ChangeAllianceLootRulesEvent.h"
#include "aion/gameserver/model/team/alliance/events/ChangeMemberGroupEvent.h"
#include "aion/gameserver/model/team/alliance/events/CheckAllianceReadyEvent.h"
#include "aion/gameserver/model/team/alliance/events/PlayerAllianceEnteredEvent.h"
#include "aion/gameserver/model/team/alliance/events/PlayerAllianceInvite.h"
#include "aion/gameserver/model/team/alliance/events/PlayerAllianceLeavedEvent.h"
#include "aion/gameserver/model/team/alliance/events/PlayerAllianceUpdateEvent.h"
#include "aion/gameserver/model/team/alliance/events/PlayerConnectedEvent.h"
#include "aion/gameserver/model/team/alliance/events/PlayerDisconnectedEvent.h"
#include "aion/gameserver/model/team/common/events/TeamKinahDistributionEvent.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/TimeUtil.h"

namespace aion::gameserver::model::team::alliance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.team.alliance.PlayerAllianceService");

using gameobjects::player::Player;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

// Java implements Runnable; scheduled at a fixed rate by initializeOfflineCheck (no members: a K3 task)
class PlayerAllianceService::OfflinePlayerAllianceChecker : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<OfflinePlayerAllianceChecker> create() { return runtime::makeRef<OfflinePlayerAllianceChecker>(); }
	void run(); // @Override of a Java library type
protected:
	OfflinePlayerAllianceChecker() = default;
	~OfflinePlayerAllianceChecker() override = default;
};

void PlayerAllianceService::OfflinePlayerAllianceChecker::run() {
	for (const runtime::Ptr<PlayerAlliance>& alliance : alliances.values()) {
		PlayerAlliance& allianceValue = *alliance;
		allianceValue.forEachTeamMember([&allianceValue](TeamMember& teamMember) {
			PlayerAllianceMember& member = *runtime::cast<PlayerAllianceMember>(teamMember);
			const int32_t kickDelay = isAutoTeam(allianceValue.getTeamType()) ? 60 : configs::main::GroupConfig::ALLIANCE_REMOVE_TIME.load();
			if (!member.isOnline() && utils::TimeUtil::isExpired(member.getLastOnlineTime() + static_cast<int64_t>(kickDelay) * 1000)) {
				if (isOffence(allianceValue.getTeamType())) {
					// Java: VortexService.getInstance().removeInvaderPlayer(member.getObject()) - the vortex invasion is M5i's (m5g-plan.md O-04)
					AION_UNPORTED();
				}
				events::PlayerAllianceLeavedEvent event(allianceValue, member.getPlayer(), events::PlayerAllianceLeavedEvent::LeaveReson::LEAVE_TIMEOUT);
				allianceValue.onEvent(event);
			}
		});
	}
}

void PlayerAllianceService::inviteToAlliance(Player& inviter, Player& invitedValue) {
	if (restrictions::PlayerRestrictions::canInviteToAlliance(inviter, invitedValue)) {
		runtime::Ptr<Player> invited(invitedValue);
		runtime::Ptr<group::PlayerGroup> playerGroup = invited->getPlayerGroup();

		if (playerGroup) {
			runtime::Ptr<Player> leader = playerGroup->getLeaderObject();
			if (!leader->equals(*invited)) {
				PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_FORCE_INVITE_PARTY_HIM(invited->getName(), leader->getName()));
				PacketSendUtility::sendPacket(inviter,
					SM_SYSTEM_MESSAGE::STR_FORCE_INVITE_PARTY(leader->getName(), static_cast<int32_t>(playerGroup->getMembers().size())));
				invited = leader;
			} else {
				PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_INVITED_HIS_PARTY(invited->getName()));
			}
		} else {
			PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_FORCE_INVITED_HIM(invited->getName()));
		}

		runtime::Ref<events::PlayerAllianceInvite> invite = events::PlayerAllianceInvite::create(inviter);
		if (invited->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION,
				runtime::Ptr<gameobjects::player::RequestResponseHandler>(invite))) {
			PacketSendUtility::sendPacket(*invited,
				SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION, 0, 0, inviter.getName()));
		}
	}
}

runtime::Ptr<PlayerAlliance> PlayerAllianceService::createAlliance(Player& leader, Player& invited, TeamType type) {
	runtime::Ref<PlayerAlliance> newAlliance = PlayerAlliance::create(*PlayerAllianceMember::create(leader), type);
	alliances.put(newAlliance->getTeamId(), newAlliance);
	addPlayer(*newAlliance, leader);
	addPlayer(*newAlliance, invited);
	if (offlineCheckStarted.compareAndSet(false, true)) {
		initializeOfflineCheck();
	}
	return runtime::Ptr<PlayerAlliance>(newAlliance);
}

void PlayerAllianceService::initializeOfflineCheck() {
	// Java: scheduleAtFixedRate(new OfflinePlayerAllianceChecker(), 1000, 30 * 1000) - the checker has no state: one per run (as the parties')
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate([] { OfflinePlayerAllianceChecker::create()->run(); }, 1000, 30 * 1000);
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceService::addPlayerToAlliance(PlayerAlliance& alliance, Player& invited) {
	runtime::Ref<PlayerAllianceMember> member = PlayerAllianceMember::create(invited);
	alliance.addMember(*member);
	services::findgroup::FindGroupService::getInstance().onJoinedTeam(invited);
	return runtime::Ptr<PlayerAllianceMember>(member);
}

void PlayerAllianceService::changeGroupRules(PlayerAlliance& alliance, common::legacy::LootGroupRules& lootRules) {
	events::ChangeAllianceLootRulesEvent event(alliance, lootRules);
	alliance.onEvent(event);
}

void PlayerAllianceService::onPlayerLogin(Player& player) {
	for (const runtime::Ptr<PlayerAlliance>& alliance : alliances.values().toVector()) {
		runtime::Ptr<PlayerAllianceMember> member = alliance->getMember(player.getObjectId());
		if (member) {
			events::PlayerConnectedEvent event(*alliance, player);
			alliance->onEvent(event);
		}
	}
}

void PlayerAllianceService::onPlayerLogout(Player& player) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		runtime::Ptr<PlayerAllianceMember> member = alliance->getMember(player.getObjectId());
		if (!member)
			throw runtime::NullPointerException("PlayerAlliance.getMember(" + std::to_string(player.getObjectId()) + ")");
		member->updateLastOnlineTime();
		events::PlayerDisconnectedEvent event(*alliance, player);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::updateAlliance(Player& player, common::legacy::PlayerAllianceEvent allianceEvent) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		events::PlayerAllianceUpdateEvent event(*alliance, player, allianceEvent);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::updateAllianceEffects(Player& player, int32_t slot) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		events::PlayerAllianceUpdateEvent event(*alliance, player, common::legacy::PlayerAllianceEvent::UPDATE_EFFECTS, slot);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::addPlayer(PlayerAlliance& alliance, Player& player) {
	// Java: Objects.requireNonNull(alliance, "Alliance should not be null") - a reference is never null
	events::PlayerAllianceEnteredEvent event(alliance, player);
	alliance.onEvent(event);
}

void PlayerAllianceService::removePlayer(Player& player) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		if (isDefence(alliance->getTeamType())) {
			// Java: VortexService.getInstance().removeDefenderPlayer(player) - the vortex defence is M5i's (m5g-plan.md O-04)
			AION_UNPORTED();
		}
		events::PlayerAllianceLeavedEvent event(*alliance, player);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::banPlayer(Player& bannedPlayer, Player& banGiver) {
	// Java: Objects.requireNonNull of both players - references are never null
	runtime::Ptr<PlayerAlliance> alliance = banGiver.getPlayerAlliance();
	if (alliance) {
		if (banGiver.equals(bannedPlayer)) {
			PacketSendUtility::sendPacket(banGiver, SM_SYSTEM_MESSAGE::STR_FORCE_CANT_BAN_SELF());
		} else if (!alliance->isLeader(banGiver)) {
			PacketSendUtility::sendPacket(banGiver, SM_SYSTEM_MESSAGE::STR_FORCE_ONLY_LEADER_CAN_BANISH());
		} else if (alliance->getTeamType() == TeamType::AUTO_ALLIANCE) {
			PacketSendUtility::sendPacket(banGiver, SM_SYSTEM_MESSAGE::STR_MSG_PARTY_FORCE_NO_RIGHT_TO_DECIDE());
		} else {
			if (isDefence(alliance->getTeamType())) {
				// Java: VortexService.getInstance().removeDefenderPlayer(bannedPlayer) - the vortex defence is M5i's (m5g-plan.md O-04)
				AION_UNPORTED();
			}
			if (alliance->hasMember(bannedPlayer.getObjectId())) {
				events::PlayerAllianceLeavedEvent event(*alliance, bannedPlayer, events::PlayerAllianceLeavedEvent::LeaveReson::BAN, banGiver.getName());
				alliance->onEvent(event);
			} else {
				std::string members;
				for (const runtime::Ptr<gameobjects::AionObject>& member : alliance->getMembers())
					members += (members.empty() ? "" : ", ") + member->toString();
				log.warn("TEAM: banning {} not in alliance [{}]", bannedPlayer.toString(), members);
			}
		}
	}
}

void PlayerAllianceService::disband(PlayerAlliance& alliance, bool onBefore) {
	services::findgroup::FindGroupService::getInstance().removeRecruitment(alliance);
	runtime::Ptr<team::league::League> league = alliance.getLeague();
	if (onBefore && league) {
		// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
		// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
		runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
		// Java: league.onEvent(new LeagueLeftEvent(league, alliance)) - the league events are the league lane's (m5g-plan.md LG-03)
		AION_UNPORTED();
	}
	{
		events::AllianceDisbandEvent event(alliance);
		alliance.onEvent(event);
	}
	alliances.remove(alliance.getTeamId());
	// C++ cycle breaker (cycles.toml PlayerAlliance.groups): every member left, so the alliance drops its groups, which hold it back
	alliance.releaseGroups();
	if (!onBefore && league) {
		// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
		// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
		runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
		// Java: league.onEvent(new LeagueLeftEvent(league, alliance)) - the league events are the league lane's (m5g-plan.md LG-03)
		AION_UNPORTED();
	}
}

void PlayerAllianceService::changeLeader(Player& player) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		events::ChangeAllianceLeaderEvent event(*alliance, player);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::changeViceCaptain(Player& player, events::AssignViceCaptainEvent_AssignType assignType) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		events::AssignViceCaptainEvent event(*alliance, player, assignType);
		alliance->onEvent(event);
	}
}

runtime::Ptr<PlayerAlliance> PlayerAllianceService::searchAlliance(int32_t playerObjId) {
	for (const runtime::Ptr<PlayerAlliance>& alliance : alliances.values().toVector()) {
		if (alliance->hasMember(playerObjId)) {
			return alliance;
		}
	}
	return nullptr;
}

void PlayerAllianceService::changeMemberGroup(Player& player, int32_t firstPlayer, int32_t secondPlayer, int32_t allianceGroupId) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (!alliance) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FORCE_YOU_ARE_NOT_FORCE_MEMBER());
		return;
	}
	if (alliance->isSomeCaptain(player)) {
		events::ChangeMemberGroupEvent event(*alliance, firstPlayer, secondPlayer, allianceGroupId);
		alliance->onEvent(event);
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FORCE_RIGHT_NOT_HAVE());
	}
}

void PlayerAllianceService::checkReady(Player& player, common::events::TeamCommand eventCode) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		events::CheckAllianceReadyEvent event(*alliance, player, eventCode);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::distributeKinah(Player& player, int64_t amount) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		common::events::TeamKinahDistributionEvent event(*alliance, player, amount);
		alliance->onEvent(event);
	}
}

void PlayerAllianceService::distributeKinahInGroup(Player& player, int64_t amount) {
	runtime::Ptr<PlayerAllianceGroup> allianceGroup = player.getPlayerAllianceGroup();
	if (allianceGroup) {
		common::events::TeamKinahDistributionEvent event(*allianceGroup, player, amount);
		allianceGroup->onEvent(event);
	}
}

} // namespace aion::gameserver::model::team::alliance
