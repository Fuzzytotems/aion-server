// The stage-0 talking commands (m5j-plan.md §5.2): //announce, //whisper, //kick, //gag, //movie (data/handlers/admincommands), on real
// Players with real AionConnections (CommandTestSupport.h). //say needs an npc target and is in MonsterCommandsTest.cpp. The texts are the
// Java literals. ChatServer.sendPlayerGagPacket is a no-op without a chat server link (ChatServer.cpp: upConnection()).

#include "CommandTestSupport.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/admincommands/Announce.h"
#include "aion/gameserver/handlers/admincommands/Gag.h"
#include "aion/gameserver/handlers/admincommands/Kick.h"
#include "aion/gameserver/handlers/admincommands/Movie.h"
#include "aion/gameserver/handlers/admincommands/Whisper.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ban/ChatBanService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using serverpackets::SM_PLAY_MOVIE;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::ban::ChatBanService;

class TalkCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"announce", "say", "whisper", "kick", "gag", "movie"}) // the GM tools' level of the stage-0 set
			levels[alias] = 3;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	void TearDown() override {
		for (Player* player : stored) {
			ChatBanService::unbanPlayer(*player); // a //gag case's ban and its unban task (the ban map is static)
			world::World::getInstance().removeObject(*player);
		}
		stored.clear();
		CommandTest::TearDown();
	}

	/** a connected character that World lists (getPlayer by name, getAllPlayers, forEachPlayer) */
	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel, model::Race race = model::Race::ELYOS) {
		Player& player = connected(objectId, name, accessLevel);
		player.getCommonData()->setRace(race);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		player.getCommonData()->setOnline(true);
		stored.push_back(&player);
		return player;
	}

	/** PacketSendUtility.sendMessage(player, text, BRIGHT_YELLOW_CENTER) */
	std::vector<uint8_t> notice(std::string_view text, size_t index) {
		return serialized(SM_MESSAGE(0, "", text, model::ChatType::BRIGHT_YELLOW_CENTER), client(index).con());
	}

	std::vector<uint8_t> system(SM_SYSTEM_MESSAGE&& packet, size_t index = 0) { return serialized(packet, client(index).con()); }

	void clearAll() {
		for (const std::unique_ptr<TestClient>& c : clients)
			(*c)->clearSent();
	}

	std::vector<Player*> stored;
};

// ---- //announce (Announce.java:24-49) -----------------------------------------------------------------------------------------------------

TEST_F(TalkCommandsTest, AnnounceSyntaxAndTheUnknownMode) {
	Player& gm = online(730500, "Warden", 3);
	handlers::admincommands::Announce announce;
	EXPECT_TRUE(announce.process(gm, args({"n"})));
	EXPECT_EQ(client()->sentBytes(), info(announce.getSyntaxInfo())) << "params.length <= 1";
	client()->clearSent();
	EXPECT_TRUE(announce.process(gm, args({"x", "hello"})));
	EXPECT_EQ(client()->sentBytes(), info(announce.getSyntaxInfo()));
}

TEST_F(TalkCommandsTest, AnnounceWithTheNameOrAnonymouslyReachesEveryone) {
	Player& gm = online(730501, "Warden", 3);
	online(730502, "Elyo", 0, model::Race::ELYOS);
	online(730503, "Asmo", 0, model::Race::ASMODIANS);
	handlers::admincommands::Announce announce;
	clearAll();
	EXPECT_TRUE(announce.process(gm, args({"N", "hello", "world"})));
	for (size_t i = 0; i < 3; i++)
		EXPECT_EQ(client(i)->sentBytes(), exactly({notice("[charname:Warden;1 1 1]: hello world", i)})) << i;

	clearAll();
	EXPECT_TRUE(announce.process(gm, args({"a", "hi"})));
	for (size_t i = 0; i < 3; i++)
		EXPECT_EQ(client(i)->sentBytes(), exactly({notice("Announce: hi", i)})) << i;
}

