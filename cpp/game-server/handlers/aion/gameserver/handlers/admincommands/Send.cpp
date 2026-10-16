#include "aion/gameserver/handlers/admincommands/Send.h"

#include <filesystem>
#include <memory>
#include <utility>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET_PacketElementTypeInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Send);

// parity= private static final String FOLDER = "./data/packets/"; private static final String SCHEMAFILE = FOLDER + "packets.xsd";

// Java Send.java:37-41
Send::Send()
	: AdminCommand("send", "Sends custom packets.",
		  "<file> - Sends packets to your client, based on the ./data/packets/<file>.xml template.\n") {
}

// Java Send.java:43-59
void Send::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	std::string fileName = params[0] + ".xml";
	std::string file = "./data/packets/" + fileName; // parity= File file = new File(FOLDER + fileName);

	if (!std::filesystem::is_regular_file(file)) { // parity= if (!file.isFile()) {
		sendInfo(admin, "File " + fileName + " not found");
		return;
	}

	Packets packetsTemplate = readPackets(file); // parity= Packets packetsTemplate = JAXBUtil.deserialize(file, Packets.class, SCHEMAFILE);
	send(admin, packetsTemplate);
}

// Java Send.java:61-87
void Send::send(Player& player, const Packets& packets) {
	using PacketElementType = network::aion::serverpackets::SM_CUSTOM_PACKET::PacketElementType; // parity: C++ using-declaration
	std::string senderObjectId = std::to_string(player.getObjectId()); // parity= String senderObjectId = String.valueOf(player.getObjectId());
	std::string targetObjectId = player.getTarget() != nullptr ? std::to_string(player.getTarget()->getObjectId()) : "0"; // parity= String targetObjectId = player.getTarget() != null ? String.valueOf(player.getTarget().getObjectId()) : "0";
	int64_t delay = 0;
	if (!packets.packets) // parity: Java's iterator() of a null list (no <packet> elements)
		throw runtime::NullPointerException("Packets.packets"); // parity: (the same)
	for (const Packet& packetTemplate : *packets.packets) { // parity= for (Packet packetTemplate : packets) {
		int32_t opcode = commons::configuration::transformers::NumberParser::decodeInt(packetTemplate.opcode); // parity= SM_CUSTOM_PACKET packet = new SM_CUSTOM_PACKET(packetTemplate.getOpcode());
		std::vector<std::pair<PacketElementType, std::string>> elements; // parity: the packet's elements, added below

		for (const Part& part : packetTemplate.parts) { // parity= for (Part part : packetTemplate.getParts()) {
			if (!part.type || part.type->empty()) // parity: Java's type.charAt(0) of a null or empty type
				throw runtime::NullPointerException("Part.type"); // parity: (the same)
			std::optional<PacketElementType> byCode = network::aion::serverpackets::getByCode(commons::utils::StringUtils::toUtf16(*part.type).front()); // parity= PacketElementType byCode = PacketElementType.getByCode(part.getType());

			if (!part.value) // parity: Java's value.contains of a null value
				throw runtime::NullPointerException("Part.value"); // parity: (the same)
			std::string value = *part.value; // parity= String value = part.getValue();

			if (value.find("${objectId}") != std::string::npos) // parity= if (value.contains("${objectId}"))
				value = commons::utils::StringUtils::replace(value, "${objectId}", senderObjectId); // parity= value = value.replace("${objectId}", senderObjectId);
			if (value.find("${targetObjectId}") != std::string::npos) // parity= if (value.contains("${targetObjectId}"))
				value = commons::utils::StringUtils::replace(value, "${targetObjectId}", targetObjectId); // parity= value = value.replace("${targetObjectId}", targetObjectId);

			for (int32_t i = 0; i < part.repeatCount; i++) // parity= for (int i = 0; i < part.getRepeatCount(); i++)
				// Java adds a null type and fails when the packet is written; C++ fails here (the packets before were scheduled)
				elements.emplace_back(byCode ? *byCode : throw runtime::NullPointerException("PacketElementType.getByCode"), value); // parity= packet.addElement(byCode, value);
		}

		delay += packetTemplate.delay; // parity= delay += packetTemplate.getDelay();

		runtime::Ref<Player> receiver(player); // parity: the lambda's capture of player
		utils::ThreadPoolManager::getInstance().schedule({&player}, [receiver, opcode, elements] { // parity= ThreadPoolManager.getInstance().schedule(() -> PacketSendUtility.sendPacket(player, packet), delay);
			network::aion::serverpackets::SM_CUSTOM_PACKET packet(opcode); // parity: (the same)
			for (const auto& [type, value] : elements) // parity: (the same)
				packet.addElement(type, value); // parity: (the same)
			PacketSendUtility::sendPacket(*receiver, packet); // parity: (the same)
		}, delay); // parity: (the same)

		delay += packets.delay; // parity= delay += packets.getDelay();
	}
}

