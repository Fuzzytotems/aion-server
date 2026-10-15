#include "aion/gameserver/handlers/consolecommands/Teleport_to_named.h"

#include <memory>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Teleport_to_named);

// Java Teleport_to_named.java:20-22
Teleport_to_named::Teleport_to_named() : ConsoleCommand("teleport_to_named") {
}

// Java Teleport_to_named.java:24-41
void Teleport_to_named::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	std::string npcName = params[0];

	std::optional<NpcTemplate> npcTemplate = getNpcTemplate(npcName); // parity= File xml = new File("./data/handlers/consolecommands/data/npcs.xml"); NpcData data = JAXBUtil.deserialize(xml, NpcData.class); NpcTemplate npcTemplate = data.getNpcTemplate(npcName);

	if (npcTemplate) { // parity= if (npcTemplate != null) {
		PacketSendUtility::sendMessage(admin, "Teleporting to Npc: " + std::to_string(npcTemplate->id)); // parity= PacketSendUtility.sendMessage(admin, "Teleporting to Npc: " + npcTemplate.getTemplateId());
		TeleportService::teleportToNpc(admin, npcTemplate->id); // parity= TeleportService.teleportToNpc(admin, npcTemplate.getTemplateId());
	}
}

// Java Teleport_to_named.java:43-46
void Teleport_to_named::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///teleport_to_named <named name>");
}

// Java Teleport_to_named.java: JAXBUtil.deserialize(xml, NpcData.class) and NpcData.getNpcTemplate (the records of `./data/handlers/consolecommands/data/npcs.xml`, element <npc>)
// parity= private static class NpcTemplate { private String id; private String name; public String getName() { return name; } public int getTemplateId() { return npcId; } private int npcId; public void setNpcId(int npcId) { this.npcId = npcId; } void afterUnmarshal(Unmarshaller u, Object parent) { setNpcId(Integer.parseInt(id)); } } private static class NpcData { private List<NpcTemplate> its; public NpcTemplate getNpcTemplate(String npc) { for (NpcTemplate it : getData()) { if (it.getName().toLowerCase().equals(npc.toLowerCase())) return it; } return null; } protected List<NpcTemplate> getData() { return its; }
std::optional<Teleport_to_named::NpcTemplate> Teleport_to_named::getNpcTemplate(std::string_view name) { // parity: the DOM record of the Java classes above
	std::unique_ptr<xml::XmlDocument> document = xml::XmlDocument::parseFile("./data/handlers/consolecommands/data/npcs.xml"); // parity: the DOM record of the Java classes above
	pugi::xml_node first = document->root().child("npc"); // parity: the DOM record of the Java classes above
	if (!first) // Java: getData() is null without elements; the loop dereferences it // parity: the DOM record of the Java classes above
		throw runtime::NullPointerException("NpcData.getData()"); // parity: the DOM record of the Java classes above
	for (pugi::xml_node node : document->root().children("npc")) { // parity: the DOM record of the Java classes above
		NpcTemplate record; // parity: the DOM record of the Java classes above
		pugi::xml_attribute idAttribute = node.attribute("id"); // parity: the DOM record of the Java classes above
		record.id = commons::utils::parseInt(idAttribute ? idAttribute.value() : ""); // Java: @XmlID String id; afterUnmarshal: Integer.parseInt(id) // parity: the DOM record of the Java classes above
		if (pugi::xml_attribute nameAttribute = node.attribute("name")) // parity: the DOM record of the Java classes above
			record.name = nameAttribute.value(); // parity: the DOM record of the Java classes above
		if (!record.name) // Java: it.getName().toLowerCase() of a null name // parity: the DOM record of the Java classes above
			throw runtime::NullPointerException("NpcTemplate.getName()"); // parity: the DOM record of the Java classes above
		if (commons::utils::StringUtils::toLowerCase(*record.name) == commons::utils::StringUtils::toLowerCase(name)) // parity: the DOM record of the Java classes above
			return record; // parity: the DOM record of the Java classes above
	} // parity: the DOM record of the Java classes above
	return std::nullopt; // parity: the DOM record of the Java classes above
} // parity: the DOM record of the Java classes above

} // namespace aion::gameserver::handlers::consolecommands