TEST_F(TalkCommandsTest, AnnounceToARaceReachesItAndTheStaff) {
	Player& gm = online(730504, "Warden", 3, model::Race::ASMODIANS);
	online(730505, "Elyo", 0, model::Race::ELYOS);
	online(730506, "Asmo", 0, model::Race::ASMODIANS);
	handlers::admincommands::Announce announce;
	clearAll();
	EXPECT_TRUE(announce.process(gm, args({"ely", "go"})));
	EXPECT_EQ(client(0)->sentBytes(), exactly({notice("Elyos: go", 0)})) << "an Asmodian GM: validateAccess";
	EXPECT_EQ(client(1)->sentBytes(), exactly({notice("Elyos: go", 1)}));
	EXPECT_TRUE(client(2)->sentBytes().empty()) << "an Asmodian player";

	clearAll();
	EXPECT_TRUE(announce.process(gm, args({"ASMO", "go"})));
	EXPECT_EQ(client(0)->sentBytes(), exactly({notice("Asmodians: go", 0)}));
	EXPECT_TRUE(client(1)->sentBytes().empty()) << "an Elyos player";
	EXPECT_EQ(client(2)->sentBytes(), exactly({notice("Asmodians: go", 2)}));
}

// ---- //whisper (Whisper.java:17-31) ------------------------------------------------------------------------------------------------------

