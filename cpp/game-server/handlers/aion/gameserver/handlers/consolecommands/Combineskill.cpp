#include "aion/gameserver/handlers/consolecommands/Combineskill.h"

#include <memory>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Combineskill);

// Java Combineskill.java:19-21
Combineskill::Combineskill() : ConsoleCommand("combineskill") {
}

// Java Combineskill.java:23-54
void Combineskill::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	std::string skillName = params[0];
	int32_t skillId = 0;
	int32_t skillLvl = 1;

	try {
		skillLvl = commons::utils::parseInt(params[1]); // parity= skillLvl = Integer.parseInt(params[1]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "Parameters need to be an integer.");
		return;
	}

	std::optional<SkillTemplate> skillTemplate = getSkillTemplate(skillName); // parity= File xml = new File("./data/handlers/consolecommands/data/skills.xml"); SkillData data = JAXBUtil.deserialize(xml, SkillData.class); SkillTemplate skillTemplate = data.getSkillTemplate(skillName);

	if (skillTemplate) // parity= if (skillTemplate != null)
		skillId = skillTemplate->id; // parity= skillId = skillTemplate.getTemplateId();

	if (skillId > 0) {
		player.getSkillList()->addSkill(player, skillId, skillLvl);
		PacketSendUtility::sendMessage(admin, "You have success add skill");
	}
}

// Java Combineskill.java:56-59
void Combineskill::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///addcskill <skill name>");
}

// Java Combineskill.java: JAXBUtil.deserialize(xml, SkillData.class) and SkillData.getSkillTemplate (the records of `./data/handlers/consolecommands/data/skills.xml`, element <skill>)
// parity= private static class SkillTemplate { private String id; private String name; public String getName() { return name; } public int getTemplateId() { return skillId; } private int skillId; public void setSkillId(int skillId) { this.skillId = skillId; } void afterUnmarshal(Unmarshaller u, Object parent) { setSkillId(Integer.parseInt(id)); } } private static class SkillData { private List<SkillTemplate> its; public SkillTemplate getSkillTemplate(String skill) { for (SkillTemplate it : getData()) { if (it.getName().toLowerCase().equals(skill.toLowerCase())) return it; } return null; } protected List<SkillTemplate> getData() { return its; }
std::optional<Combineskill::SkillTemplate> Combineskill::getSkillTemplate(std::string_view name) { // parity: the DOM record of the Java classes above
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
