#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"

#include <string>
#include <vector>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CUSTOM_PACKET::PacketElement::PacketElement(SM_CUSTOM_PACKET::PacketElementType typeValue, std::string_view valueValue)
	: type(typeValue), value(valueValue) {
}

runtime::Ref<SM_CUSTOM_PACKET::PacketElement> SM_CUSTOM_PACKET::PacketElement::create(SM_CUSTOM_PACKET::PacketElementType typeValue,
	std::string_view valueValue) {
	return runtime::makeRef<SM_CUSTOM_PACKET::PacketElement>(typeValue, valueValue);
}

void SM_CUSTOM_PACKET::PacketElement::writeValue(SM_CUSTOM_PACKET& packet) {
	// Java: type.write(packet, value), the abstract method of each PacketElementType constant
	using commons::configuration::transformers::NumberParser::decodeInt;
	using commons::configuration::transformers::NumberParser::decodeLong;
	static_cast<void>(packet); // the write helpers are static and use the buffer of the running serialization
	switch (type) {
		case PacketElementType::D:
			SM_CUSTOM_PACKET::writeD(decodeInt(value));
			break;
		case PacketElementType::B: {
			const int32_t length = commons::utils::parseInt(value); // Java: Integer.valueOf(value)
			if (length < 0)
				throw commons::utils::IllegalArgumentException("Negative array size: " + std::to_string(length)); // Java: NegativeArraySizeException
			SM_CUSTOM_PACKET::writeB(std::vector<uint8_t>(static_cast<size_t>(length)));
			break;
		}
		case PacketElementType::H:
			SM_CUSTOM_PACKET::writeH(decodeInt(value));
			break;
		case PacketElementType::C:
			SM_CUSTOM_PACKET::writeC(decodeInt(value));
			break;
		case PacketElementType::F:
			SM_CUSTOM_PACKET::writeF(commons::configuration::transformers::NumberParser::parseFloat(value));
			break;
		case PacketElementType::DF:
			SM_CUSTOM_PACKET::writeDF(commons::configuration::transformers::NumberParser::parseDouble(value));
			break;
		case PacketElementType::Q:
			SM_CUSTOM_PACKET::writeQ(decodeLong(value));
			break;
		case PacketElementType::S:
			SM_CUSTOM_PACKET::writeS(value);
			break;
	}
}

SM_CUSTOM_PACKET::PacketElement::~PacketElement() = default;

SM_CUSTOM_PACKET::SM_CUSTOM_PACKET(int32_t opcode) : AionServerPacket(opcode) {
}

void SM_CUSTOM_PACKET::addElement(SM_CUSTOM_PACKET::PacketElement& packetElement) {
	elements.emplace_back(packetElement);
}

void SM_CUSTOM_PACKET::addElement(SM_CUSTOM_PACKET::PacketElementType type, std::string_view value) {
	elements.push_back(PacketElement::create(type, value));
}

SM_CUSTOM_PACKET::~SM_CUSTOM_PACKET() = default;

void SM_CUSTOM_PACKET::writeImpl(AionConnection* con) {
	for (const runtime::Ref<PacketElement>& el : elements) {
		el->writeValue(*this);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