TEST_F(TalkCommandsTest, WhisperSwitchesTheNoWhispersMode) {
	Player& gm = connected(730510, "Warden", 3);
	handlers::admincommands::Whisper whisper;
	EXPECT_TRUE(whisper.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(whisper.getSyntaxInfo()));

	client()->clearSent();
	EXPECT_TRUE(whisper.process(gm, args({"OFF"})));
	EXPECT_TRUE(gm.isInCustomState(CustomPlayerState::NO_WHISPERS_MODE));
	EXPECT_EQ(client()->sentBytes(), info("Accepting whispers: OFF"));

	client()->clearSent();
	EXPECT_TRUE(whisper.process(gm, args({"maybe"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "neither on nor off: no answer";
	EXPECT_TRUE(gm.isInCustomState(CustomPlayerState::NO_WHISPERS_MODE));

	EXPECT_TRUE(whisper.process(gm, args({"on"})));
	EXPECT_FALSE(gm.isInCustomState(CustomPlayerState::NO_WHISPERS_MODE));
	EXPECT_EQ(client()->sentBytes(), info("Accepting whispers: ON"));
}

// ---- //kick (Kick.java:24-49) ------------------------------------------------------------------------------------------------------------

TEST_F(TalkCommandsTest, KickOnePlayerByName) {
	Player& gm = online(730520, "Warden", 3);
	online(730521, "Victim", 0);
	handlers::admincommands::Kick kick;
	EXPECT_TRUE(kick.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(kick.getSyntaxInfo()));

	clearAll();
	EXPECT_TRUE(kick.process(gm, args({"nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NO_OFFLINE_CHARACTER())}));

	clearAll();
	EXPECT_TRUE(kick.process(gm, args({"vICTIM"}))) << "Util.convertName";
	EXPECT_EQ(client(1)->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_KICK_CHARACTER(), 1)})) << "the close packet";
	EXPECT_TRUE(client(1)->isPendingClose());
	EXPECT_FALSE(client()->isPendingClose());
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_USER_KICKED("Victim"))}));
}

TEST_F(TalkCommandsTest, KickAllSparesTheGmAndNeedsUppercase) {
	Player& gm = online(730522, "Warden", 3);
	handlers::admincommands::Kick kick;
	EXPECT_TRUE(kick.process(gm, args({"ALL"})));
	EXPECT_EQ(client()->sentBytes(), info("There is nobody online to kick.")) << "only the GM";

	online(730523, "One", 0);
	online(730524, "Two", 0);
	clearAll();
	EXPECT_TRUE(kick.process(gm, args({"all"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NO_OFFLINE_CHARACTER())}))
		<< "lower case is a name (\"All\")";
	EXPECT_FALSE(client(1)->isPendingClose());

	clearAll();
	EXPECT_TRUE(kick.process(gm, args({"ALL"})));
	EXPECT_FALSE(client()->isPendingClose());
	EXPECT_TRUE(client(1)->isPendingClose());
	EXPECT_TRUE(client(2)->isPendingClose());
	std::vector<std::vector<uint8_t>> answers = client()->sentBytes();
	std::vector<std::vector<uint8_t>> expected{system(SM_SYSTEM_MESSAGE::STR_USER_KICKED("One")), system(SM_SYSTEM_MESSAGE::STR_USER_KICKED("Two"))};
	std::ranges::sort(answers); // World.forEachPlayer's order is the map's
	std::ranges::sort(expected);
	EXPECT_EQ(answers, expected);
}

// ---- //gag (Gag.java:26-62) --------------------------------------------------------------------------------------------------------------

TEST_F(TalkCommandsTest, GagRefusals) {
	Player& gm = online(730530, "Warden", 3);
	online(730531, "Talker", 0);
	handlers::admincommands::Gag gag;
	EXPECT_TRUE(gag.process(gm, args({"talker"})));
	EXPECT_EQ(client()->sentBytes(), info(gag.getSyntaxInfo())) << "fewer than two parameters";

	client()->clearSent();
	EXPECT_TRUE(gag.process(gm, args({"nobody", "5", "spam"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"))}));

	client()->clearSent();
	EXPECT_TRUE(gag.process(gm, args({"talker", "0", "spam"})));
	EXPECT_EQ(client()->sentBytes(), info("Duration must be at least 1 minute."));

	client()->clearSent();
	EXPECT_TRUE(gag.process(gm, args({"talker", "5"})));
	EXPECT_EQ(client()->sentBytes(), info("Reason must be specified."));

	client()->clearSent();
	EXPECT_TRUE(gag.process(gm, args({"talker", "remove"})));
	EXPECT_EQ(client()->sentBytes(), info("[charname:Talker;1 1 1] can already chat."));
	EXPECT_EQ(client(1)->sentBytes().size(), 0u);
}

TEST_F(TalkCommandsTest, GagBansForTheMinutesAndRemoveLiftsIt) {
	Player& gm = online(730532, "Warden", 3);
	Player& talker = online(730533, "Talker", 0);
	handlers::admincommands::Gag gag;
	clearAll();
	EXPECT_TRUE(gag.process(gm, args({"Talker", "3", "too", "loud"})));
	EXPECT_TRUE(ChatBanService::isBanned(talker));
	EXPECT_EQ(ChatBanService::getBanMinutes(talker), 3) << "Duration.ofMinutes(3).toMillis()";
	EXPECT_TRUE(talker.getController().hasTask(model::TaskId::GAG));
	std::vector<std::vector<uint8_t>> toTalker{system(SM_SYSTEM_MESSAGE::STR_INGAME_BLOCK_ENABLE_NO_CHAT(3), 1)};
	for (const std::vector<uint8_t>& part : info("too loud", 1))
		toTalker.push_back(part);
	EXPECT_EQ(client(1)->sentBytes(), toTalker);
	EXPECT_EQ(client()->sentBytes(), info("[charname:Talker;1 1 1] is now gagged for 3 minute(s)."));

	clearAll();
	EXPECT_TRUE(gag.process(gm, args({"talker", "REMOVE"})));
	EXPECT_FALSE(ChatBanService::isBanned(talker));
	EXPECT_FALSE(talker.getController().hasTask(model::TaskId::GAG));
	EXPECT_EQ(client(1)->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_CAN_CHAT_NOW(), 1)}));
	EXPECT_EQ(client()->sentBytes(), info("Unbanned [charname:Talker;1 1 1] from all chats."));
}

TEST_F(TalkCommandsTest, GagWithANonNumericDurationAnswersTheNumberError) {
	Player& gm = online(730534, "Warden", 3);
	Player& talker = online(730535, "Talker", 0);
	handlers::admincommands::Gag gag;
	clearAll();
	EXPECT_TRUE(gag.process(gm, args({"talker", "ten", "spam"}))) << "a NumberFormatException is an IllegalArgumentException";
	EXPECT_FALSE(ChatBanService::isBanned(talker));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"ten\""))
		<< "ChatCommand.toErrorMessage of Integer.parseInt's NumberFormatException";
	EXPECT_TRUE(client(1)->sentBytes().empty());
}

// ---- //movie (Movie.java:21-30) ----------------------------------------------------------------------------------------------------------

TEST_F(TalkCommandsTest, MoviePlaysACutsceneOrAMovie) {
	Player& gm = connected(730540, "Warden", 3);
	handlers::admincommands::Movie movie;
	EXPECT_TRUE(movie.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(movie.getSyntaxInfo()));

	client()->clearSent();
	EXPECT_TRUE(movie.process(gm, args({"42"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_PLAY_MOVIE(false, 0, 0, 42, true), client().con())}));

	client()->clearSent();
	EXPECT_TRUE(movie.process(gm, args({"M", "7"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_PLAY_MOVIE(true, 0, 0, 7, true), client().con())}));

	client()->clearSent();
	EXPECT_TRUE(movie.process(gm, args({"m"})));
	EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>"))
		<< "params[1]: ArrayIndexOutOfBoundsException, logged by ChatCommand.run (Java's behaviour, a proposed correction)";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
