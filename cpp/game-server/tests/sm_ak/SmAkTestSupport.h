#pragma once

// Shared helpers of the P4-16 server packet tests (SM_A* to SM_K*): a little endian byte builder for the expected packet bodies, written by hand
// from the Java writeImpl methods (BaseServerPacket: writeD/H/C/Q/F/DF little endian, writeS UTF-16LE plus a 0 char, writeS(text, n) n chars
// plus a 0 char), the serialization of a packet with its opcode header check, a real AionConnection (unregistered, on a loopback socket) for
// the PER_RECIPIENT packets, and the real Player on stat container doubles (the PlayerGameStats and PlayerLifeStats constructors belong to
// P5-01), as the controllers and DAO tests create it.

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/utils/InetSocketAddress.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets::test {

/** The expected body of a packet, written like Java's BaseServerPacket write methods (little endian) */
class Bytes {
public:
	Bytes& D(int32_t value) { return put(static_cast<uint32_t>(value), 4); }
	Bytes& H(int32_t value) { return put(static_cast<uint32_t>(value), 2); }
	Bytes& C(int32_t value) { return put(static_cast<uint32_t>(value), 1); }
	Bytes& Q(int64_t value) {
		put(static_cast<uint32_t>(static_cast<uint64_t>(value)), 4);
		return put(static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32), 4);
	}
	Bytes& F(float value) { return put(std::bit_cast<uint32_t>(value), 4); }
	Bytes& DF(double value) { return Q(std::bit_cast<int64_t>(value)); }
	/** Java writeS(text): the UTF-16 chars and a terminating 0 char (null writes only the 0 char) */
	Bytes& S(std::u16string_view text) {
		for (char16_t c : text)
			H(c);
		return H(0);
	}
	Bytes& S(std::string_view text) { return S(std::u16string_view(commons::utils::StringUtils::toUtf16(text))); }
	/** Java writeS(text, fixedLength): fixedLength chars (truncated or zero padded) and a terminating 0 char */
	Bytes& S(std::string_view text, int32_t fixedLength) {
		std::u16string utf16 = commons::utils::StringUtils::toUtf16(text);
		for (int32_t i = 0; i < fixedLength; i++)
			H(i < static_cast<int32_t>(utf16.size()) ? utf16[static_cast<size_t>(i)] : 0);
		return H(0);
	}
	Bytes& B(std::span<const uint8_t> bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
	/** writeB(new byte[count]) */
	Bytes& zeros(size_t count) {
		data.insert(data.end(), count, uint8_t{0});
		return *this;
	}
	Bytes& append(const std::vector<uint8_t>& bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}

	std::vector<uint8_t> data;

private:
	Bytes& put(uint32_t value, int count) {
		for (int i = 0; i < count; i++)
			data.push_back(static_cast<uint8_t>(value >> (8 * i)));
		return *this;
	}
};

/**
 * Serializes the packet (for `con`, nullptr for a SHARED serialization) and returns the writeImpl data after the 5 byte header, which is checked
 * against the packet's opcode (AionServerPacket.writeOP: [H (opcode + 207) ^ 0xDF][C 0x44][H ~wire opcode]).
 */
inline std::vector<uint8_t> dataOf(AionServerPacket& packet, AionConnection* con = nullptr) {
	SerializedBody body = packet.serialize(con);
	const std::vector<uint8_t>& bytes = *body.bytes;
	EXPECT_GE(bytes.size(), 5u);
	if (bytes.size() < 5)
		return {};
	const uint16_t wire = static_cast<uint16_t>(Crypt::encodeServerPacketOpcode(packet.getOpCode()));
	EXPECT_EQ(static_cast<uint16_t>(bytes[0] | bytes[1] << 8), wire);
	EXPECT_EQ(bytes[2], 0x44);
	EXPECT_EQ(static_cast<uint16_t>(bytes[3] | bytes[4] << 8), static_cast<uint16_t>(~wire));
	EXPECT_EQ(body.opCode, packet.getOpCode());
	return std::vector<uint8_t>(bytes.begin() + 5, bytes.end());
}

/** Temporary packets: `dataOf(SM_X(...))` */
template <std::derived_from<AionServerPacket> P>
std::vector<uint8_t> dataOf(P&& packet, AionConnection* con = nullptr) {
	return dataOf(static_cast<AionServerPacket&>(packet), con);
}

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class TestGameStats final : public model::stats::container::CreatureGameStats {
public:
	explicit TestGameStats(model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** CreatureLifeStats double with fixed maximum HP and MP */
class TestLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit TestLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** The real Player with the stat container doubles */
class TestPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// PlayerGameStats(Player&) is P5-01: everything before it ran
		}
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

struct PlayerFixture {
	runtime::Ref<model::account::Account> account;
	runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData;
	runtime::Ref<TestPlayer> player;
};

/** Java PlayerService.getPlayer: account, common data, appearance, account data and the account warehouse, then create<Player> */
inline PlayerFixture makePlayer(int32_t objectId, int32_t accountId, std::string_view name) {
	PlayerFixture f;
	f.account = model::account::Account::create(accountId);
	f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
	f.commonData->setName(name);
	f.commonData->setRace(model::Race::ELYOS);
	runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
	f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
	f.account->setAccountWarehouse(
		std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
	f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
	return f;
}

/**
 * A real AionConnection on a connected loopback socket of a never destroyed NioServer. It is not registered with the server, so no IO runs on it:
 * the tests only read its account and active player while serializing PER_RECIPIENT packets.
 */
class TestConnection {
public:
	TestConnection() : acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
		asio::ip::tcp::socket socket = server().openSocket(commons::utils::InetSocketAddress{"127.0.0.1", acceptor.local_endpoint().port()});
		connection = std::make_shared<AionConnection>(std::move(socket), server());
	}

	AionConnection* get() const { return connection.get(); }
	AionConnection* operator->() const { return connection.get(); }

private:
	static commons::network::NioServer& server() {
		static auto* created = [] {
			auto* nioServer = new commons::network::NioServer(1, std::vector<commons::network::ServerCfg>{});
			nioServer->connect();
			return nioServer;
		}();
		return *created;
	}

	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	std::shared_ptr<AionConnection> connection;
};

/** Runs every test in a TEST task scope with a deterministic scheduler backend and fresh object ids */
class PacketTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 17));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	void TearDown() override {
		scope.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
	std::unique_ptr<runtime::TaskScope> scope;
};

} // namespace aion::gameserver::network::aion::serverpackets::test
