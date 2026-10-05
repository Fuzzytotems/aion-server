#include "aion/gameserver/handlers/admincommands/AddSkill.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(AddSkill);

AddSkill::AddSkill() : AdminCommand("addskill") {
}

// Java AddSkill.java:20-46
void AddSkill::execute(Player& player, std::span<const std::string> params) {
	if (params.size() != 2) {
		PacketSendUtility::sendMessage(player, "syntax //addskill <skillId> <skillLevel>");
		return;
	}

	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(player.getTarget());
	Player& target = targetPlayer != nullptr ? *targetPlayer : player;
	int32_t skillId = 0;
	int32_t skillLevel = 0;

	try {
		skillId = commons::utils::parseInt(params[0]);
		skillLevel = commons::utils::parseInt(params[1]);
	} catch (const commons::utils::NumberFormatException&) {
		PacketSendUtility::sendMessage(player, "Parameters need to be an integer.");
		return;
	}

	target.getSkillList()->addSkill(target, skillId, skillLevel);
	PacketSendUtility::sendMessage(player, "You have success add skill");
	if (!target.equals(player))
		PacketSendUtility::sendMessage(target, "You have acquire a new skill");
}

// Java AddSkill.java:48-51
void AddSkill::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //addskill <skillId> <skillLevel>");
}

} // namespace aion::gameserver::handlers::admincommands
