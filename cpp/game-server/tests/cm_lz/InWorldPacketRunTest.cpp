// runImpl of the P5-16 in-world client packets ported in stage 3 wave B (m5a-plan.md §11 "Unported client packets", m5a-client-session.md F-2)
// other than CM_TARGET_SELECT (TargetSelectTest.cpp): CM_SHOW_BLOCKLIST and CM_PLAYER_LISTENER. The real client sent both while the user played
// and the factory refused both, because the classes did not exist.
//
// Java: clientpackets/CM_SHOW_BLOCKLIST.java:20-29, CM_PLAYER_LISTENER.java:21-29.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PLAYER_LISTENER.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_BLOCKLIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::BlockedPlayer;
using network::test::LogCapture;

/** decoded opcodes (ClientPacketInfo.gen.inc) */
constexpr int32_t OPCODE_SHOW_BLOCKLIST = 158;
constexpr int32_t OPCODE_PLAYER_LISTENER = 40;

class InWorldPacketRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(100001, 9001, "Actor");
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player)
			actor.player->setClientConnection(nullptr);
		client.reset();
		actor = {};
		InWorldPacketTest::TearDown();
	}

	/** Reads and runs one packet of type P on the actor's connection (both packets have an empty body) */
	template <class P>
	void run(int32_t opcode) {
		Driver<P> packet(opcode);
		packet.readAndRun({}, client->get());
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

// ------------------------------------------------------------------------------------------------------------------- CM_SHOW_BLOCKLIST

TEST_F(InWorldPacketRunTest, ShowBlocklistAnswersTheEmptyList) {
	run<CM_SHOW_BLOCKLIST>(OPCODE_SHOW_BLOCKLIST);

	// Java: sendPacket(new SM_BLOCK_LIST()), whose writeImpl reads the active player's block list at serialization
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(serverpackets::SM_BLOCK_LIST(), client->con())}));
}

/** The answer is decoded from the Java writeImpl (SM_BLOCK_LIST.java:26-34: H -size, C 0, then S name and S reason per entry), not compared
 *  against the server's own serialization, so a wrong width or order would show (m5a-plan.md D9) */
TEST_F(InWorldPacketRunTest, ShowBlocklistAnswersTheBlockedPlayers) {
	actor.player->getBlockList()->add(*BlockedPlayer::create(100002, "Other", "spam"));
	actor.player->getBlockList()->add(*BlockedPlayer::create(100003, "Third", ""));
	ASSERT_EQ(actor.player->getBlockList()->getSize(), 2);

	run<CM_SHOW_BLOCKLIST>(OPCODE_SHOW_BLOCKLIST);

	std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
	ASSERT_EQ(sent.size(), 1u);
	network::test::PacketReader reader(bodyOf(sent.at(0)));
	EXPECT_EQ(reader.H(), -2) << "Java writes the negated size";
	EXPECT_EQ(reader.C(), 0);
	// BlockList keeps its entries in a map keyed by object id, so read both pairs and match them by name. The two reads are separate
	// statements on purpose: the evaluation order of function arguments is unspecified, so emplace_back(reader.S(), reader.S()) would swap them.
	std::vector<std::pair<std::string, std::string>> entries;
	for (int i = 0; i < 2; ++i) {
		std::string name = reader.S();
		std::string reason = reader.S();
		entries.emplace_back(std::move(name), std::move(reason));
	}
	EXPECT_EQ(reader.remaining(), 0u) << "the body is consumed exactly";
	std::ranges::sort(entries);
	EXPECT_EQ(entries, (std::vector<std::pair<std::string, std::string>>{{"Other", "spam"}, {"Third", ""}}));
}

TEST_F(InWorldPacketRunTest, ShowBlocklistReadConsumesAnEmptyBody) {
	Driver<CM_SHOW_BLOCKLIST> packet(OPCODE_SHOW_BLOCKLIST);
	std::vector<uint8_t> data;
	packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
	LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"});
	ASSERT_TRUE(packet.read());
	EXPECT_EQ(packet.getRemainingBytes(), 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

// ------------------------------------------------------------------------------------------------------------------- CM_PLAYER_LISTENER

/**
 * The client sends C_SAVE every five minutes. Java's whole body is behind GSConfig.ENABLE_WEB_REWARDS, which is false by default
 * (gameserver.properties:94) and in the M5a profile, so the answer is silence - which is exactly what the 4.8 client expects, since it never
 * waits for one (the real-client session idled 17 minutes with the packet unhandled and never stalled).
 */
TEST_F(InWorldPacketRunTest, PlayerListenerDoesNothingWithWebRewardsOff) {
	ASSERT_FALSE(configs::main::GSConfig::ENABLE_WEB_REWARDS.load()) << "the default of gameserver.web_rewards.enable";

	run<CM_PLAYER_LISTENER>(OPCODE_PLAYER_LISTENER);

	EXPECT_TRUE((*client)->sentBytes().empty());
}

/**
 * The one unported body this handler can reach, pinned so the gap is a fact and not a comment: with the key on, Java calls
 * WebRewardService.sendAvailableRewards, whose C++ body is still AION_UNPORTED (WebRewardService.cpp:29-31, P5-08). The wave B report lists it.
 * A throw here is logged by AionClientPacket::run and does not disconnect the player, and the key is off in the M5a profile.
 */
TEST_F(InWorldPacketRunTest, PlayerListenerReachesTheUnportedWebRewardServiceWhenTheKeyIsOn) {
	configs::main::GSConfig::ENABLE_WEB_REWARDS.store(true);
	Driver<CM_PLAYER_LISTENER> packet(OPCODE_PLAYER_LISTENER);
	std::vector<uint8_t> empty;
	packet.setBuffer(commons::utils::ByteBuffer::wrap(empty));
	packet.setConnection(client->get());
	ASSERT_TRUE(packet.read());

	EXPECT_THROW(packet.runNow(), runtime::UnportedException);

	configs::main::GSConfig::ENABLE_WEB_REWARDS.store(false);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
