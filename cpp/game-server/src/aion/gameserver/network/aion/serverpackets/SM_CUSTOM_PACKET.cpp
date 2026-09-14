#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"

#include "aion/gameserver/runtime/base/Unported.h"
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
	AION_UNPORTED();
}

SM_CUSTOM_PACKET::PacketElement::~PacketElement() = default;

SM_CUSTOM_PACKET::SM_CUSTOM_PACKET(int32_t opcode) : AionServerPacket(opcode) {
}

void SM_CUSTOM_PACKET::addElement(SM_CUSTOM_PACKET::PacketElement& packetElement) {
	AION_UNPORTED();
}

void SM_CUSTOM_PACKET::addElement(SM_CUSTOM_PACKET::PacketElementType type, std::string_view value) {
	AION_UNPORTED();
}

SM_CUSTOM_PACKET::~SM_CUSTOM_PACKET() = default;

void SM_CUSTOM_PACKET::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
