#include "aion/gameserver/model/team/league/LeagueService.h"

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/model/team/league/events/LeagueChangeLeaderEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueCreateEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueDisbandEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueInviteEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueJoinEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueKinahDistributionEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueLeftEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueLootRulesChangeEvent.h"
#include "aion/gameserver/model/team/league/events/LeagueMoveEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::league {

using alliance::PlayerAlliance;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

namespace {

/** Java: player.getPlayerAlliance() dereferenced - a player without an alliance is Java's NullPointerException */
PlayerAlliance& allianceOf(Player& player) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (!alliance)
		throw runtime::NullPointerException(player.getName() + ".getPlayerAlliance()");
	return *alliance;
}

/** Java: alliance.getLeague() dereferenced */
League& leagueOf(PlayerAlliance& alliance) {
	runtime::Ptr<League> league = alliance.getLeague();
	if (!league)
		throw runtime::NullPointerException("PlayerAlliance.getLeague()");
	return *league;
}

} // namespace

void LeagueService::inviteToLeague(Player& inviter, Player& invitedValue) {
	if (canInvite(inviter, invitedValue)) {
		runtime::Ptr<Player> invited(invitedValue);
		runtime::Ptr<PlayerAlliance> playerAlliance = invited->getPlayerAlliance();

		if (playerAlliance) {
			runtime::Ptr<Player> leader = playerAlliance->getLeaderObject();
			if (!leader->equals(*invited)) {
				PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_INVITE_HIS_LEADER(invited->getName(), leader->getName()));
			}
			invited = leader;
		}

		runtime::Ref<events::LeagueInviteEvent> invite = events::LeagueInviteEvent::create(inviter, *invited);
		if (invited->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_MSGBOX_UNION_INVITE_ME,
				runtime::Ptr<gameobjects::player::RequestResponseHandler>(invite))) {
			PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_INVITE_HIM(invited->getName(), allianceOf(*invited).size()));
			PacketSendUtility::sendPacket(*invited, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_MSGBOX_UNION_INVITE_ME, 0, 0, inviter.getName()));
		}
	}
}

bool LeagueService::canInvite(Player& inviter, Player& invited) {
	if (inviter.isDead()) {
		// You cannot use the Alliance League invitation function while you are dead.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_CANT_INVITE_WHEN_DEAD());
		return false;
	} else if (!invited.isOnline()) {
		// The player you invited to the Alliance League is currently offline.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_OFFLINE_MEMBER());
		return false;
	} else if (!invited.getPlayerAlliance()) {
		// Currently, %0 cannot accept your invitation to join the alliance.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_CANT_INVITE_WHEN_HE_IS_ASKED_QUESTION(invited.getName()));
		return false;
	} else if (allianceOf(inviter).hasMember(invited.getObjectId())) {
		// You cannot invite your own alliance.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_CANT_INVITE_SELF());
		return false;
	} else if (allianceOf(invited).isInLeague()) {
		// The selected target is already a member of another force league.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_ALREADY_MY_UNION());
		return false;
	} else if (allianceOf(inviter).isInLeague() && leagueOf(allianceOf(inviter)).isFull()) {
		// You cannot invite anymore as the Alliance League is full.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_CANT_ADD_NEW_MEMBER());
		return false;
	} else if (allianceOf(inviter).isInLeague() && allianceOf(invited).isInLeague() &&
	           leagueOf(allianceOf(inviter)).equals(leagueOf(allianceOf(invited)))) {
		// %0 is already a member of another Alliance League.
		PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_UNION_ALREADY_OTHER_UNION(invited.getName()));
		return false;
	}
	return true;
}

runtime::Ptr<League> LeagueService::createLeague(Player& leader) {
	runtime::Ptr<PlayerAlliance> alliance = leader.getPlayerAlliance();
	if (!alliance)
		throw runtime::NullPointerException("Alliance can not be null");
	runtime::Ref<LeagueMember> mainAlliance = LeagueMember::create(*alliance, 0);
	runtime::Ref<League> league = League::create(*mainAlliance);
	league->setLootGroupRules(common::legacy::LootGroupRules::create(common::legacy::LootRuleType::FREEFORALL, 0, 0, 2, 2, 2, 2, 2));
	league->addMember(*mainAlliance);
	leagues.put(league->getTeamId(), league);
	events::LeagueCreateEvent event(*league);
	league->onEvent(event);
	return runtime::Ptr<League>(league);
}

void LeagueService::addAlliance(League& league, PlayerAlliance& alliance) {
	// Java: Objects.requireNonNull(league, "League should not be null") - a reference is never null
	events::LeagueJoinEvent event(league, alliance);
	league.onEvent(event);
}

void LeagueService::removeAlliance(runtime::Ptr<PlayerAlliance> alliance) {
	if (alliance) {
		runtime::Ptr<League> league = alliance->getLeague();
		if (!league)
			throw runtime::NullPointerException("League should not be null");
		events::LeagueLeftEvent event(*league, *alliance, events::LeagueLeftEvent::LeaveReson::LEAVE);
		league->onEvent(event);
	}
}

void LeagueService::expelAlliance(LeagueMember& leagueAlliance, Player& leagueLeader) {
	PlayerAlliance& leagueLeaderAlliance = allianceOf(leagueLeader);
	if (!leagueLeaderAlliance.isLeader(leagueLeader))
		throw commons::utils::IllegalArgumentException("Given player is not the league alliance leader");
	League& league = leagueOf(leagueLeaderAlliance);
	if (!league.isLeader(leagueLeaderAlliance))
		throw commons::utils::IllegalArgumentException("Leader's alliance is not the league leader");
	events::LeagueLeftEvent event(league, leagueAlliance.getAlliance(), events::LeagueLeftEvent::LeaveReson::EXPEL);
	league.onEvent(event);
}

void LeagueService::setLeader(Player& player, Player& allianceLeader) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		runtime::Ptr<League> league = alliance->getLeague();
		if (league) {
			events::LeagueChangeLeaderEvent event(*alliance, allianceLeader);
			league->onEvent(event);
		}
	}
}

void LeagueService::disband(League& league) {
	events::LeagueDisbandEvent event(league);
	league.onEvent(event);
	leagues.remove(league.getTeamId());
}

std::vector<runtime::Ptr<League>> LeagueService::getLeagues() {
	std::vector<runtime::Ptr<League>> result;
	for (const runtime::Ptr<League>& league : leagues.values())
		result.push_back(league);
	return result;
}

void LeagueService::moveAlliance(Player& player, int32_t selectedId, int32_t targetId) {
	League& league = leagueOf(allianceOf(player));
	if (league.getLeaderObject()->getLeaderObject()->equals(player)) {
		events::LeagueMoveEvent event(league, selectedId, targetId);
		league.onEvent(event);
	}
}

void LeagueService::changeGroupRules(League& league, common::legacy::LootGroupRules& lootRules) {
	events::LeagueLootRulesChangeEvent event(league, lootRules);
	league.onEvent(event);
}

void LeagueService::distributeKinah(Player& player, int64_t amount) {
	runtime::Ptr<League> league = allianceOf(player).getLeague();
	if (league) {
		events::LeagueKinahDistributionEvent event(player, amount);
		league->onEvent(event);
	}
}

} // namespace aion::gameserver::model::team::league
