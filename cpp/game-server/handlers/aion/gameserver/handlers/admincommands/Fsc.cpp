#include "aion/gameserver/handlers/admincommands/Fsc.h"

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET_PacketElementTypeInfo.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Fsc);

// Java Fsc.java:26-28
Fsc::Fsc() : AdminCommand("fsc") {
}

// Java Fsc.java:30-48
void Fsc::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 3) { // parity= if (params.length < 3) {
		PacketSendUtility::sendMessage(admin, "Incorrent number of params in //fsc command");
		return;
	}

	int32_t id = commons::configuration::transformers::NumberParser::decodeInt(params[0]); // parity= int id = Integer.decode(params[0]);
	const std::string& format = params[1];

	SM_CUSTOM_PACKET packet(id); // parity= SM_CUSTOM_PACKET packet = new SM_CUSTOM_PACKET(id);

	size_t i = 0;
	for (char16_t c : commons::utils::StringUtils::toUtf16(format)) { // parity= for (char c : format.toCharArray()) {
		std::optional<SM_CUSTOM_PACKET::PacketElementType> type = network::aion::serverpackets::getByCode(c); // parity: (the addElement below)
		if (i + 2 >= params.size()) // parity: Java's ArrayIndexOutOfBoundsException of params[i + 2], explicit
			throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(i + 2) + " out of bounds for length " + std::to_string(params.size())); // parity: (the same)
		// Java: addElement(null, ...) for an unknown code fails when the packet is written (SM_CUSTOM_PACKET.writeImpl)
		packet.addElement(type.has_value() ? *type : throw runtime::NullPointerException("PacketElementType.getByCode"), params[i + 2]); // parity= packet.addElement(PacketElementType.getByCode(c), params[i + 2]);
		i++;
	}
	PacketSendUtility::sendPacket(admin, packet);
}

// Java Fsc.java:50-53
void Fsc::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Incorrent number of params in //fsc command");
}

} // namespace aion::gameserver::handlers::admincommands
