#include "aion/gameserver/handlers/admincommands/DelSkill.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(DelSkill);

DelSkill::DelSkill() : AdminCommand("delskill") {
}

// Java DelSkill.java:23-75
void DelSkill::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1 || params.size() > 2) {
		PacketSendUtility::sendMessage(admin, "No parameters detected.\nPlease use //delskill <Player name> <all | skillId>\n" // parity= PacketSendUtility.sendMessage(admin, "No parameters detected.\n" + "Please use //delskill <Player name> <all | skillId>\n" + "or use //delskill [target] <all | skillId>");
											  "or use //delskill [target] <all | skillId>"); // parity: (continued)
		return;
	}

	runtime::Ptr<Player> player;
	runtime::Ptr<PlayerSkillList> playerSkillList = nullptr;
	std::string recipient; // parity= String recipient = null;
	recipient = Util::convertName(params[0]);
	int32_t skillId = 0;

	if (params.size() == 2) {
		player = World::getInstance().getPlayer(recipient);
		if (player == nullptr) {
			PacketSendUtility::sendMessage(admin, "The specified player is not online.");
			return;
		}
		if (std::string_view("all").starts_with(params[1])) // parity= if ("all".startsWith(params[1]))
			playerSkillList = player->getSkillList();
		else {
			try {
				skillId = commons::utils::parseInt(params[1]);
			} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
				PacketSendUtility::sendMessage(admin, "Param 1 must be an integer or <all>.");
				return;
			}
			// Correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/C1.md): skill ID 0 is apply's "all"
			// arm, so it skips the presence and stigma checks (Java's getSkillEntry(0).isStigmaSkill() threw) and takes the skill list
			if (skillId == 0) // parity: the owner's correction of 2026-10-05 above (skill ID 0 takes the skill list)
				playerSkillList = player->getSkillList(); // parity: (continued)
			else if (!check(admin, *player, skillId))
				return;
		}
		apply(admin, *player, skillId, playerSkillList);
	}
	if (params.size() == 1) {
		runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= player = admin.getTarget() instanceof Player target ? target : admin;
		player = target != nullptr ? target : runtime::Ptr<Player>(&admin); // parity: (continued)
		if (std::string_view("all").starts_with(params[0])) // parity= if ("all".startsWith(params[0]))
			playerSkillList = player->getSkillList();
		else {
			try {
				skillId = commons::utils::parseInt(params[0]);
			} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
				PacketSendUtility::sendMessage(admin, "Param 0 must be an integer or <all>.");
				return;
			}
			// Correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/C1.md): skill ID 0 is apply's "all"
			// arm, so it skips the presence and stigma checks (Java's getSkillEntry(0).isStigmaSkill() threw) and takes the skill list
			if (skillId == 0) // parity: the owner's correction of 2026-10-05 above (skill ID 0 takes the skill list)
				playerSkillList = player->getSkillList(); // parity: (continued)
			else if (!check(admin, *player, skillId))
				return;
		}
		apply(admin, *player, skillId, playerSkillList);
	}
}

// Java DelSkill.java:77-87
bool DelSkill::check(Player& admin, Player& player, int32_t skillId) {
	if (skillId != 0 && !player.getSkillList()->isSkillPresent(skillId)) {
		PacketSendUtility::sendMessage(admin, "Player dont have this skill.");
		return false;
	}
	runtime::Ptr<model::skill::PlayerSkillEntry> skillEntry = player.getSkillList()->getSkillEntry(skillId);
	if (skillEntry == nullptr) // parity: Java's NullPointerException of getSkillEntry(skillId).isStigmaSkill(), explicit; execute no longer calls check for skill ID 0 (the owner's correction, C1.md)
		throw runtime::NullPointerException("Cannot invoke \"PlayerSkillEntry.isStigmaSkill()\" because the return value of \"getSkillEntry(int)\" is null"); // parity: (the same)
	if (skillEntry->isStigmaSkill()) {
		PacketSendUtility::sendMessage(admin, "You can't remove stigma skill.");
		return false;
	}
	return true;
}

// Java DelSkill.java:89-102
void DelSkill::apply(Player& admin, Player& player, int32_t skillId, runtime::Ptr<PlayerSkillList> playerSkillList) {
	if (skillId != 0) {
		SkillLearnService::removeSkill(player, skillId);
		PacketSendUtility::sendMessage(admin, "You have successfully deleted the specified skill.");
	} else {
		for (const runtime::Ptr<model::skill::PlayerSkillEntry>& skillEntry : playerSkillList->getAllSkills()) {
			if (!skillEntry->isStigmaSkill()) {
				SkillLearnService::removeSkill(player, skillEntry->getSkillId());
			}
		}
		PacketSendUtility::sendMessage(admin, "You have success delete All skills.");
	}
}

// Java DelSkill.java:104-108
void DelSkill::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "No parameters detected.\nPlease use //delskill <Player name> <all | skillId>\n" // parity= PacketSendUtility.sendMessage(player, "No parameters detected.\n" + "Please use //delskill <Player name> <all | skillId>\n" + "or use //delskill [target] <all | skillId>");
										   "or use //delskill [target] <all | skillId>"); // parity: (continued)
}

} // namespace aion::gameserver::handlers::admincommands
