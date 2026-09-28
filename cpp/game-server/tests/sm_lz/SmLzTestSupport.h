#pragma once

// Shared support of the P4-17 packet tests (server packets L-Z and the Abstract* bases):
// - Bytes: the expected body written by hand from a Java writeImpl, in Java's little endian BaseServerPacket encoding, with the opcode header of
//   AionServerPacket.writeOP (Crypt.encodeServerPacketOpcode: (opcode + SM_VERSION_CHECK.INTERNAL_VERSION) ^ 0xDF, 0x44, ~encoded).
// - Probe<P>: a packet whose writeImpl stops at the first AION_UNPORTED body of another chunk and keeps the bytes written before it, so a golden
//   prefix can be checked while the model bodies (the stat calculation of P5-01, most services) are missing.
// - PacketTest: a deterministic scheduler, IDFactory and Npc skill data like the controller tests, real Players (stat container doubles, the
//   PlayerGameStats constructor is P5-01) and Npcs, packet lookups (detail/PacketLookups.h) and a real AionConnection for PER_RECIPIENT packets.

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/utils/InetSocketAddress.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets::testing {

#define PACKET_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

/** Expected packet bytes, written like Java's BaseServerPacket/AionServerPacket (little endian) */
class Bytes {
public:
	Bytes& C(int32_t value) {
		data.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	Bytes& H(int32_t value) { return C(value).C(value >> 8); }
	Bytes& D(int32_t value) { return H(value).H(value >> 16); }
	Bytes& Q(int64_t value) { return D(static_cast<int32_t>(value)).D(static_cast<int32_t>(value >> 32)); }
	Bytes& F(float value) { return D(std::bit_cast<int32_t>(value)); }
	/** Java writeS(String): the UTF-16 code units and a terminating 0 char */
	Bytes& S(std::string_view text) {
		for (char16_t c : commons::utils::StringUtils::wtf8ToUtf16(text))
			H(c);
		return H(0);
	}
	/** Java AionServerPacket.writeS(String, fixedLength): zero bytes for an empty text, else fixedLength chars (truncated or 0-padded) and 0 */
	Bytes& S(std::string_view text, int32_t fixedLength) {
		if (text.empty())
			return zeros((fixedLength + 1) * 2);
		std::u16string utf16 = commons::utils::StringUtils::wtf8ToUtf16(text);
		for (int32_t i = 0; i < fixedLength; i++)
			H(static_cast<size_t>(i) < utf16.size() ? utf16[static_cast<size_t>(i)] : 0);
		return H(0);
	}
	/** Java writeDyeInfo(Integer rgb) */
	Bytes& dye(std::optional<int32_t> rgb) {
		if (!rgb)
			return zeros(4);
		return C(1).C((*rgb & 0xFF0000) >> 16).C((*rgb & 0xFF00) >> 8).C(*rgb & 0xFF);
	}
	Bytes& zeros(int32_t count) {
		data.insert(data.end(), static_cast<size_t>(count), uint8_t{0});
		return *this;
	}
	Bytes& raw(const std::vector<uint8_t>& bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
	/** AionServerPacket.writeOP for the Java opcode of ServerPacketsOpcodes */
	Bytes& header(int32_t javaOpcode) {
		int32_t encoded = (javaOpcode + 207) ^ 0xDF;
		return H(encoded).C(0x44).H(~encoded);
	}

	std::vector<uint8_t> data;
};

/** One serialization of the packet (a SHARED serialization without a connection, or for the given recipient) */
inline std::vector<uint8_t> serialized(AionServerPacket& packet, AionConnection* con = nullptr) {
	return *packet.serialize(con).bytes;
}

/** A failure message naming the first differing byte offset (gtest prints only the first bytes of long vectors) */
inline std::string firstDifference(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected) {
	size_t i = 0;
	while (i < actual.size() && i < expected.size() && actual[i] == expected[i])
		i++;
	if (i == actual.size() && i == expected.size())
		return "equal";
	auto byteAt = [](const std::vector<uint8_t>& v, size_t index) { return index < v.size() ? std::to_string(v[index]) : std::string("end"); };
	return "first difference at offset " + std::to_string(i) + ": actual " + byteAt(actual, i) + ", expected " + byteAt(expected, i) + " (sizes " +
		std::to_string(actual.size()) + " and " + std::to_string(expected.size()) + ")";
}

#define EXPECT_BYTES(actual, expected)                                                                                                                \
	do {                                                                                                                                              \
		const std::vector<uint8_t> actualBytes = (actual);                                                                                            \
		const std::vector<uint8_t> expectedBytes = (expected);                                                                                        \
		EXPECT_EQ(actualBytes, expectedBytes) << ::aion::gameserver::network::aion::serverpackets::testing::firstDifference(actualBytes, expectedBytes); \
	} while (false)

/** serialized() of a packet temporary */
inline std::vector<uint8_t> serialized(AionServerPacket&& packet, AionConnection* con = nullptr) {
	return *packet.serialize(con).bytes;
}

/** A packet whose writeImpl stops at the first unported body of another chunk; serialize() then returns the bytes written before it */
template <class P>
class Probe final : public P {
public:
	using P::P;

	/** The message of the UnportedException that stopped the last serialization (the function name), empty if it completed */
	std::string unportedAt;

protected:
	void writeImpl(AionConnection* con) override {
		unportedAt.clear();
		try {
			P::writeImpl(con);
		} catch (const runtime::UnportedException& e) {
			unportedAt = e.what();
		}
	}
};

/** Installs packet lookups for the lifetime of the guard */
class LookupsGuard {
public:
	explicit LookupsGuard(const detail::PacketLookupsForTests& lookups) { detail::setPacketLookupsForTests(&lookups); }
	~LookupsGuard() { detail::setPacketLookupsForTests(nullptr); }
	LookupsGuard(const LookupsGuard&) = delete;
	LookupsGuard& operator=(const LookupsGuard&) = delete;
};

/** Life stats with fixed HP and MP (NpcLifeStats and PlayerLifeStats read the stat calculation of P5-01) */
class FixedLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** Game stats of a player without the P5-01 calculation */
class TestPlayerGameStats final : public model::stats::container::CreatureGameStats {
public:
	explicit TestPlayerGameStats(model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** The real Player; only the stat containers are doubles (the PlayerGameStats constructor is P5-01) */
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
		setGameStats(std::make_unique<TestPlayerGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

struct PlayerFixture {
	runtime::Ref<model::account::Account> account;
	runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData;
	runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance;
	runtime::Ref<TestPlayer> player;
};

/** Java PlayerService.getPlayer: account, common data, appearance, account data and the account warehouse, then create<Player> */
inline PlayerFixture makePlayer(int32_t objectId, int32_t accountId, std::string_view name, model::Race race = model::Race::ELYOS) {
	PlayerFixture f;
	f.account = model::account::Account::create(accountId);
	f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
	f.commonData->setName(name);
	f.commonData->setRace(race);
	f.appearance = model::gameobjects::player::PlayerAppearance::create();
	f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *f.appearance));
	f.account->setAccountWarehouse(
		std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
	f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
	return f;
}

/** An Npc with the real constructor and postConstruct chain; only the stat containers are doubles */
class TestNpc final : public model::gameobjects::Npc {
	AION_MAKE_REF_FRIEND
public:
	TestNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, model::templates::spawns::SpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~TestNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

class TestSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	TestSpawnTemplate(model::templates::spawns::SpawnGroup& group, int32_t staticId)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, staticId, 0, std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them */
inline const model::templates::npc::NpcTemplate* npcTemplate(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context, std::string(xml)).release();
}

/** A real AionConnection (a loopback socket, no IO started) for the PER_RECIPIENT packets, which read its active player */
class TestConnection {
public:
	TestConnection() : acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
		server.connect();
		commons::utils::InetSocketAddress address{"127.0.0.1", acceptor.local_endpoint().port()};
		connection = std::make_shared<AionConnection>(server.openSocket(address), server);
	}
	~TestConnection() {
		if (connection)
			connection->setActivePlayer(nullptr);
		connection.reset();
		server.shutdown(std::chrono::seconds(2));
	}
	TestConnection(const TestConnection&) = delete;
	TestConnection& operator=(const TestConnection&) = delete;

	AionConnection* get() const { return connection.get(); }

private:
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	commons::network::NioServer server{1, {}};
	std::shared_ptr<AionConnection> connection;
};

class PacketTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 17));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		PACKET_TEST_SCOPE;
		group = model::templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr);
	}

	void TearDown() override {
		detail::setPacketLookupsForTests(nullptr);
		group.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	/** A spawned-looking Npc of the template at (10, 20, 30) with the given spawn static id */
	runtime::Ref<TestNpc> createNpc(const model::templates::npc::NpcTemplate* objectTemplate, int32_t staticId = 0) {
		auto& spawn = group->addSpawnTemplate(std::make_unique<TestSpawnTemplate>(*group, staticId));
		return model::gameobjects::VisibleObject::create<TestNpc>(std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
	}

	runtime::ManualClock clock{0};
	runtime::Ref<model::templates::spawns::SpawnGroup> group;
};

} // namespace aion::gameserver::network::aion::serverpackets::testing
