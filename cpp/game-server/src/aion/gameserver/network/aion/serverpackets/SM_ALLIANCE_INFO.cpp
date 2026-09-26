#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: league.getMember(allianceObjectId).getLeaguePosition() - LeagueMember.h (P5-10) is not written yet */
int32_t leaguePosition(model::team::league::League& league, int32_t allianceObjectId) {
	static_cast<void>(league);
	static_cast<void>(allianceObjectId);
	AION_UNPORTED();
}

} // namespace

runtime::Ref<SM_ALLIANCE_INFO::AllianceInfo> SM_ALLIANCE_INFO::AllianceInfo::create() {
	return runtime::makeRef<AllianceInfo>();
}

SM_ALLIANCE_INFO::AllianceInfo::AllianceInfo() = default;

SM_ALLIANCE_INFO::AllianceInfo::~AllianceInfo() = default;

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue) : SM_ALLIANCE_INFO(allianceValue, 0, "", nullptr) {
}

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue, model::team::alliance::PlayerAlliance& skipped)
	: SM_ALLIANCE_INFO(allianceValue, 0, "", skipped) {
}

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue, int32_t messageIdValue, std::string_view messageValue)
	: SM_ALLIANCE_INFO(allianceValue, messageIdValue, messageValue, nullptr) {
}

SM_ALLIANCE_INFO::SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& allianceValue, int32_t messageIdValue, std::string_view messageValue,
	runtime::Ptr<model::team::alliance::PlayerAlliance> skipped)
	: AionServerPacket(opcodeOf<SM_ALLIANCE_INFO>), lootRules(allianceValue.getLootGroupRules()), alliance(allianceValue),
	  // C++: GeneralTeam::getLeader returns the member Java's PlayerAlliance.getLeader() casts (PlayerAllianceMember.h is not written yet, P5-10)
	  leaderid(static_cast<model::team::GeneralTeam&>(allianceValue).getLeader()->getObjectId()), groupid(allianceValue.getObjectId()),
	  type(detail::teamTypeType(allianceValue.getTeamType())), subType(detail::teamTypeSubType(allianceValue.getTeamType())), messageId(messageIdValue),
	  message(messageValue) {
	runtime::Ptr<model::team::league::League> league = allianceValue.getLeague();
	if (league) {
		leagueId = league->getTeamId();
		lootLeagueRules = league->getLootGroupRules();
		for (runtime::Ptr<model::gameobjects::player::Player> captain : league->getCaptains()) {
			runtime::Ref<AllianceInfo> info = AllianceInfo::create();
			runtime::Ptr<model::team::alliance::PlayerAlliance> captainAlliance = captain->getPlayerAlliance();
			if (captainAlliance) {
				info->setAlliancePosition(leaguePosition(*league, captainAlliance->getObjectId()));
				info->setAllianceObjectId(captainAlliance->getObjectId());
				info->setMemberCount(captainAlliance->size());
				if (!skipped || !captainAlliance->equals(*skipped)) { // Java: !captainAlliance.equals(skipped)
					info->setCaptainName(captain->getName());
					info->setCaptainWorldId(captain->getWorldId());
				}
			}
			leagueData.push_back(std::move(info));
		}
	}
}

SM_ALLIANCE_INFO::~SM_ALLIANCE_INFO() = default;

void SM_ALLIANCE_INFO::writeImpl(AionConnection* con) {
	if (con == nullptr)
		throw runtime::NullPointerException("SM_ALLIANCE_INFO::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	writeH(alliance->groupSize());
	writeD(groupid);
	writeD(leaderid);
	writeD(!player || !player->getPosition() ? 0 : player->getWorldId()); // mapId
	std::vector<int32_t> ids = alliance->getViceCaptainIds().snapshot();
	for (int32_t id : ids) {
		writeD(id);
	}
	for (int32_t i = 0; i < 4 - static_cast<int32_t>(ids.size()); i++) {
		writeD(0);
	}
	writeD(detail::lootRuleId(lootRules->getLootRule()));
	writeD(lootRules->getMisc());
	writeD(lootRules->getCommonItemAbove());
	writeD(lootRules->getSuperiorItemAbove());
	writeD(lootRules->getHeroicItemAbove());
	writeD(lootRules->getFabledItemAbove());
	writeD(lootRules->getEternalItemAbove());
	writeD(lootRules->getMythicItemAbove());
	writeD(0x02);
	writeC(0x00);
	writeD(type);
	writeD(subType); // 3.5
	writeD(leagueId);
	for (int32_t a = 0; a < 4; a++) {
		writeD(a); // group num
		writeD(1000 + a); // group id
	}
	writeD(messageId); // System message ID
	writeS(messageId != 0 ? std::string_view(message) : std::string_view()); // System message

	if (!leagueData.empty()) {
		writeH(static_cast<int32_t>(leagueData.size()));
		writeD(detail::lootRuleId(lootLeagueRules->getLootRule()));
		writeD(lootLeagueRules->getMisc());
		writeD(lootLeagueRules->getCommonItemAbove());
		writeD(lootLeagueRules->getSuperiorItemAbove());
		writeD(lootLeagueRules->getHeroicItemAbove());
		writeD(lootLeagueRules->getFabledItemAbove());
		writeD(lootLeagueRules->getEternalItemAbove());
		writeD(lootLeagueRules->getMythicItemAbove());
		writeD(0x02);
		for (const runtime::Ref<AllianceInfo>& info : leagueData) {
			writeD(info->getAlliancePosition());
			writeD(info->getAllianceObjectId());
			writeD(info->getMemberCount());
			writeS(info->getCaptainName());
			writeD(info->getCaptainWorldId());
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
