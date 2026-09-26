// runImpl of the P5-15 in-world client packets ported in stage 3 wave B (m5a-plan.md §11 "Unported client packets", m5a-client-session.md F-2):
// CM_CHECK_PAK, CM_FRIEND_STATUS and CM_INSTANCE_INFO. The real client sent all three while the user played, and the factory refused all three
// because the classes did not exist. CM_EMOTION, the fourth of this chunk, has its own file (EmotionRunTest.cpp).
//
// Each test drives the packet against a real Player and a real AionConnection and asserts the state change plus the exact bytes runImpl queued.
//
// Java: clientpackets/CM_CHECK_PAK.java:24-34, CM_FRIEND_STATUS.java:30-44, CM_INSTANCE_INFO.java:26-41.

#include "InWorldPacketRunSupport.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/FriendList_StatusInfo.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_PAK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_STATUS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_INSTANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::FriendList;
using network::test::LogCapture;
using network::test::PacketWriter;

/** decoded opcodes (ClientPacketInfo.gen.inc) */
constexpr int32_t OPCODE_CHECK_PAK = 62;
constexpr int32_t OPCODE_FRIEND_STATUS = 170;
constexpr int32_t OPCODE_INSTANCE_INFO = 192;

/** Two worlds of the real instance_cooltimes.xml, so SM_INSTANCE_INFO writes a real cooldown row per world instead of an empty list */
constexpr std::string_view INSTANCE_COOLTIMES = R"(<instance_cooltimes>
	<instance_cooltime race="PC_ALL" worldId="310090000" id="1" sync_id="1">
		<type>DAILY</type>
		<ent_cool_time>900</ent_cool_time>
		<maxcount>5</maxcount>
		<max_member_light>6</max_member_light>
		<max_member_dark>6</max_member_dark>
		<enter_min_level_light>41</enter_min_level_light>
		<enter_min_level_dark>41</enter_min_level_dark>
		<can_enter_mentor>true</can_enter_mentor>
	</instance_cooltime>
	<instance_cooltime race="ELYOS" worldId="320080000" id="2" sync_id="2">
		<type>DAILY</type>
		<ent_cool_time>900</ent_cool_time>
		<maxcount>5</maxcount>
		<max_member_light>6</max_member_light>
		<max_member_dark>6</max_member_dark>
		<enter_min_level_light>39</enter_min_level_light>
		<enter_min_level_dark>39</enter_min_level_dark>
		<can_enter_mentor>true</can_enter_mentor>
	</instance_cooltime>
