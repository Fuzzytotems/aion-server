#include "aion/gameserver/handlers/admincommands/PlayerInfo.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(PlayerInfo);

PlayerInfo::PlayerInfo()
	: AdminCommand("playerinfo", "Shows information about a player.",
		  "<player name> - Shows basic information about the given player.\n"
		  "<player name> <item|party|skills|legion|ap|chars|knownlist> - Shows extended information about the given player.\n") {
}

// Java PlayerInfo.java:33-107
void PlayerInfo::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	std::string playerName = Util::convertName(params[0]);
	runtime::Ptr<Player> target = World::getInstance().getPlayer(playerName);
	if (target == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}

	std::shared_ptr<AionConnection> connection = target->getClientConnection(); // parity: the connection of target.getClientConnection().getIP()
	if (connection == nullptr) // parity: Java's NullPointerException of target.getClientConnection().getIP(), explicit
		throw runtime::NullPointerException("Player.getClientConnection()"); // parity: (the same)
	sendInfo(admin, // parity: (the statement whose last line carries the Java)
		"[Info about " + name(*target) + "]\n- Common: lv" + std::to_string(target->getLevel()) + " (" + std::to_string(target->getCommonData()->getExpShown()) + // parity: (the statement whose last line carries the Java)
			" xp), " + std::string(xml::enumName(target->getRace())) + ", " + std::string(xml::enumName(target->getPlayerClass())) + "\n- IP: " + // parity: (the statement whose last line carries the Java)
			connection->getIP() + "\n" + "- Account name: " + target->getAccount()->getName() + "\n- " + // parity: (the statement whose last line carries the Java)
			ChatUtil::position("Location", *target->getPosition()) + ": " + target->getPosition()->toCoordString()); // parity= sendInfo(admin, "[Info about " + name(target) + "]\n- Common: lv" + target.getLevel() + " (" + target.getCommonData().getExpShown() + " xp), " + target.getRace() + ", " + target.getPlayerClass() + "\n- IP: " + target.getClientConnection().getIP() + "\n" + "- Account name: " + target.getAccount().getName() + "\n- " + ChatUtil.position("Location", target.getPosition()) + ": " + target.getPosition().toCoordString());

	if (params.size() < 2)
		return;

	if (equalsIgnoreCase(params[1], "item")) {
		std::string strbld = "- Items in inventory:"; // parity= StringBuilder strbld = new StringBuilder("- Items in inventory:");
		appendItems(strbld, target->getInventory().getItemsWithKinah());
		strbld += "\n- Equipped items:"; // parity= strbld.append("\n- Equipped items:");
		appendItems(strbld, target->getEquipment().getEquippedItems());
		strbld += "\n- Items in warehouse:"; // parity= strbld.append("\n- Items in warehouse:");
		appendItems(strbld, target->getWarehouse().getItemsWithKinah());
		sendInfo(admin, strbld); // parity= sendInfo(admin, strbld.toString());
	} else if (equalsIgnoreCase(params[1], "party")) {
		std::string sb = "- Party: "; // parity= StringBuilder sb = new StringBuilder("- Party: ");
		runtime::Ptr<model::team::TemporaryPlayerTeam> team = target->getCurrentTeam();
		if (team == nullptr) {
			sb += "none"; // parity= sb.append("none");
		} else {
			std::string simpleName = utils::simpleClassName(typeid(*team)); // parity: Java's team.getClass().getSimpleName()
			for (size_t at = simpleName.find("Player"); at != std::string::npos; at = simpleName.find("Player", at)) // parity: Java's String.replace("Player", "")
				simpleName.erase(at, 6); // parity: (the same)
			sb += simpleName; // parity= sb.append(team.getClass().getSimpleName().replace("Player", ""));
			sb += "\n\tLeader: " + name(*team->getLeaderObject()) + "\n\tMembers:\n"; // parity= sb.append("\n\tLeader: ").append(name(team.getLeaderObject())).append("\n\tMembers:\n");
			team->forEach([&sb](model::gameobjects::AionObject& player) { sb += "\t" + name(*runtime::cast<Player>(player)) + "\n"; }); // parity= team.forEach(player -> sb.append("\t").append(name(player)).append("\n"));
		}
		sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
	} else if (equalsIgnoreCase(params[1], "skills")) {
		std::string sb = "- Skills:"; // parity= StringBuilder sb = new StringBuilder("- Skills:");
		for (const runtime::Ptr<model::skill::PlayerSkillEntry>& skill : target->getSkillList()->getAllSkills())
			sb += "\n\tlevel " + std::to_string(skill->getSkillLevel()) + " of " + skill->getSkillTemplate()->getL10n(); // parity= sb.append("\n\tlevel " + skill.getSkillLevel() + " of " + skill.getSkillTemplate().getL10n());
		sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
	} else if (equalsIgnoreCase(params[1], "legion")) {
		runtime::Ptr<model::team::legion::Legion> legion = target->getLegion();
		if (legion == nullptr)
			sendInfo(admin, "- Legion: none");
		else {
			std::string sb = "- Legion: \"" + legion->getName() + "\", level: " + std::to_string(legion->getLegionLevel()); // parity= StringBuilder sb = new StringBuilder("- Legion: \"" + legion.getName() + "\", level: " + legion.getLegionLevel());
			sb += "\n\t" + std::to_string(legion->getMembers().size()) + " members:"; // parity= sb.append("\n\t").append(legion.getMembers().size()).append(" members:");
			for (const runtime::Ptr<model::team::legion::LegionMember>& lm : legion->getMembers())
				sb += "\n\t" + lm->getName() + " - " + std::string(xml::enumName(lm->getRank())) + (lm->isOnline() ? " (online)" : ""); // parity= sb.append("\n\t").append(lm.getName()).append(" - ").append(lm.getRank()).append(lm.isOnline() ? " (online)" : "");
			sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
		}
	} else if (equalsIgnoreCase(params[1], "ap")) {
		sendInfo(admin, "- AP info:");
		sendInfo(admin, "\tTotal AP = " + std::to_string(target->getAbyssRank()->getAp()));
		sendInfo(admin, "\tTotal Kills = " + std::to_string(target->getAbyssRank()->getAllKill()));
		sendInfo(admin, "\tToday Kills = " + std::to_string(target->getAbyssRank()->getDailyKill()));
		sendInfo(admin, "\tToday AP = " + std::to_string(target->getAbyssRank()->getDailyAP()));
	} else if (equalsIgnoreCase(params[1], "chars")) {
		sendInfo(admin, "- Characters (" + std::to_string(target->getAccount()->size()) + "):");
		for (const runtime::Ptr<model::account::PlayerAccountData>& d : target->getAccount()->getPlayerAccDataList()) // parity= target.getAccount().forEach(d -> sendInfo(admin, "\t" + d.getPlayerCommonData().getName()));
			sendInfo(admin, "\t" + d->getPlayerCommonData()->getName()); // parity: (the forEach of the line above)
	} else if (equalsIgnoreCase(params[1], "knownlist")) {
		std::string list; // parity: Java's stream().map(...).collect(Collectors.joining())
		for (const runtime::Ptr<world::knownlist::KnownObject>& o : target->getKnownList().stream()) // parity: (the same)
			list += "\n\t" + o->toString(); // parity: (the same)
		sendInfo(admin, "- KnownList:" + list); // parity= sendInfo(admin, "- KnownList:" + target.getKnownList().stream().map(o -> "\n\t" + o).collect(Collectors.joining()));
	} else {
		sendInfo(admin);
	}
}

// Java PlayerInfo.java:109-115
void PlayerInfo::appendItems(std::string& strbld, const std::vector<runtime::Ptr<Item>>& items) {
	if (items.empty())
		strbld += "\nnone"; // parity= strbld.append("\nnone");
	else
		for (const runtime::Ptr<Item>& item : items) // parity= items.forEach(item -> strbld.append("\n").append(ChatUtil.leftPad(item.getItemCount(), 4)).append("x ").append(ChatUtil.item(item.getItemId())));
			strbld += "\n" + ChatUtil::leftPad(item->getItemCount(), 4) + "x " + ChatUtil::item(item->getItemId()); // parity: (the forEach of the line above)
}

} // namespace aion::gameserver::handlers::admincommands
