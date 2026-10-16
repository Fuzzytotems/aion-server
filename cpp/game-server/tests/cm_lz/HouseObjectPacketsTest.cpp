// M5h HS-1 (P5-16): CM_USE_HOUSE_OBJECT and CM_RELEASE_OBJECT - the read of both bodies and the run of an object id nobody knows (nothing sent,
// nothing thrown). The use and the release of a real house object are the studio gate's (m5h-plan.md 14.2, HS-4) and the objects' tests.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/network/aion/clientpackets/CM_RELEASE_OBJECT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_HOUSE_OBJECT.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_USE_HOUSE_OBJECT_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_RELEASE_OBJECT_clientPacketFactory(int32_t opcode, const StateSet& validStates);

struct CM_USE_HOUSE_OBJECTTestAccess {
	static int32_t itemObjectId(const CM_USE_HOUSE_OBJECT& p) { return p.itemObjectId; }
};
struct CM_RELEASE_OBJECTTestAccess {
	static int32_t targetObjectId(const CM_RELEASE_OBJECT& p) { return p.targetObjectId; }
};

namespace testing {
namespace {

using network::test::PacketWriter;

constexpr int32_t CM_USE_HOUSE_OBJECT_OPCODE = 224;
constexpr int32_t CM_RELEASE_OBJECT_OPCODE = 225;

template <class P>
std::unique_ptr<P> readPacket(const std::vector<uint8_t>& data, int32_t opcode, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(HouseObjectPacketsReadTest, BothReadTheObjectId) {
	int32_t unread = -1;
	auto use = readPacket<CM_USE_HOUSE_OBJECT>(PacketWriter().D(0x0A0B0C0D).data, CM_USE_HOUSE_OBJECT_OPCODE, unread);
	ASSERT_NE(use, nullptr);
	EXPECT_EQ(CM_USE_HOUSE_OBJECTTestAccess::itemObjectId(*use), 0x0A0B0C0D);
	EXPECT_EQ(unread, 0);
	auto release = readPacket<CM_RELEASE_OBJECT>(PacketWriter().D(42).data, CM_RELEASE_OBJECT_OPCODE, unread);
	ASSERT_NE(release, nullptr);
	EXPECT_EQ(CM_RELEASE_OBJECTTestAccess::targetObjectId(*release), 42);
	EXPECT_EQ(unread, 0);
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_USE_HOUSE_OBJECT*>(CM_USE_HOUSE_OBJECT_clientPacketFactory(CM_USE_HOUSE_OBJECT_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_RELEASE_OBJECT*>(CM_RELEASE_OBJECT_clientPacketFactory(CM_RELEASE_OBJECT_OPCODE, inGame).get()), nullptr);
}

class HouseObjectPacketsRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		items::publishPoetaWorldDataOnce(); // World.getInstance() builds its maps from the world data
		// the player's active house is looked up in HousingService, which reads the house data (none here)
		xml::LoadContext context;
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, "<house_lands/>"));
		user = makePlayer(320001, 9401, "Visitor");
		user.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(user);
	}

	void TearDown() override {
		user.player->setClientConnection(nullptr);
		client.reset();
		user = {};
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	PlayerFixture user;
	std::unique_ptr<TestClient> client;
};

TEST_F(HouseObjectPacketsRunTest, AnUnknownObjectIsNeitherUsedNorReleased) {
	(*client)->clearSent();
	Driver<CM_USE_HOUSE_OBJECT>(CM_USE_HOUSE_OBJECT_OPCODE).readAndRun(PacketWriter().D(987654).data, client->get());
	Driver<CM_RELEASE_OBJECT>(CM_RELEASE_OBJECT_OPCODE).readAndRun(PacketWriter().D(987654).data, client->get());
	EXPECT_TRUE((*client)->sent().empty());
}

} // namespace
} // namespace testing
} // namespace aion::gameserver::network::aion::clientpackets
