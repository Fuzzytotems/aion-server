#include "aion/gameserver/handlers/consolecommands/Addskill.h"

#include <memory>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Addskill);

// Java Addskill.java:18-20
Addskill::Addskill() : ConsoleCommand("addskill") {
}

// Java Addskill.java:22-45
void Addskill::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	std::string skillName = params[0];
	int32_t skillId = 0;

	std::optional<SkillTemplate> skillTemplate = getSkillTemplate(skillName); // parity= File xml = new File("./data/handlers/consolecommands/data/skills.xml"); SkillData data = JAXBUtil.deserialize(xml, SkillData.class); SkillTemplate skillTemplate = data.getSkillTemplate(skillName);

	if (skillTemplate) // parity= if (skillTemplate != null)
		skillId = skillTemplate->id; // parity= skillId = skillTemplate.getSkillId();

	if (skillId > 0) {
		player.getSkillList()->addSkill(player, skillId, 1);
		PacketSendUtility::sendMessage(admin, "You have success add skill");
	}
}

// Java Addskill.java:47-50
void Addskill::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///addskill <skill name>");
}

// Java Addskill.java: JAXBUtil.deserialize(xml, SkillData.class) and SkillData.getSkillTemplate (the records of `./data/handlers/consolecommands/data/skills.xml`, element <skill>)
// parity= private static class SkillTemplate { private int skillId; private String name; public String getName() { return name; } public int getSkillId() { return skillId; } } private static class SkillData { private List<SkillTemplate> its; public SkillTemplate getSkillTemplate(String skill) { for (SkillTemplate it : getData()) { if (it.getName().toLowerCase().equals(skill.toLowerCase())) return it; } return null; } protected List<SkillTemplate> getData() { return its; }
std::optional<Addskill::SkillTemplate> Addskill::getSkillTemplate(std::string_view name) { // parity: the DOM record of the Java classes above
	std::unique_ptr<xml::XmlDocument> document = xml::XmlDocument::parseFile("./data/handlers/consolecommands/data/skills.xml"); // parity: the DOM record of the Java classes above
	pugi::xml_node first = document->root().child("skill"); // parity: the DOM record of the Java classes above
	if (!first) // Java: getData() is null without elements; the loop dereferences it // parity: the DOM record of the Java classes above
		throw runtime::NullPointerException("SkillData.getData()"); // parity: the DOM record of the Java classes above
	for (pugi::xml_node node : document->root().children("skill")) { // parity: the DOM record of the Java classes above
		SkillTemplate record; // parity: the DOM record of the Java classes above
		pugi::xml_attribute idAttribute = node.attribute("id"); // parity: the DOM record of the Java classes above
		record.id = idAttribute ? commons::utils::parseInt(commons::utils::StringUtils::trim(idAttribute.value())) : 0; // Java: @XmlAttribute int skillId // parity: the DOM record of the Java classes above
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
