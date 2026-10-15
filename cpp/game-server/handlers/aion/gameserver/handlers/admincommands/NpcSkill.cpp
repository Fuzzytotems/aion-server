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

	std::string strbld = "-list of skills:\n"; // parity= StringBuilder strbld = new StringBuilder("-list of skills:\n");
	const std::vector<NpcSkillTemplate>* list = nullptr;
	if (DataManager::NPC_SKILL_DATA->getNpcSkillList(target->getNpcId()) != nullptr) {
		list = &DataManager::NPC_SKILL_DATA->getNpcSkillList(target->getNpcId())->getNpcSkills();
	}

	if (list != nullptr && !list->empty()) {
		for (const NpcSkillTemplate& skill : *list)
			strbld += "    level " + std::to_string(skill.getSkillLevel()) + " of " + std::to_string(skill.getSkillId()) + ", " + // parity= strbld.append("    level " + skill.getSkillLevel() + " of " + skill.getSkillId() + ", " + skill.getProbability() + "% prob and " + skill.getCooldown() +"ms cd.\n");
				std::to_string(skill.getProbability()) + "% prob and " + std::to_string(skill.getCooldown()) + "ms cd.\n"; // parity: (continued)
		showAllLines(admin, strbld); // parity= showAllLines(admin, strbld.toString());
	} else {
		PacketSendUtility::sendMessage(admin, "This npc does not have any skills.");
	}
}

// Java NpcSkill.java:48-66
void NpcSkill::showAllLines(Player& admin, std::string_view str) {
	size_t index = 0;
	std::vector<std::string> strarray = commons::utils::StringUtils::splitJava(str, "\n"); // parity= String[] strarray = str.split("\n");
	while (strarray.size() >= 20 && index < strarray.size() - 20) { // parity= while (index < strarray.length - 20) { // int arithmetic in Java; size_t here, hence the size guard
		std::string strbld; // parity= StringBuilder strbld = new StringBuilder();
		for (int32_t i = 0; i < 20; i++, index++) {
			strbld += strarray[index]; // parity= strbld.append(strarray[index]);
			if (i < 20 - 1)
				strbld += "\n"; // parity= strbld.append("\n");
		}
		PacketSendUtility::sendMessage(admin, strbld); // parity= PacketSendUtility.sendMessage(admin, strbld.toString());
	}
	size_t odd = strarray.size() - index;
	std::string strbld; // parity= StringBuilder strbld = new StringBuilder();
	for (size_t i = 0; i < odd; i++, index++)
		strbld += strarray[index] + "\n"; // parity= strbld.append(strarray[index] + "\n");
	PacketSendUtility::sendMessage(admin, strbld); // parity= PacketSendUtility.sendMessage(admin, strbld.toString());
}

// Java NpcSkill.java:68-71 (an empty override)
void NpcSkill::info(Player& /*player*/, std::optional<std::string_view> /*message*/) { // parity= public void info(Player player, String message) {
}

} // namespace aion::gameserver::handlers::admincommands
