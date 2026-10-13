#include "aion/gameserver/handlers/consolecommands/Deleteskill.h"

#include <memory>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/services/SkillLearnService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Deleteskill);

// Java Deleteskill.java:25-27
Deleteskill::Deleteskill() : ConsoleCommand("deleteskill") {
}

// Java Deleteskill.java:29-49
void Deleteskill::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	std::string skillName = params[0];
	int32_t skillId = 0;

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)
	std::optional<SkillTemplate> skillTemplate = getSkillTemplate(skillName); // parity= File xml = new File("./data/handlers/consolecommands/data/skills.xml"); SkillData data = JAXBUtil.deserialize(xml, SkillData.class); SkillTemplate skillTemplate = data.getSkillTemplate(skillName);

	if (skillTemplate) // parity= if (skillTemplate != null)
		skillId = skillTemplate->id; // parity= skillId = skillTemplate.getTemplateId();

	if (skillId > 0 && check(admin, player, skillId))
		apply(admin, player, skillId);
}

// Java Deleteskill.java:46-57
bool Deleteskill::check(Player& admin, Player& player, int32_t skillId) {
	if (skillId != 0 && !player.getSkillList()->isSkillPresent(skillId)) {
		PacketSendUtility::sendMessage(admin, "Player dont have this skill.");
		return false;
	}
	if (player.getSkillList()->getSkillEntry(skillId)->isStigmaSkill()) {
		PacketSendUtility::sendMessage(admin, "You can't remove stigma skill.");
		return false;
	}
	return true;
}

// Java Deleteskill.java:59-64
void Deleteskill::apply(Player& admin, Player& player, int32_t skillId) {
	if (skillId != 0) {
		SkillLearnService::removeSkill(player, skillId);
		PacketSendUtility::sendMessage(admin, "You have successfully deleted the specified skill.");
	}
}

// Java Deleteskill.java:70-73
void Deleteskill::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///addcskill <skill name>");
}

// Java Deleteskill.java: JAXBUtil.deserialize(xml, SkillData.class) and SkillData.getSkillTemplate (the records of `./data/handlers/consolecommands/data/skills.xml`, element <skill>)
// parity= private static class SkillTemplate { private String id; private String name; public String getName() { return name; } public int getTemplateId() { return skillId; } private int skillId; public void setSkillId(int skillId) { this.skillId = skillId; } void afterUnmarshal(Unmarshaller u, Object parent) { setSkillId(Integer.parseInt(id)); } } private static class SkillData { private List<SkillTemplate> its; public SkillTemplate getSkillTemplate(String skill) { for (SkillTemplate it : getData()) { if (it.getName().toLowerCase().equals(skill.toLowerCase())) return it; } return null; } protected List<SkillTemplate> getData() { return its; }
std::optional<Deleteskill::SkillTemplate> Deleteskill::getSkillTemplate(std::string_view name) { // parity: the DOM record of the Java classes above
	std::unique_ptr<xml::XmlDocument> document = xml::XmlDocument::parseFile("./data/handlers/consolecommands/data/skills.xml"); // parity: the DOM record of the Java classes above
	pugi::xml_node first = document->root().child("skill"); // parity: the DOM record of the Java classes above
	if (!first) // Java: getData() is null without elements; the loop dereferences it // parity: the DOM record of the Java classes above
		throw runtime::NullPointerException("SkillData.getData()"); // parity: the DOM record of the Java classes above
	for (pugi::xml_node node : document->root().children("skill")) { // parity: the DOM record of the Java classes above
		SkillTemplate record; // parity: the DOM record of the Java classes above
		pugi::xml_attribute idAttribute = node.attribute("id"); // parity: the DOM record of the Java classes above
		record.id = commons::utils::parseInt(idAttribute ? idAttribute.value() : ""); // Java: @XmlID String id; afterUnmarshal: Integer.parseInt(id) // parity: the DOM record of the Java classes above
		if (pugi::xml_attribute nameAttribute = node.attribute("name")) // parity: the DOM record of the Java classes above
			record.name = nameAttribute.value(); // parity: the DOM record of the Java classes above
		if (!record.name) // Java: it.getName().toLowerCase() of a null name // parity: the DOM record of the Java classes above
			throw runtime::NullPointerException("SkillTemplate.getName()"); // parity: the DOM record of the Java classes above
		if (commons::utils::StringUtils::toLowerCase(*record.name) == commons::utils::StringUtils::toLowerCase(name)) // parity: the DOM record of the Java classes above
			return record; // parity: the DOM record of the Java classes above
	} // parity: the DOM record of the Java classes above
	return std::nullopt; // parity: the DOM record of the Java classes above
} // parity: the DOM record of the Java classes above

} // namespace aion::gameserver::handlers::consolecommands
