#include "aion/gameserver/network/aion/AionClientPacketFactory.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.AionClientPacketFactory");

namespace {

using State = AionConnection_State;
using configs::network::NetworkConfig;

#define AION_CLIENT_PACKET_TABLE_SIZE(size) constexpr size_t PACKET_TABLE_SIZE = size;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
#undef AION_CLIENT_PACKET_TABLE_SIZE

/** Java: packets = new PacketInfo<?>[250]; a slot without a packet has an empty class name */
using PacketTable = std::array<AionClientPacketFactory::PacketInfo, PACKET_TABLE_SIZE>;

/** Java: the static initializer - one slot per packet of ClientPacketInfo.gen.inc, with the factory of its AION_CLIENT_PACKET marker */
const PacketTable* buildTable(std::span<const handlers::ClientPacketEntry> entries) {
	auto* table = new PacketTable(); // leaked: published tables are never freed (setEntries)
	using enum AionConnection_State; // the state names of ClientPacketInfo.gen.inc
	auto add = [table, entries](int32_t opcode, std::string_view name, const StateSet& states) {
		AionClientPacketFactory::PacketInfo& info = (*table)[static_cast<size_t>(opcode)];
		info.packetClassName = name;
		info.validStates = states;
		if (const handlers::ClientPacketEntry* entry = handlers::findClientPacket(entries, name))
			info.packetFactory = entry->create;
	};
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) add(opcode, #Class, StateSet{__VA_ARGS__});
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	return table;
}

// lint: L14 the published immutable table (network internals, never a game object)
std::atomic<const PacketTable*> publishedTable{nullptr};
std::mutex tableMutex;

const PacketTable& packetTable() {
	if (const PacketTable* table = publishedTable.load(std::memory_order_acquire))
		return *table;
	std::lock_guard lock(tableMutex);
	if (const PacketTable* table = publishedTable.load(std::memory_order_acquire))
		return *table;
	const PacketTable* table = buildTable(handlers::clientPacketEntries());
	publishedTable.store(table, std::memory_order_release);
	return *table;
}

/** @return true the first time a class name is passed (the "not ported yet" warning is logged once per class) */
bool firstUnportedUse(std::string_view packetClassName) {
	static std::mutex mutex;
	static auto* reported = new std::unordered_set<std::string_view>(); // leaked immortal
	std::lock_guard lock(mutex);
	return reported->insert(packetClassName).second;
}

} // namespace

std::unique_ptr<AionClientPacket> AionClientPacketFactory::PacketInfo::newPacket(int32_t opCode, const commons::utils::ByteBuffer& buffer,
	AionConnection* con) const {
	std::unique_ptr<AionClientPacket> packet = packetFactory(opCode, validStates);
	packet->setBuffer(buffer);
	packet->setConnection(con->sharedFromThis());
	return packet;
}

std::unique_ptr<AionClientPacket> AionClientPacketFactory::tryCreatePacket(commons::utils::ByteBuffer& data, AionConnection* client) {
	State state = client->getState();
	int32_t opcode = Crypt::decodeClientPacketOpcode(data.getShort() & 0xffff);
	data.position(data.position() + 3); // skip static code (short) and secondary opcode (byte)
	const PacketInfo* packetInfo = getPacketInfo(opcode);
	if (packetInfo == nullptr) {
		client->sendUnknownClientPacketInfo(opcode);
		if (NetworkConfig::LOG_UNKNOWN_PACKETS.load())
			log.warn(fmt::format("Aion client sent data with unknown opcode: 0x{:03X}, state={} \n{}", static_cast<uint32_t>(opcode), xml::enumName(state),
				commons::utils::NetworkUtils::toHex(data)));
		return nullptr;
	}
	if (!packetInfo->isValid(state)) {
		if (NetworkConfig::LOG_IGNORED_PACKETS.load())
			log.warn(client->toString() + " sent " + std::string(packetInfo->getPacketClassName()) + " but the connections current state (" +
				std::string(xml::enumName(state)) + ") is invalid for this packet. Packet won't be instantiated.");
		return nullptr;
	}
	if (packetInfo->packetFactory == nullptr) { // C++ only: the packet class is not ported yet
		if (firstUnportedUse(packetInfo->getPacketClassName()))
			log.warn(client->toString() + " sent " + std::string(packetInfo->getPacketClassName()) + ", which is not ported yet. Packet won't be instantiated.");
		return nullptr;
	}
	return packetInfo->newPacket(opcode, data, client);
}

const AionClientPacketFactory::PacketInfo* AionClientPacketFactory::getPacketInfo(int32_t opcode) {
	if (opcode < 0 || static_cast<size_t>(opcode) >= PACKET_TABLE_SIZE)
		return nullptr;
	const PacketInfo& info = packetTable()[static_cast<size_t>(opcode)];
	return info.packetClassName.empty() ? nullptr : &info;
}

void AionClientPacketFactory::setEntries(std::span<const handlers::ClientPacketEntry> entries) {
	std::lock_guard lock(tableMutex);
	publishedTable.store(buildTable(entries), std::memory_order_release);
}

} // namespace aion::gameserver::network::aion
