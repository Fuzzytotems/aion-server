#include "aion/gameserver/network/chatserver/CsClientPacketFactory.h"

#include <array>
#include <string>

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/network/chatserver/ChatServerConnection.h"
#include "aion/gameserver/network/chatserver/CsClientPacket.h"
#include "aion/gameserver/network/chatserver/clientpackets/CM_CS_AUTH_RESPONSE.h"
#include "aion/gameserver/network/chatserver/clientpackets/CM_CS_PLAYER_AUTH_RESPONSE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::chatserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.chatserver.CsClientPacketFactory");

namespace {

using State = ChatServerConnection_State;

template <class Packet>
std::unique_ptr<CsClientPacket> create(int32_t opcode) {
	return std::make_unique<Packet>(opcode);
}

constexpr uint32_t stateBit(State state) noexcept {
	return uint32_t{1} << static_cast<uint32_t>(state);
}

struct Slot {
	int32_t opcode;
	CsClientPacketFactory::PacketInfo info;
};

// Java: the static initializer
const std::array<Slot, 2> PACKETS{{
	{0x00, {"CM_CS_AUTH_RESPONSE", &create<clientpackets::CM_CS_AUTH_RESPONSE>, stateBit(State::CONNECTED)}},
	{0x01, {"CM_CS_PLAYER_AUTH_RESPONSE", &create<clientpackets::CM_CS_PLAYER_AUTH_RESPONSE>, stateBit(State::AUTHED)}},
}};

const CsClientPacketFactory::PacketInfo* findPacket(int32_t opCode) {
	for (const Slot& slot : PACKETS) {
		if (slot.opcode == opCode)
			return &slot.info;
	}
	return nullptr;
}

} // namespace

std::unique_ptr<CsClientPacket> CsClientPacketFactory::PacketInfo::newPacket(int32_t opCode, const commons::utils::ByteBuffer& buffer,
	ChatServerConnection* con) const {
	std::unique_ptr<CsClientPacket> packet = packetFactory(opCode);
	packet->setBuffer(buffer);
	packet->setConnection(con->sharedFromThis());
	return packet;
}

std::unique_ptr<CsClientPacket> CsClientPacketFactory::tryCreatePacket(commons::utils::ByteBuffer& data, ChatServerConnection* client) {
	State state = client->getState();
	int32_t opCode = data.get() & 0xff;
	const PacketInfo* packetInfo = findPacket(opCode);
	if (packetInfo == nullptr) {
		log.warn(fmt::format("{} sent data with unknown opcode: 0x{:02X}, state={} \n{}", client->toString(), opCode, xml::enumName(state),
			commons::utils::NetworkUtils::toHex(data)));
		return nullptr;
	}
	if (!packetInfo->isValid(state)) {
		log.warn(client->toString() + " sent " + std::string(packetInfo->getPacketClassName()) + " but the connections current state (" +
			std::string(xml::enumName(state)) + ") is invalid for this packet. Packet won't be instantiated.");
		return nullptr;
	}
	return packetInfo->newPacket(opCode, data, client);
}

} // namespace aion::gameserver::network::chatserver