// Java Send.java:89-177: Packets, Packet and Part as JAXB binds them (JAXBUtil.deserialize(file, Packets.class, SCHEMAFILE))
// parity= private long delay = -1; public long getDelay() { return delay; } public Iterator<Packet> iterator() { return packets.iterator(); } public String toString() { return "Packets" + "{delay=" + delay + ", packets=" + packets + '}'; } private Collection<Part> parts = new ArrayList<>(); private String opcode = "-1"; private long delay = 0; public int getOpcode() { return Integer.decode(opcode); } public Collection<Part> getParts() { return parts; } public long getDelay() { return delay; } public String toString() { return "Packet" + "{opcode=" + opcode + ", parts=" + parts + '}'; } private String type = null; private String value = null; private int repeatCount = 1; public char getType() { return type.charAt(0); } public String getValue() { return value; } public int getRepeatCount() { return repeatCount; } public String toString() { return "Part" + "{type='" + type + '\'' + ", value='" + value + '\'' + ", repeatCount=" + repeatCount + '}'; }
Send::Packets Send::readPackets(const std::string& file) { // parity: the DOM record of the Java classes above
	std::unique_ptr<xml::XmlDocument> document = xml::XmlDocument::parseFile(file); // parity: (the same)
	Packets packets; // parity: (the same)
	if (pugi::xml_attribute delay = document->root().attribute("delay")) // parity: (the same)
		packets.delay = commons::utils::parseLong(commons::utils::StringUtils::trim(delay.value())); // parity: (the same)
	for (pugi::xml_node packetNode : document->root().children("packet")) { // parity: (the same)
		if (!packets.packets) // parity: (the same)
			packets.packets.emplace(); // parity: (the same)
		Packet& packet = packets.packets->emplace_back(); // parity: (the same)
		if (pugi::xml_attribute opcode = packetNode.attribute("opcode")) // parity: (the same)
			packet.opcode = opcode.value(); // parity: (the same)
		if (pugi::xml_attribute delay = packetNode.attribute("delay")) // parity: (the same)
			packet.delay = commons::utils::parseLong(commons::utils::StringUtils::trim(delay.value())); // parity: (the same)
		for (pugi::xml_node partNode : packetNode.children("part")) { // parity: (the same)
			Part& part = packet.parts.emplace_back(); // parity: (the same)
			if (pugi::xml_attribute type = partNode.attribute("type")) // parity: (the same)
				part.type = type.value(); // parity: (the same)
			if (pugi::xml_attribute value = partNode.attribute("value")) // parity: (the same)
				part.value = value.value(); // parity: (the same)
			if (pugi::xml_attribute repeat = partNode.attribute("repeat")) // parity: (the same)
				part.repeatCount = commons::utils::parseInt(commons::utils::StringUtils::trim(repeat.value())); // parity: (the same)
		} // parity: (the same)
	} // parity: (the same)
	return packets; // parity: (the same)
} // parity: (the same)

} // namespace aion::gameserver::handlers::admincommands
