// CM_BIND_POINT_TELEPORT (P5-15, m5f-plan.md §5 P-03 and P-07): the world map's hotspot teleport, C_HOTSPOT. Java: CM_BIND_POINT_TELEPORT.java:23-46.
//
// The fixture is tests/playersvc/TravelTestSupport.h (P5-08's, by relative path as tests/cm_lz includes it): a connected Elyos WARRIOR in Poeta
// with a second player beside him, on the fixture's DeterministicExecutor. The hotspot rows are hotspot_template.xml:6 and :8 (Poeta), verbatim;
// the service behind the packet is pinned by tests/playersvc/BindPointTeleportTest.cpp, these cases pin the packet's reading and dispatch.

#include "../playersvc/TravelTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/HotspotData.bind.h"
#include "aion/gameserver/dataholders/HotspotData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/AionConnection_State.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BIND_POINT_TELEPORT.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The fields CM_BIND_POINT_TELEPORT::readImpl decoded (Java keeps them private without a getter) */
struct CM_BIND_POINT_TELEPORTTestAccess {
	static int8_t action(const CM_BIND_POINT_TELEPORT& p) { return p.action; }
	static int32_t locId(const CM_BIND_POINT_TELEPORT& p) { return p.locId; }
	static int64_t kinah(const CM_BIND_POINT_TELEPORT& p) { return p.kinah; }
};

} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::services::teleport::test {
namespace {

using network::aion::clientpackets::CM_BIND_POINT_TELEPORT;
using network::aion::clientpackets::CM_BIND_POINT_TELEPORTTestAccess;
using network::test::PacketWriter;

// ClientPacketInfo.gen.inc:201 (AionClientPacketFactory packets[244])
constexpr int32_t BIND_POINT_TELEPORT_OPCODE = 244;
/** ServerPacketsOpcodes.java:314 */
constexpr int32_t SM_BIND_POINT_TELEPORT_OPCODE = 296;

/** hotspot_template.xml:6 and :8 */
constexpr std::string_view PACKET_HOTSPOT_XML = R"xml(<hotspot_template>
	<hotspot_location id="14" worldId="210010000" x="560.0" y="1382.0" z="119.0" race="ELYOS" price="44"/>
	<hotspot_location id="15" worldId="210010000" x="427.0" y="1741.0" z="120.0" race="ELYOS" price="44"/>
</hotspot_template>)xml";

/** CM_BIND_POINT_TELEPORT with its readImpl and runImpl reachable, and the bytes it left unread */
class HotspotDriver final : public CM_BIND_POINT_TELEPORT {
public:
	HotspotDriver()
		: CM_BIND_POINT_TELEPORT(BIND_POINT_TELEPORT_OPCODE, network::aion::StateSet{network::aion::AionConnection_State::IN_GAME}) {}

	bool readBody(std::vector<uint8_t> bytes, const std::shared_ptr<network::aion::AionConnection>& connection) {
		body = std::move(bytes);
		setBuffer(commons::utils::ByteBuffer::wrap(body));
		setConnection(connection);
		return read();
	}

	int32_t remaining() const { return getRemainingBytes(); }

	void runNow() { runImpl(); }

private:
	std::vector<uint8_t> body;
};

class BindPointTeleportPacketTest : public TravelTest {
protected:
	void SetUp() override {
		TravelTest::SetUp();
		if (prepared)
			dataholders::DataManager::HOTSPOT_DATA.publish(xml::bindString<dataholders::HotspotData>(context, PACKET_HOTSPOT_XML));
	}

	void TearDown() override {
		if (prepared)
			dataholders::DataManager::HOTSPOT_DATA.resetForTests();
		TravelTest::TearDown();
	}

	/** readImpl's body: C action; action 1: D locId, Q kinah */
	static std::vector<uint8_t> castBody(int32_t locId, int64_t kinah) { return PacketWriter().C(1).D(locId).Q(kinah).data; }

	void receive(std::vector<uint8_t> body) {
		HotspotDriver packet;
		ASSERT_TRUE(packet.readBody(std::move(body), actorClient->get()));
		ASSERT_EQ(packet.remaining(), 0);
		packet.runNow();
	}

	std::vector<uint8_t> hotspotPacket(int32_t action, int32_t locId) {
		PacketWriter body;
		body.C(action).D(player().getObjectId());
		if (action == 1)
			body.D(locId);
		return javaPacket(SM_BIND_POINT_TELEPORT_OPCODE, body);
	}

	static void advance(int64_t millis) {
		dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend()).advance(std::chrono::milliseconds(millis));
	}
};

