// CM_REMOVE_ALTERED_STATE (P5-16, m5b2-plan.md P-03/P-05): C_TURN_OFF_ABNORMAL_STATUS, what the client sends when the player right-clicks one
// of his own effect icons - the only client-driven way to end an effect early.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_REMOVE_ALTERED_STATE.java:23-42.
//
// runImpl's first statement asks the player's EffectController for the effect of the skill, and EffectController::findBySkillId is still
// AION_UNPORTED (m5b2-plan.md K-02, P5-02b). As CM_REVIVE's arms are asserted through the service body each one reaches (RevivePacketTest.cpp),
// the run test asserts the name in that UnportedException: the packet reaches the effect controller, and a unit test can see no further today.
// WHEN K-02 LANDS it becomes the three real cases: no effect of that skill -> nothing; a DEBUFF -> an audit line "tried to remove a debuff: <id>
// <name> (effector: <creature>)" and the effect stays; anything else -> Effect.endEffect. None of them can be built before Effect and
// EffectController are ported (an Effect needs Effect::create's initialize path, K-01), so they are NOT COVERED here and are named so that the
// absence is not mistaken for coverage.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REMOVE_ALTERED_STATE.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friend CM_REMOVE_ALTERED_STATE.h declares: the skill id readImpl decoded, which Java keeps private */
struct CM_REMOVE_ALTERED_STATETestAccess {
	static int32_t skillId(const CM_REMOVE_ALTERED_STATE& p) { return p.skillId; }
};

namespace testing {
namespace {

using Access = CM_REMOVE_ALTERED_STATETestAccess;
using network::test::LogCapture;
using network::test::PacketWriter;

/** the decoded opcode of ClientPacketInfo.gen.inc:47 (Java AionClientPacketFactory: packets[35] = CM_REMOVE_ALTERED_STATE, State.IN_GAME) */
constexpr int32_t OPCODE = 35;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** Java readImpl: UH skill id, C, C (the second "seen 1 with skillId 3573") */
std::vector<uint8_t> body(int32_t skillId, int32_t first = 0, int32_t second = 1) {
	return PacketWriter().H(skillId).C(first).C(second).data;
}

/** A fresh packet that has read `data`; nullptr if read() failed */
std::unique_ptr<CM_REMOVE_ALTERED_STATE> readPacket(const std::vector<uint8_t>& data, int32_t& unreadBytes) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_REMOVE_ALTERED_STATE>(OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unreadBytes = packet->getRemainingBytes();
	return packet;
}

TEST(RemoveAlteredStateReadTest, ASkillIdAndTwoBytes) {
	for (int32_t skillId : {3195, 0xFFFF}) {
		SCOPED_TRACE("skill " + std::to_string(skillId));
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		int32_t unread = -1;
		std::unique_ptr<CM_REMOVE_ALTERED_STATE> p = readPacket(body(skillId, 0x7F, 1), unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::skillId(*p), skillId) << "readUH: 0xFFFF is 65535";
		EXPECT_EQ(unread, 0) << "2 + 1 + 1 bytes";
		EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	}
	{ // the second byte missing: the last readC underflows
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		int32_t unread = -1;
		ASSERT_NE(readPacket(PacketWriter().H(3195).C(0).data, unread), nullptr);
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}
	{ // a spare byte is left: readImpl reads nothing after the second C
		std::vector<uint8_t> data = body(3195);
		data.push_back(0);
		int32_t unread = -1;
		ASSERT_NE(readPacket(data, unread), nullptr);
		EXPECT_EQ(unread, 1);
	}
}

class RemoveAlteredStateRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(320001, 9401, "Buffed");
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		if (actor.player)
			actor.player->setClientConnection(nullptr);
		client.reset();
		actor = {};
		InWorldPacketTest::TearDown();
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(RemoveAlteredStateRunTest, TheRequestGoesToThePlayersEffectController) {
	Driver<CM_REMOVE_ALTERED_STATE> packet(OPCODE);
	std::string what;
	try {
		packet.readAndRun(body(3195), client->get());
		ADD_FAILURE() << "EffectController::findBySkillId answered: K-02 has landed and this case must become the three real ones (file header)";
	} catch (const runtime::UnportedException& e) {
		what = e.what();
	}

	// CM_REMOVE_ALTERED_STATE.java:33: player.getEffectController().findBySkillId(skillId)
	EXPECT_NE(what.find("EffectController::findBySkillId"), std::string::npos) << what;
	EXPECT_TRUE((*client)->sentBytes().empty());
}

} // namespace
} // namespace testing
} // namespace aion::gameserver::network::aion::clientpackets
