// The chat packets (P5-15; m5g-plan.md K-04, W-01, W-03; m5j-plan.md J1): CM_CHAT_MESSAGE_PUBLIC (C_SAY, CM_CHAT_MESSAGE_PUBLIC.java:37-150) and
// CM_CHAT_MESSAGE_WHISPER (C_WHISPER, CM_CHAT_MESSAGE_WHISPER.java:52-76), with the server path behind them: ChatProcessor's command hook,
// PlayerRestrictions.canChat (prison, chat ban, flood), PlayerChatService's logs, NameRestrictionService.filterMessage and the arms of the
// chat types. ChatBanService itself is pinned by tests/playersvc/ChatBanServiceTest.cpp.
//
// The fixture is tests/playersvc/TravelTestSupport.h (P5-08's, by relative path as BindPointTeleportPacketTest.cpp includes it): a connected
// Elyos "Traveller" in Poeta with an Elyos "Watcher" 1 m from him; the whisper cases add an Asmodian "Stranger" in Ishalgen. Every expected
// packet is written from the Java writeImpl (m5a-plan.md D9): SM_MESSAGE (ServerPacketsOpcodes.java:42, SM_MESSAGE.java:136-151) and
// SM_SYSTEM_MESSAGE (:43, SM_SYSTEM_MESSAGE.java writeImpl).