</instance_cooltimes>)";

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

	/** Reads and runs one packet of type P on the actor's connection */
	template <class P>
	void run(int32_t opcode, const std::vector<uint8_t>& packetBody) {
		Driver<P> packet(opcode);
		packet.readAndRun(packetBody, client->get());
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

// ------------------------------------------------------------------------------------------------------------------- CM_CHECK_PAK

/** Java readImpl: C unk (always 2), S pak status. runImpl audits anything that is neither empty, nor "[1:OK]"-terminated, nor "File not found" */
std::vector<uint8_t> checkPakBody(std::string_view pakStatus) {
	return PacketWriter().C(2).S(pakStatus).data;
}

TEST_F(InWorldPacketRunTest, CheckPakAuditsAModifiedPak) {
	configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	LogCapture audit({"AUDIT_LOG"});

	run<CM_CHECK_PAK>(OPCODE_CHECK_PAK, checkPakBody("data/Data.pak[2:MODIFIED]"));

	EXPECT_TRUE(audit.contains("using modified data pak: data/Data.pak[2:MODIFIED]")) << audit.dump();
	EXPECT_TRUE(audit.contains("Player [id=100001, name=Actor]")) << audit.dump();
	configs::main::LoggingConfig::LOG_AUDIT.store(false);
}

TEST_F(InWorldPacketRunTest, CheckPakStaysSilentForTheThreeAcceptedStatuses) {
	configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	LogCapture audit({"AUDIT_LOG"});

	// Java: isEmpty() || endsWith("[1:OK]") || contains("File not found")
	run<CM_CHECK_PAK>(OPCODE_CHECK_PAK, checkPakBody(""));
	run<CM_CHECK_PAK>(OPCODE_CHECK_PAK, checkPakBody("data/Data.pak[1:OK]"));
	run<CM_CHECK_PAK>(OPCODE_CHECK_PAK, checkPakBody("data/Data.pak: File not found"));

	EXPECT_FALSE(audit.contains("modified data pak")) << audit.dump();
	EXPECT_TRUE((*client)->sentBytes().empty()) << "CM_CHECK_PAK answers nothing";
	configs::main::LoggingConfig::LOG_AUDIT.store(false);
}

/** "[1:OK]" only saves the status at its end: Java uses endsWith, not contains */
TEST_F(InWorldPacketRunTest, CheckPakAuditsAStatusThatOnlyStartsWithOk) {
	configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	LogCapture audit({"AUDIT_LOG"});

	run<CM_CHECK_PAK>(OPCODE_CHECK_PAK, checkPakBody("a.pak[1:OK] b.pak[2:MODIFIED]"));

	EXPECT_TRUE(audit.contains("using modified data pak: a.pak[1:OK] b.pak[2:MODIFIED]")) << audit.dump();
	configs::main::LoggingConfig::LOG_AUDIT.store(false);
}

TEST_F(InWorldPacketRunTest, CheckPakReadConsumesTheBodyExactly) {
	Driver<CM_CHECK_PAK> packet(OPCODE_CHECK_PAK);
	std::vector<uint8_t> data = checkPakBody("data/Data.pak[1:OK]");
	packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
	LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"});
	ASSERT_TRUE(packet.read());
	EXPECT_EQ(packet.getRemainingBytes(), 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

// ------------------------------------------------------------------------------------------------------------------- CM_FRIEND_STATUS

TEST_F(InWorldPacketRunTest, FriendStatusStoresTheStatusAndEchoesIt) {
	ASSERT_EQ(actor.player->getFriendList().getStatus(), FriendList::Status::OFFLINE) << "a fresh FriendList starts offline";

	run<CM_FRIEND_STATUS>(OPCODE_FRIEND_STATUS, PacketWriter().C(model::gameobjects::player::getId(FriendList::Status::AWAY)).data);

	EXPECT_EQ(actor.player->getFriendList().getStatus(), FriendList::Status::AWAY);
	// Java: sendPacket(new SM_FRIEND_STATUS(status)) - the raw byte, not the enum's id
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(serverpackets::SM_FRIEND_STATUS(3))}));
}

TEST_F(InWorldPacketRunTest, FriendStatusAcceptsOnlineAndOffline) {
	run<CM_FRIEND_STATUS>(OPCODE_FRIEND_STATUS, PacketWriter().C(1).data);
	EXPECT_EQ(actor.player->getFriendList().getStatus(), FriendList::Status::ONLINE);

	(*client)->clearSent();
	run<CM_FRIEND_STATUS>(OPCODE_FRIEND_STATUS, PacketWriter().C(0).data);

	EXPECT_EQ(actor.player->getFriendList().getStatus(), FriendList::Status::OFFLINE);
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(serverpackets::SM_FRIEND_STATUS(0))}));
}

/** Java: an id Status.getByValue does not know warns and falls back to ONLINE, but the echoed byte stays the one the client sent */
TEST_F(InWorldPacketRunTest, FriendStatusFallsBackToOnlineForAnUnknownIdAndEchoesTheRawByte) {
	LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_FRIEND_STATUS"});

	run<CM_FRIEND_STATUS>(OPCODE_FRIEND_STATUS, PacketWriter().C(2).data); // 2 is between ONLINE (1) and AWAY (3)

	EXPECT_EQ(actor.player->getFriendList().getStatus(), FriendList::Status::ONLINE);
	EXPECT_TRUE(capture.contains("received unknown status id 2")) << capture.dump();
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(serverpackets::SM_FRIEND_STATUS(2))}));
}