// ---- readImpl ------------------------------------------------------------------------------------------------------------------------------

TEST_F(BindPointTeleportPacketTest, TheCastReadsItsThirteenBytes) {
	spawnActor(0);
	HotspotDriver packet;
	std::vector<uint8_t> body = PacketWriter().C(1).D(0x01020304).Q(0x0102030405060708LL).data;
	ASSERT_EQ(body.size(), 13u);

	ASSERT_TRUE(packet.readBody(body, actorClient->get()));

	EXPECT_EQ(CM_BIND_POINT_TELEPORTTestAccess::action(packet), 1);
	EXPECT_EQ(CM_BIND_POINT_TELEPORTTestAccess::locId(packet), 0x01020304);
	EXPECT_EQ(CM_BIND_POINT_TELEPORTTestAccess::kinah(packet), 0x0102030405060708LL);
	EXPECT_EQ(packet.remaining(), 0);
}

TEST_F(BindPointTeleportPacketTest, TheCancelAndTheDoneReadOneByte) {
	spawnActor(0);
	for (int32_t action : {2, 3}) {
		HotspotDriver packet;
		ASSERT_TRUE(packet.readBody(PacketWriter().C(action).D(15).Q(71).data, actorClient->get()));

		EXPECT_EQ(CM_BIND_POINT_TELEPORTTestAccess::action(packet), action);
		EXPECT_EQ(CM_BIND_POINT_TELEPORTTestAccess::locId(packet), 0) << "only action 1 reads a loc id";
		EXPECT_EQ(CM_BIND_POINT_TELEPORTTestAccess::kinah(packet), 0);
		EXPECT_EQ(packet.remaining(), 12) << action;
	}
}

// ---- runImpl -------------------------------------------------------------------------------------------------------------------------------

TEST_F(BindPointTeleportPacketTest, TheCastStartsTheHotspotTeleport) {
	spawnActor(1000);

	receive(castBody(15, 71));

	EXPECT_EQ(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(1, 15)}));
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::SKILL_USE));
	advance(11000);
	EXPECT_EQ(kinah(), 1000 - 71) << "the client's kinah reached the price check (max(71, 71))";
	EXPECT_FLOAT_EQ(player().getX(), 427.0f);
}

TEST_F(BindPointTeleportPacketTest, TheClientsKinahIsThePriceItSent) {
	spawnActor(1000);

	receive(castBody(15, 90));
	advance(11000);

	EXPECT_EQ(kinah(), 1000 - 90);
}

TEST_F(BindPointTeleportPacketTest, TheCancelCancelsWithLocIdZero) {
	spawnActor(1000);
	receive(castBody(15, 71));
	clearSent();

	receive(PacketWriter().C(2).data);

	// CM_BIND_POINT_TELEPORT.java:42-43: the cancel carries no loc id, so cancelTeleport broadcasts 0
	EXPECT_EQ(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(2, 0)}));
	EXPECT_EQ(ofOpcode(watcherSent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(2, 0)}));
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
	advance(11000);
	EXPECT_EQ(kinah(), 1000);
}

TEST_F(BindPointTeleportPacketTest, TheDoneDoesNothing) {
	spawnActor(1000);
	receive(castBody(15, 71));
	clearSent();

	receive(PacketWriter().C(3).data);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::SKILL_USE)) << "the cast goes on";
}

TEST_F(BindPointTeleportPacketTest, ADeadPlayersPacketIsIgnored) {
	spawnActor(1000);
	player().setLifeStats(std::make_unique<cptest::DeadPlayerLifeStats>(player()));
	ASSERT_TRUE(player().isDead());

	receive(castBody(15, 71));

	EXPECT_TRUE(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE).empty());
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

TEST_F(BindPointTeleportPacketTest, ADeadPlayersCancelIsIgnored) {
	spawnActor(1000);
	receive(castBody(15, 71));
	player().setLifeStats(std::make_unique<cptest::DeadPlayerLifeStats>(player()));
	clearSent();

	receive(PacketWriter().C(2).data);

	EXPECT_TRUE(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE).empty());
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