#include "../playersvc/TravelTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/network/aion/AionConnection_State.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_MESSAGE_PUBLIC.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_MESSAGE_WHISPER.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/ban/ChatBanService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The fields the two readImpl decoded (Java keeps them private without a getter) */
struct ChatPacketsTestAccess {
	static model::ChatType type(const CM_CHAT_MESSAGE_PUBLIC& p) { return p.type; }
	static const std::string& message(const CM_CHAT_MESSAGE_PUBLIC& p) { return p.message; }
	static const std::string& name(const CM_CHAT_MESSAGE_WHISPER& p) { return p.name; }
	static const std::string& message(const CM_CHAT_MESSAGE_WHISPER& p) { return p.message; }
};

} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::services::teleport::test {
namespace {

using model::ChatType;
using network::aion::clientpackets::CM_CHAT_MESSAGE_PUBLIC;
using network::aion::clientpackets::CM_CHAT_MESSAGE_WHISPER;
using network::aion::clientpackets::ChatPacketsTestAccess;
using network::test::PacketWriter;

// ClientPacketInfo.gen.inc:39-40 (AionClientPacketFactory packets[27], [28])
constexpr int32_t CHAT_PUBLIC_OPCODE = 27;
constexpr int32_t CHAT_WHISPER_OPCODE = 28;
/** ServerPacketsOpcodes.java:42-43 */
constexpr int32_t SM_MESSAGE_OPCODE = 24;
constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = 25;
/** ChatType ids (ChatType.java) */
constexpr int8_t NORMAL = 0, SHOUT = 3, WHISPER = 4, GROUP = 5, ALLIANCE = 6, GROUP_LEADER = 7, LEAGUE = 8, LEGION = 10, COMMAND = 24,
				 GOLDEN_YELLOW = 25;
/** SM_SYSTEM_MESSAGE ids (SM_SYSTEM_MESSAGE.java) */
constexpr int32_t STR_NO_SUCH_USER = 1300627;                // :12191-12192
constexpr int32_t STR_YOU_EXCLUDED = 1300628;                // :12198-12199
constexpr int32_t STR_WHISPER_REFUSE = 1300629;              // :12205-12206
constexpr int32_t STR_CANT_WHISPER_LEVEL = 1310004;          // :15249-15250
constexpr int32_t STR_MSG_CANT_WHISPER_OTHER_RACE = 1401174; // :25191-25192
constexpr int32_t STR_INGAME_BLOCK_IN_NO_CHAT = 1300814;     // :13493-13494
constexpr int32_t STR_FLOODING = 1310001;                    // :15228-15229

/** A chat packet with its readImpl and runImpl reachable */
template <class P>
class ChatDriver final : public P {
public:
	explicit ChatDriver(int32_t opcode) : P(opcode, network::aion::StateSet{network::aion::AionConnection_State::IN_GAME}) {}

	bool readBody(std::vector<uint8_t> bytes, const std::shared_ptr<network::aion::AionConnection>& connection) {
		body = std::move(bytes);
		this->setBuffer(commons::utils::ByteBuffer::wrap(body));
		this->setConnection(connection);
		return this->read();
	}

	int32_t remaining() const { return this->getRemainingBytes(); }

	void runNow() { this->runImpl(); }

private:
	std::vector<uint8_t> body;
};

class ChatPacketsTest : public TravelTest {
protected:
	void SetUp() override {
		TravelTest::SetUp();
		savedGeneral = configs::main::LoggingConfig::LOG_GENERAL_CHATS.load();
		savedPrivate = configs::main::LoggingConfig::LOG_PRIVATE_CHATS.load();
		savedGmAudit = configs::main::LoggingConfig::LOG_GMAUDIT.load();
		savedFloodMsg = configs::main::SecurityConfig::FLOOD_MSG.load();
		savedFloodDelay = configs::main::SecurityConfig::FLOOD_DELAY.load();
		savedLevelToWhisper = configs::main::CustomConfig::LEVEL_TO_WHISPER.load();
		savedFactions = configs::main::CustomConfig::SPEAKING_BETWEEN_FACTIONS.load();
		// the shipped values (logging.properties:32-36, security.properties: flood.msg 6 / flood.delay 1, custom.properties)
		configs::main::LoggingConfig::LOG_GENERAL_CHATS.store(true);
		configs::main::LoggingConfig::LOG_PRIVATE_CHATS.store(false);
		configs::main::LoggingConfig::LOG_GMAUDIT.store(true);
		configs::main::SecurityConfig::FLOOD_MSG.store(6);
		configs::main::SecurityConfig::FLOOD_DELAY.store(1);
		configs::main::CustomConfig::LEVEL_TO_WHISPER.store(0);
		configs::main::CustomConfig::SPEAKING_BETWEEN_FACTIONS.store(false);
	}

	void TearDown() override {
		if (stranger.player) {
			stranger.player->getController().cancelAllTasks();
			if (stranger.player->isSpawned())
				world::World::getInstance().despawn(*stranger.player);
			stranger.player->clearKnownlist(model::animations::ObjectDeleteAnimation::FADE_OUT);
			world::World::getInstance().removeObject(*stranger.player);
			stranger.player->setQuestStateList(nullptr);
			stranger.player->setClientConnection(nullptr);
			strangerClient.reset();
			stranger = {};
		}
		configs::main::NameConfig::FORBIDDEN_WORDS.set(std::vector<std::string>{});
		configs::main::LoggingConfig::LOG_GENERAL_CHATS.store(savedGeneral);
		configs::main::LoggingConfig::LOG_PRIVATE_CHATS.store(savedPrivate);
		configs::main::LoggingConfig::LOG_GMAUDIT.store(savedGmAudit);
		configs::main::SecurityConfig::FLOOD_MSG.store(savedFloodMsg);
		configs::main::SecurityConfig::FLOOD_DELAY.store(savedFloodDelay);
		configs::main::CustomConfig::LEVEL_TO_WHISPER.store(savedLevelToWhisper);
		configs::main::CustomConfig::SPEAKING_BETWEEN_FACTIONS.store(savedFactions);
		TravelTest::TearDown();
	}

	void say(int8_t type, std::string_view text) {
		ChatDriver<CM_CHAT_MESSAGE_PUBLIC> packet(CHAT_PUBLIC_OPCODE);
		ASSERT_TRUE(packet.readBody(PacketWriter().C(type).S(text).data, actorClient->get()));
		ASSERT_EQ(packet.remaining(), 0);
		packet.runNow();
	}

	void whisper(std::string_view to, std::string_view text) {
		ChatDriver<CM_CHAT_MESSAGE_WHISPER> packet(CHAT_WHISPER_OPCODE);
		ASSERT_TRUE(packet.readBody(PacketWriter().S(to).S(text).data, actorClient->get()));
		ASSERT_EQ(packet.remaining(), 0);
		packet.runNow();
	}

	/** SM_MESSAGE.writeImpl of the actor's message as `receiverIsStaff` reads it: C type, C race (Elyos 0 + 1, 0 for staff), D id, S name, S text */
	std::vector<uint8_t> message(int8_t type, std::string_view text, int8_t senderRace = 1) {
		PacketWriter body;
		body.C(type).C(senderRace).D(player().getObjectId()).S("Traveller").S(text);
		if (type == SHOUT)
			body.F(ACTOR_SPOT.x).F(ACTOR_SPOT.y).F(ACTOR_SPOT.z);
		return javaPacket(SM_MESSAGE_OPCODE, body);
	}

	/** SM_SYSTEM_MESSAGE.writeImpl: C GOLDEN_YELLOW, C 0, D 0, D id, C params, S each, C 0 */
	static std::vector<uint8_t> systemMessage(int32_t id, std::initializer_list<std::string_view> params = {}) {
		PacketWriter body;
		body.C(GOLDEN_YELLOW).C(0).D(0).D(id).C(static_cast<int32_t>(params.size()));
		for (std::string_view param : params)
			body.S(param);
		body.C(0);
		return javaPacket(SM_SYSTEM_MESSAGE_OPCODE, body);
	}

	void spawnStranger() {
		stranger = spawnPlayer(420301, 9503, "Stranger", model::Race::ASMODIANS, ISHALGEN_ID, OSMAR_SPOT, 0, strangerClient);
		clearSent();
		(*strangerClient)->clearSent();
	}

	std::vector<std::vector<uint8_t>> strangerSent() { return (*strangerClient)->sentBytes(); }

	static constexpr int32_t ISHALGEN_ID = 220010000;
	cptest::PlayerFixture stranger;
	std::unique_ptr<cptest::TestClient> strangerClient;
	bool savedGeneral = false, savedPrivate = false, savedGmAudit = false, savedFactions = false;
	int32_t savedFloodMsg = 0, savedFloodDelay = 0, savedLevelToWhisper = 0;
};

// ---- readImpl ------------------------------------------------------------------------------------------------------------------------------

TEST_F(ChatPacketsTest, ThePublicMessageReadsItsTypeAndText) {
	spawnActor(0);
	ChatDriver<CM_CHAT_MESSAGE_PUBLIC> packet(CHAT_PUBLIC_OPCODE);
	ASSERT_TRUE(packet.readBody(PacketWriter().C(SHOUT).S("Hello there").data, actorClient->get()));
	EXPECT_EQ(packet.remaining(), 0);
	EXPECT_EQ(ChatPacketsTestAccess::type(packet), ChatType::SHOUT);
	EXPECT_EQ(ChatPacketsTestAccess::message(packet), "Hello there");
}

TEST_F(ChatPacketsTest, AnUnknownChatTypeFailsTheRead) {
	spawnActor(0);
	ChatDriver<CM_CHAT_MESSAGE_PUBLIC> packet(CHAT_PUBLIC_OPCODE);
	// ChatType.getChatType(2): no constant has id 2 - Java's IllegalArgumentException in readImpl, a failed read
	EXPECT_FALSE(packet.readBody(PacketWriter().C(2).S("x").data, actorClient->get()));
}

TEST_F(ChatPacketsTest, TheWhisperReadsTheNameAndText) {
	spawnActor(0);
	ChatDriver<CM_CHAT_MESSAGE_WHISPER> packet(CHAT_WHISPER_OPCODE);
	ASSERT_TRUE(packet.readBody(PacketWriter().S("Watcher").S("psst").data, actorClient->get()));
	EXPECT_EQ(packet.remaining(), 0);
	EXPECT_EQ(ChatPacketsTestAccess::name(packet), "Watcher");
	EXPECT_EQ(ChatPacketsTestAccess::message(packet), "psst");
}

// ---- CM_CHAT_MESSAGE_PUBLIC ----------------------------------------------------------------------------------------------------------------

TEST_F(ChatPacketsTest, NormalChatReachesTheSenderAndTheOnesAround) {
	spawnActor(0);
	network::test::LogCapture capture({"CHAT_LOG"}, spdlog::level::info);
	say(NORMAL, "Hello Poeta");
	EXPECT_EQ(sent(), cptest::exactly({message(NORMAL, "Hello Poeta")})) << "broadcastPacket(player, ..., true, ...): to himself as well";
	EXPECT_EQ(watcherSent(), cptest::exactly({message(NORMAL, "Hello Poeta")}));
	EXPECT_TRUE(capture.contains("info|CHAT_LOG|[NORMAL] - [Traveller](ELYOS): Hello Poeta")) << capture.dump();
}

TEST_F(ChatPacketsTest, AShoutCarriesTheSendersPosition) {
	spawnActor(0);
	say(SHOUT, "Hey!");
	EXPECT_EQ(watcherSent(), cptest::exactly({message(SHOUT, "Hey!")}));
}

TEST_F(ChatPacketsTest, ABlockingPlayerGetsNothingButTheSenderStillSeesHisOwnLine) {
	spawnActor(0);
	watcher.player->getBlockList()->add(*model::gameobjects::player::BlockedPlayer::create(player().getObjectId(), "Traveller", ""));
	say(NORMAL, "anyone?");
	EXPECT_TRUE(watcherSent().empty()) << "the predicate: !p.getBlockList().contains(player)";
	EXPECT_EQ(sent(), cptest::exactly({message(NORMAL, "anyone?")}));
}

TEST_F(ChatPacketsTest, AStaffSenderReachesAPlayerWhoBlocksHim) {
	spawnActor(0);
	player().getAccount()->setAccessLevel(1);
	watcher.player->getBlockList()->add(*model::gameobjects::player::BlockedPlayer::create(player().getObjectId(), "Traveller", ""));
	say(NORMAL, "GM here");
	EXPECT_EQ(watcherSent(), cptest::exactly({message(NORMAL, "GM here", 0)})) << "|| player.isStaff(); a staff sender's race byte is 0";
}

TEST_F(ChatPacketsTest, AForbiddenWordIsStarredAfterItWasLogged) {
	spawnActor(0);
	configs::main::NameConfig::FORBIDDEN_WORDS.set(std::vector<std::string>{"darn"});
	network::test::LogCapture capture({"CHAT_LOG"}, spdlog::level::info);
	say(NORMAL, "oh darn it");
	EXPECT_EQ(watcherSent(), cptest::exactly({message(NORMAL, "oh **** it")})) << "NameRestrictionService.filterMessage";
	EXPECT_TRUE(capture.contains("oh darn it")) << "logMessage gets the unfiltered text (CM_CHAT_MESSAGE_PUBLIC.java:52-53): " << capture.dump();
}

TEST_F(ChatPacketsTest, TeamChatWithoutATeamSendsNothing) {
	spawnActor(0);
	for (const int8_t type : {GROUP, ALLIANCE, GROUP_LEADER, LEAGUE, LEGION}) {
		say(type, "team?");
		EXPECT_TRUE(sent().empty()) << "type " << static_cast<int>(type);
		EXPECT_TRUE(watcherSent().empty()) << "type " << static_cast<int>(type);
	}
}

TEST_F(ChatPacketsTest, CommandChatNeedsACommander) {
	spawnActor(0);
	say(COMMAND, "charge");
	EXPECT_TRUE(sent().empty()) << "a level-1 player's abyss rank is no COMMANDER";
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(ChatPacketsTest, ASystemChatTypeIsStaffOnly) {
	spawnActor(0);
	say(GOLDEN_YELLOW, "announce");
	EXPECT_TRUE(watcherSent().empty()) << "default: !player.isStaff() returns";
	player().getAccount()->setAccessLevel(1);
	say(GOLDEN_YELLOW, "announce");
	// a system type keeps the sender race 0 (chatType.isSysMsg())
	EXPECT_EQ(watcherSent(), cptest::exactly({message(GOLDEN_YELLOW, "announce", 0)}));
}

TEST_F(ChatPacketsTest, ATextWithACommandPrefixButNoCommandIsChat) {
	spawnActor(0);
	say(NORMAL, "//nosuchcommand x");
	EXPECT_EQ(watcherSent(), cptest::exactly({message(NORMAL, "//nosuchcommand x")})) << "handleChatCommand finds no command and answers false";
}

TEST_F(ChatPacketsTest, APrisonerCannotChat) {
	spawnActor(0);
	player().setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 150 * 1000);
	say(NORMAL, "let me out");
	ASSERT_EQ(sent().size(), 1u);
	// getPrisonDurationSeconds() / 60 + 1: 149 or 150 s -> 3
	EXPECT_EQ(sent()[0], systemMessage(STR_INGAME_BLOCK_IN_NO_CHAT, {"3"}));
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(ChatPacketsTest, TheSeventhQuickMessageIsFloodingAndBansForTwoMinutes) {
	spawnActor(0);
	// flood.msg 6: setLastMessageTime counts a message less than flood.delay (1 s) after the previous one; the 8th call sees a count of 7
	for (int i = 0; i < 7; i++)
		say(NORMAL, "spam");
	clearSent();
	say(NORMAL, "spam");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_FLOODING)})) << "isFlooding: floodMsgCount 7 > 6";
	EXPECT_TRUE(watcherSent().empty());
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::GAG)) << "ChatBanService.banPlayer(player, 2 min)";
	clearSent();
	say(NORMAL, "still?");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_INGAME_BLOCK_IN_NO_CHAT, {"2"})})) << "the ban's minutes left, rounded up";
}