/** readImpl is readC(), i.e. a signed byte: 0xFF arrives as -1, which is no status id */
TEST_F(InWorldPacketRunTest, FriendStatusReadsTheStatusAsASignedByte) {
	LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_FRIEND_STATUS"});

	run<CM_FRIEND_STATUS>(OPCODE_FRIEND_STATUS, PacketWriter().C(0xFF).data);

	EXPECT_EQ(actor.player->getFriendList().getStatus(), FriendList::Status::ONLINE);
	EXPECT_TRUE(capture.contains("received unknown status id -1")) << capture.dump();
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(serverpackets::SM_FRIEND_STATUS(-1))}));
}

// ------------------------------------------------------------------------------------------------------------------- CM_INSTANCE_INFO

class InstanceInfoRunTest : public InWorldPacketRunTest {
protected:
	void SetUp() override {
		InWorldPacketRunTest::SetUp();
		// SM_INSTANCE_INFO's constructor defaults its world list to every key of INSTANCE_COOLTIME_DATA and its writeImpl dereferences the
		// template of each (SM_INSTANCE_INFO.cpp:38-42, 55-70), so the holder must be published
		xml::LoadContext context;
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(
			xml::bindString<dataholders::InstanceCooltimeData>(context, INSTANCE_COOLTIMES));
	}

	void TearDown() override {
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.resetForTests();
		InWorldPacketRunTest::TearDown();
	}

	/** Java readImpl: D unk (always 0), C update type */
	static std::vector<uint8_t> body(int8_t updateType) { return PacketWriter().D(0).C(updateType).data; }
};

TEST_F(InstanceInfoRunTest, AnswersTheOwnCooldownsWhenThePlayerIsInNoTeam) {
	ASSERT_FALSE(actor.player->isInTeam());

	run<CM_INSTANCE_INFO>(OPCODE_INSTANCE_INFO, body(0));

	// Java: `player.isInTeam() ? getLeaderObject() : player`, then one SM_INSTANCE_INFO and nothing else (updateType != 1)
	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(serverpackets::SM_INSTANCE_INFO(int8_t{0}, *actor.player), client->con())}));
}

/** updateType 1 is the team-member update; without a team Java sends only the player's own row and skips the split list entirely */
TEST_F(InstanceInfoRunTest, UpdateTypeOneWithoutATeamStillSendsOnlyTheOwnRow) {
	run<CM_INSTANCE_INFO>(OPCODE_INSTANCE_INFO, body(1));

	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(serverpackets::SM_INSTANCE_INFO(int8_t{1}, *actor.player), client->con())}));
}

/** The update type is written straight into the answer, so a client value the server does not interpret still comes back */
TEST_F(InstanceInfoRunTest, TheUpdateTypeIsEchoedIntoThePacket) {
	run<CM_INSTANCE_INFO>(OPCODE_INSTANCE_INFO, body(2));

	std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent.at(0), serialized(serverpackets::SM_INSTANCE_INFO(int8_t{2}, *actor.player), client->con()));
	EXPECT_NE(sent.at(0), serialized(serverpackets::SM_INSTANCE_INFO(int8_t{0}, *actor.player), client->con()))
		<< "the update type must reach the wire, so 2 and 0 cannot serialize alike";
}

TEST_F(InstanceInfoRunTest, ReadConsumesTheBodyExactly) {
	Driver<CM_INSTANCE_INFO> packet(OPCODE_INSTANCE_INFO);
	std::vector<uint8_t> data = body(1);
	ASSERT_EQ(data.size(), 5u) << "D unk + C update type";
	packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
	LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"});
	ASSERT_TRUE(packet.read());
	EXPECT_EQ(packet.getRemainingBytes(), 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
