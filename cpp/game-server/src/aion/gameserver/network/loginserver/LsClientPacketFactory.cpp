#include "aion/gameserver/network/loginserver/LsClientPacketFactory.h"

#include <array>
#include <string>

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/network/loginserver/LoginServerConnection.h"
#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_ACCOUNT_AUTH_RESPONSE.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_ACCOUNT_RECONNECT_KEY.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_BAN_RESPONSE.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_GS_AUTH_RESPONSE.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_GS_CHARACTER_RESPONSE.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_HDD_BANLIST.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_LS_CONTROL_RESPONSE.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_LS_PING.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_MACBAN_LIST.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_PTRANSFER_RESPONSE.h"
#include "aion/gameserver/network/loginserver/clientpackets/CM_REQUEST_KICK_ACCOUNT.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.LsClientPacketFactory");

namespace {

using State = LoginServerConnection_State;

template <class Packet>
std::unique_ptr<LsClientPacket> create(int32_t opcode) {
	return std::make_unique<Packet>(opcode);
}

constexpr uint32_t stateBit(State state) noexcept {
	return uint32_t{1} << static_cast<uint32_t>(state);
}

struct Slot {
	int32_t opcode;
	LsClientPacketFactory::PacketInfo info;
};

// Java: the static initializer (packets.put(opcode, new PacketInfo<>(CM_X.class, states...)))
const std::array<Slot, 11> PACKETS{{
	{0x00, {"CM_GS_AUTH_RESPONSE", &create<clientpackets::CM_GS_AUTH_RESPONSE>, stateBit(State::CONNECTED)}},
	{0x01, {"CM_ACCOUNT_AUTH_RESPONSE", &create<clientpackets::CM_ACCOUNT_AUTH_RESPONSE>, stateBit(State::AUTHED)}},
	{0x02, {"CM_REQUEST_KICK_ACCOUNT", &create<clientpackets::CM_REQUEST_KICK_ACCOUNT>, stateBit(State::AUTHED)}},
	{0x03, {"CM_ACCOUNT_RECONNECT_KEY", &create<clientpackets::CM_ACCOUNT_RECONNECT_KEY>, stateBit(State::AUTHED)}},
	{0x04, {"CM_LS_CONTROL_RESPONSE", &create<clientpackets::CM_LS_CONTROL_RESPONSE>, stateBit(State::AUTHED)}},
	{0x05, {"CM_BAN_RESPONSE", &create<clientpackets::CM_BAN_RESPONSE>, stateBit(State::AUTHED)}},
	{0x08, {"CM_GS_CHARACTER_RESPONSE", &create<clientpackets::CM_GS_CHARACTER_RESPONSE>, stateBit(State::AUTHED)}},
	{0x09, {"CM_MACBAN_LIST", &create<clientpackets::CM_MACBAN_LIST>, stateBit(State::AUTHED)}},
	{0x0A, {"CM_HDD_BANLIST", &create<clientpackets::CM_HDD_BANLIST>, stateBit(State::AUTHED)}},
	{0x0B, {"CM_LS_PING", &create<clientpackets::CM_LS_PING>, stateBit(State::AUTHED)}},
	{0x0C, {"CM_PTRANSFER_RESPONSE", &create<clientpackets::CM_PTRANSFER_RESPONSE>, stateBit(State::AUTHED)}},
}};

const LsClientPacketFactory::PacketInfo* findPacket(int32_t opCode) {
	for (const Slot& slot : PACKETS) {
		if (slot.opcode == opCode)
			return &slot.info;
	}
	return nullptr;
}

} // namespace

std::unique_ptr<LsClientPacket> LsClientPacketFactory::PacketInfo::newPacket(int32_t opCode, const commons::utils::ByteBuffer& buffer,
	LoginServerConnection* con) const {
	std::unique_ptr<LsClientPacket> packet = packetFactory(opCode);
	packet->setBuffer(buffer);
	packet->setConnection(con->sharedFromThis());
	return packet;
}

std::unique_ptr<LsClientPacket> LsClientPacketFactory::tryCreatePacket(commons::utils::ByteBuffer& data, LoginServerConnection* client) {
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

} // namespace aion::gameserver::network::loginserver