// ---- CM_CHAT_MESSAGE_WHISPER ---------------------------------------------------------------------------------------------------------------

TEST_F(ChatPacketsTest, AWhisperReachesOnlyTheReceiver) {
	spawnActor(0);
	network::test::LogCapture capture({"CHAT_LOG", "ADMINAUDIT_LOG"}, spdlog::level::info);
	whisper("Watcher", "psst");
	EXPECT_EQ(watcherSent(), cptest::exactly({message(WHISPER, "psst")}));
	EXPECT_TRUE(sent().empty()) << "Java sends the sender nothing (the client echoes it)";
	EXPECT_FALSE(capture.contains("psst")) << "private chats are not logged by default: " << capture.dump();
}

TEST_F(ChatPacketsTest, AWhisperIsLoggedWhenPrivateChatsAre) {
	spawnActor(0);
	configs::main::LoggingConfig::LOG_PRIVATE_CHATS.store(true);
	network::test::LogCapture capture({"CHAT_LOG"}, spdlog::level::info);
	whisper("Watcher", "psst");
	EXPECT_TRUE(capture.contains("info|CHAT_LOG|[WHISPER] - [Traveller]>[Watcher]: psst")) << capture.dump();
}

TEST_F(ChatPacketsTest, AWhisperOfAStaffMemberGoesToTheGmAudit) {
	spawnActor(0);
	player().getAccount()->setAccessLevel(1);
	network::test::LogCapture capture({"CHAT_LOG", "ADMINAUDIT_LOG"}, spdlog::level::info);
	whisper("Watcher", "psst");
	EXPECT_TRUE(capture.contains("info|ADMINAUDIT_LOG|[WHISPER] - [Traveller]>[Watcher]: psst")) << capture.dump();
}

