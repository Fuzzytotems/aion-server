#include "aion/gameserver/handlers/admincommands/NpcSkill.h"

#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplates.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(NpcSkill);

NpcSkill::NpcSkill() : AdminCommand("npcskill") {
}

// Java NpcSkill.java:23-46
void NpcSkill::execute(Player& admin, std::span<const std::string> /*params*/) {
	runtime::Ptr<Npc> target = nullptr;
	runtime::Ptr<VisibleObject> creature = admin.getTarget();
	if (runtime::as<Npc>(admin.getTarget()) != nullptr) {
		target = runtime::as<Npc>(creature);
	}

	if (target == nullptr) {
		PacketSendUtility::sendMessage(admin, "You should select a valid target first!");
		return;
	}

	std::string strbld = "-list of skills:\n";
	const std::vector<NpcSkillTemplate>* list = nullptr;
	if (DataManager::NPC_SKILL_DATA->getNpcSkillList(target->getNpcId()) != nullptr) {
		list = &DataManager::NPC_SKILL_DATA->getNpcSkillList(target->getNpcId())->getNpcSkills();
	}

	if (list != nullptr && !list->empty()) {
		for (const NpcSkillTemplate& skill : *list)
			strbld += "    level " + std::to_string(skill.getSkillLevel()) + " of " + std::to_string(skill.getSkillId()) + ", " +
				std::to_string(skill.getProbability()) + "% prob and " + std::to_string(skill.getCooldown()) + "ms cd.\n";
		showAllLines(admin, strbld);
	} else {
		PacketSendUtility::sendMessage(admin, "This npc does not have any skills.");
	}
}

// Java NpcSkill.java:48-66
void NpcSkill::showAllLines(Player& admin, std::string_view str) {
	size_t index = 0;
	std::vector<std::string> strarray = commons::utils::StringUtils::splitJava(str, "\n");
	while (strarray.size() >= 20 && index < strarray.size() - 20) { // Java: index < strarray.length - 20 (int arithmetic)
		std::string strbld;
		for (int32_t i = 0; i < 20; i++, index++) {
			strbld += strarray[index];
			if (i < 20 - 1)
				strbld += "\n";
		}
		PacketSendUtility::sendMessage(admin, strbld);
	}
	size_t odd = strarray.size() - index;
	std::string strbld;
	for (size_t i = 0; i < odd; i++, index++)
		strbld += strarray[index] + "\n";
	PacketSendUtility::sendMessage(admin, strbld);
}

// Java NpcSkill.java:68-71 (an empty override)
void NpcSkill::info(Player& /*player*/, std::optional<std::string_view> /*message*/) {
}

} // namespace aion::gameserver::handlers::admincommands
