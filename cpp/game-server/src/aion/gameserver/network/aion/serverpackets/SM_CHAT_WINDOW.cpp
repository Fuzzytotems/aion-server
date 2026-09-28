#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"

#include <array>
#include <string>
#include <vector>

#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHAT_WINDOW::SM_CHAT_WINDOW(model::gameobjects::player::Player& targetValue, bool isGroupValue)
	: AionServerPacket(opcodeOf<SM_CHAT_WINDOW>), target(targetValue), isGroup(isGroupValue) {
}

SM_CHAT_WINDOW::~SM_CHAT_WINDOW() = default;

void SM_CHAT_WINDOW::writeImpl(AionConnection* con) {
	using model::gameobjects::player::Player;
	if (!target)
		return;
	if (isGroup) {
		if (target->isInGroup()) {
			writeC(2); // group
			writeS(target->getName(true));
			runtime::Ptr<model::team::group::PlayerGroup> group = target->getPlayerGroup();
			model::team::GeneralTeam& team = *group;
			writeD(team.getTeamId());
			writeS(team.getLeader()->getName()); // C++: GeneralTeam::getLeader is Java's PlayerGroup.getLeader() before its cast (P5-10)
			std::vector<runtime::Ptr<model::gameobjects::AionObject>> members = team.getMembers();
			for (runtime::Ptr<model::gameobjects::AionObject> groupMember : members)
				writeC(runtime::cast<Player>(groupMember)->getLevel());
			for (int32_t i = team.size(); i < 6; i++)
				writeC(0);
			for (runtime::Ptr<model::gameobjects::AionObject> groupMember : members)
				writeC(model::getClassId(runtime::cast<Player>(groupMember)->getPlayerClass()));
			for (int32_t i = team.size(); i < 6; i++)
				writeC(0);
		} else if (target->isInAlliance()) {
			writeC(3); // alliance
			runtime::Ptr<model::team::alliance::PlayerAlliance> alliance = target->getPlayerAlliance();
			model::team::GeneralTeam& team = *alliance;
			writeS(team.getLeader()->getName());
			writeD(team.getTeamId());
			std::vector<runtime::Ptr<model::gameobjects::AionObject>> members = team.getMembers();
			std::vector<runtime::Ptr<model::gameobjects::AionObject>> membersIt = team.getMembers(); // Java: alliance.getMembers().iterator()
			size_t next = 0;
			std::array<std::string, 4> capitans{"", "", "", ""};
			for (size_t i = 0; i < capitans.size(); i++) {
				while (next < membersIt.size()) {
					runtime::Ptr<Player> groupMember = runtime::cast<Player>(membersIt[next++]);
					if (alliance->isSomeCaptain(*groupMember)) {
						capitans[i] = groupMember->getName();
						break;
					}
				}
			}
			for (const std::string& capitan : capitans) {
				writeS(capitan);
			}
			writeH(0);
			writeC(team.size());
			writeH(alliance->getMinExpPlayerLevel()); // LVL
			writeH(alliance->getMaxExpPlayerLevel());
			std::array<int16_t, 17> counts{}; // Java: new short[PlayerClass.values().length]
			for (runtime::Ptr<model::gameobjects::AionObject> groupMember : members) {
				counts.at(static_cast<size_t>(model::getClassId(runtime::cast<Player>(groupMember)->getPlayerClass())))++;
			}
			for (int16_t count : counts) {
				writeH(count);
			}
		} else {
			writeC(4); // no group
			writeS(target->getName(true));
			writeD(0); // no group yet
			writeC(model::getClassId(target->getPlayerClass()));
			writeC(target->getLevel());
			writeC(0); // unk
		}
	} else {
		writeC(1);
		writeS(target->getName(true));
		writeS(target->getLegion() ? target->getLegion()->getName() : "");
		writeC(target->getLevel());
		writeH(model::getClassId(target->getPlayerClass()));
		writeS(target->getCommonData()->getNote());
		writeD(1); // unk
		writeC(target->getAccount()->getMembership()); // vip level
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