TEST_F(ChatPacketsTest, AnUnknownNameIsNoSuchUser) {
	spawnActor(0);
	whisper("Nobody", "hello?");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_NO_SUCH_USER, {"Nobody"})}));
}

TEST_F(ChatPacketsTest, AReceiverInNoWhispersModeRefuses) {
	spawnActor(0);
	watcher.player->setCustomState(model::gameobjects::player::CustomPlayerState::NO_WHISPERS_MODE);
	whisper("Watcher", "psst");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_WHISPER_REFUSE, {"Watcher"})}));
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(ChatPacketsTest, BelowTheWhisperLevelTheWhisperIsRefused) {
	spawnActor(0);
	configs::main::CustomConfig::LEVEL_TO_WHISPER.store(10);
	whisper("Watcher", "psst");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_CANT_WHISPER_LEVEL, {"10"})}));
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(ChatPacketsTest, AReceiverWhoBlocksTheSenderExcludesHim) {
	spawnActor(0);
	watcher.player->getBlockList()->add(*model::gameobjects::player::BlockedPlayer::create(player().getObjectId(), "Traveller", ""));
	whisper("Watcher", "psst");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_YOU_EXCLUDED, {"Watcher"})}));
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(ChatPacketsTest, TheOtherRaceCannotBeWhisperedUnlessTheFactionsMaySpeak) {
	spawnActor(0);
	spawnStranger();
	whisper("Stranger", "hi");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_MSG_CANT_WHISPER_OTHER_RACE)}));
	EXPECT_TRUE(strangerSent().empty());

	clearSent();
	configs::main::CustomConfig::SPEAKING_BETWEEN_FACTIONS.store(true);
	whisper("Stranger", "hi");
	EXPECT_TRUE(sent().empty());
	// with the factions speaking, SM_MESSAGE keeps the sender race 0 (SM_MESSAGE.java:121-123)
	EXPECT_EQ(strangerSent(), cptest::exactly({message(WHISPER, "hi", 0)}));
}

TEST_F(ChatPacketsTest, ABannedSenderIsStoppedByCanChatAfterTheChecks) {
	spawnActor(0);
	ban::ChatBanService::banPlayer(player(), 60 * 1000);
	whisper("Watcher", "psst");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_INGAME_BLOCK_IN_NO_CHAT, {"1"})}));
	EXPECT_TRUE(watcherSent().empty());
	clearSent();
	whisper("Nobody", "psst");
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_NO_SUCH_USER, {"Nobody"})})) << "the receiver checks come before canChat";
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
