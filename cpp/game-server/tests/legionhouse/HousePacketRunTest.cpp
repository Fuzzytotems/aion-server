// M5h HS-1 (P5-15): the run cases of the studio client packets a player without a house reaches - an unknown item, the
// script overflow and the foreign address, the kick and decoration refusals - on a recording connection (tests/cm_ak/ItemPacketTestSupport.h,
// included by relative path) and the test database (LegionHouseTestSupport.h; Player.getActiveHouse constructs HousingService, which reads it;
// run under gate_lock). The read cases are tests/cm_ak/HousePacketsTest.cpp.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "LegionHouseTestSupport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_DECORATE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_KICK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_SCRIPT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

namespace lh = ::aion::gameserver::legionhouse::test;
using network::test::PacketWriter;
using serverpackets::SM_HOUSE_SCRIPTS;
using serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t CM_HOUSE_SCRIPT_OPCODE = 30;
constexpr int32_t CM_HOUSE_KICK_OPCODE = 72;
constexpr int32_t CM_HOUSE_DECORATE_OPCODE = 75;
constexpr int32_t CM_HOUSE_EDIT_OPCODE = 82;

#define HOUSE_REQUIRE_DATABASE()                                                                                                                      	if (!lh::isDatabaseEnabled())                                                                                                                     		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

/** A player without a house on a recording connection */
class HousePacketsRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		if (lh::isDatabaseEnabled())
			lh::setUpDatabaseOnce(); // HousingService's construction (Player.getActiveHouse) reads the database
		items::publishPoetaWorldDataOnce(); // World.getInstance() builds its maps from the world data
		// the player's active house is looked up in HousingService, which reads the house data (none here)
		xml::LoadContext context;
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, "<house_lands/>"));
		owner = makePlayer(310001, 9301, "Dweller");
		owner.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(owner);
	}

	void TearDown() override {
		owner.player->setClientConnection(nullptr);
		client.reset();
		owner = {};
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	template <class P>
	void run(int32_t opcode, const PacketWriter& body) {
		(*client)->clearSent();
		Driver<P> packet(opcode);
		packet.readAndRun(body.data, client->get());
	}

	int32_t count(AionServerPacket&& packet) {
		const std::vector<uint8_t> expected = serialized(std::move(packet), client->con());
		int32_t n = 0;
		for (const SerializedBody& body : (*client)->sent())
			n += *body.bytes == expected ? 1 : 0;
		return n;
	}

	PlayerFixture owner;
	std::unique_ptr<TestClient> client;
};

TEST_F(HousePacketsRunTest, AnItemThePlayerDoesNotHaveIsNotRegistered) {
	HOUSE_REQUIRE_DATABASE();
	// every SM_HOUSE_EDIT reads the active house in writeImpl (SM_HOUSE_EDIT.java: house.getRegistry()), so the mode arms need a house: the
	// studio gate's C16
	run<CM_HOUSE_EDIT>(CM_HOUSE_EDIT_OPCODE, PacketWriter().C(3).D(123456));
	EXPECT_TRUE((*client)->sent().empty()) << "an item the player does not have is ignored";
}

TEST_F(HousePacketsRunTest, ScriptsOfAnotherHouseOrTooLargeAreRefused) {
	HOUSE_REQUIRE_DATABASE();
	const int32_t tooLarge = SM_HOUSE_SCRIPTS::MAX_COMPRESSED_SCRIPT_SIZE + 1;
	run<CM_HOUSE_SCRIPT>(CM_HOUSE_SCRIPT_OPCODE, PacketWriter().D(1001).C(4).H(100).D(tooLarge));
	EXPECT_EQ(count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_SCRIPT_OVERFLOW()), 1);
	run<CM_HOUSE_SCRIPT>(CM_HOUSE_SCRIPT_OPCODE, PacketWriter().D(1001).C(2).H(0));
	EXPECT_TRUE((*client)->sent().empty()) << "no house: audit-logged, nothing sent";
}

TEST_F(HousePacketsRunTest, KickAndDecorateWithoutAHouseOrALineAreIgnored) {
	HOUSE_REQUIRE_DATABASE();
	run<CM_HOUSE_KICK>(CM_HOUSE_KICK_OPCODE, PacketWriter().C(1).H(0));
	EXPECT_TRUE((*client)->sent().empty());
	run<CM_HOUSE_DECORATE>(CM_HOUSE_DECORATE_OPCODE, PacketWriter().D(0).D(0).H(26));
	EXPECT_TRUE((*client)->sent().empty()) << "a line number no part type covers";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
