// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned commands of M5g): //event (Event.java) on real Players with real AionConnections
// (CommandTestSupport.h) in a Poeta map instance. The texts are the Java literals.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/admincommands/Event.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;

/** ChatCommand.name (protected) */
struct EventNamePeer : handlers::admincommands::Event {
	using Event::name;
};

class EventCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["event"] = 1; // commands.properties
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
	}

	Player& gm(int32_t objectId, std::string_view name = "Warden") {
		Player& player = connected(objectId, name, 9);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	/** PacketSendUtility.sendMessage(player, text, BRIGHT_YELLOW_CENTER) */
	std::vector<uint8_t> centered(std::string_view text, size_t index) {
		return serialized(serverpackets::SM_MESSAGE(0, "", text, model::ChatType::BRIGHT_YELLOW_CENTER), client(index).con());
	}

	std::vector<Player*> stored;
};

/** Event.java:38-123: the syntax, pvpSpawn, clearInstance outside an instance */
TEST_F(EventCommandTest, SyntaxPvpSpawnAndClearInstance) {
	Player& admin = gm(730801);
	handlers::admincommands::Event event;
	for (std::vector<std::string> params : {args({}), args({"setEnemy"}), args({"jump"})}) {
		client()->clearSent();
		EXPECT_TRUE(event.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info(event.getSyntaxInfo())) << params.size();
	}
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"PVPSPAWN", "orc"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid race parameter!"));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"pvpSpawn", "asmo"})));
	EXPECT_EQ(client()->sentBytes(), info("Eventspawn for Asmodians was set!"));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"pvpSpawn", "ELY"})));
	EXPECT_EQ(client()->sentBytes(), info("Eventspawn for Elyos was set!"));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"clearInstance"})));
	EXPECT_EQ(client()->sentBytes(), info("This map is not an instance!"));
}

/** Event.java:76-122, :125-139, :161-177: setStatus, list, setEnemy, setGroupStatus, announce, removeAll */
TEST_F(EventCommandTest, EventStates) {
	Player& admin = gm(730802);
	Player& other = gm(730803, "Other");
	handlers::admincommands::Event event;
	EXPECT_TRUE(event.process(admin, args({"setStatus"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(serverpackets::SM_SYSTEM_MESSAGE::STR_INVALID_TARGET(), client().con())})) << "no target";
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setStatus", "nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(serverpackets::SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"), client().con())}));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setEnemy", "team", "other"})));
	EXPECT_EQ(client()->sentBytes(), info(EventNamePeer::name(other) + " is not in event state"));
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setStatus", "other"})));
	EXPECT_TRUE(other.isInCustomState(CustomPlayerState::EVENT_MODE));
	EXPECT_EQ(client()->sentBytes(), info(EventNamePeer::name(other) + " was set in event state."));
	EXPECT_EQ(client(1)->sentBytes(), exactly({centered("You are in event state now. Please notice that you are not allowed to leave the event without "
													  "removal of this state!", 1)}));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"list"})));
	EXPECT_EQ(client()->sentBytes(), info("Players in event state:\n\t" + EventNamePeer::name(other)));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setEnemy", "x", "other"})));
	EXPECT_EQ(client()->sentBytes(), info(event.getSyntaxInfo())) << "an unknown mode";
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setEnemy", "TEAM", "other"})));
	EXPECT_TRUE(other.isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS));
	EXPECT_TRUE(other.isInFfaTeamMode());
	EXPECT_EQ(client()->sentBytes(), info(EventNamePeer::name(other) + " is in Team-FFA state now."));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setEnemy", "ffa", "other"})));
	EXPECT_FALSE(other.isInFfaTeamMode());
	EXPECT_EQ(client()->sentBytes(), info(EventNamePeer::name(other) + " is in FFA state now."));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setEnemy", "cancel", "other"})));
	EXPECT_FALSE(other.isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS));
	EXPECT_EQ(client()->sentBytes(), info(EventNamePeer::name(other) + " is no longer in FFA state."));
	client()->clearSent();
	EXPECT_TRUE(event.process(admin, args({"setGroupStatus", "other"})));
	EXPECT_EQ(client()->sentBytes(), info("The target is not in a group or alliance!"));
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(event.process(admin, args({"announce", "go", "now"})));
	const std::string announcement = EventNamePeer::name(admin) + ": go now";
	EXPECT_EQ(client()->sentBytes(), exactly({centered(announcement, 0)})) << "the admin";
	EXPECT_EQ(client(1)->sentBytes(), exactly({centered(announcement, 1)})) << "a player in event state";
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(event.process(admin, args({"removeAll"})));
	EXPECT_FALSE(other.isInCustomState(CustomPlayerState::EVENT_MODE));
	EXPECT_EQ(client()->sentBytes(), info(EventNamePeer::name(other) + " was removed from event state.")) << "only the one in event state";
	const std::vector<std::vector<uint8_t>> otherSent = client(1)->sentBytes();
	ASSERT_FALSE(otherSent.empty());
	EXPECT_EQ(otherSent.back(), centered("You were removed from event state!", 1)) << "after onChangedPlayerAttributes' packets";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
