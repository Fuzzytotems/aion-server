// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned commands of M5f): //moveto, //goto, //movetome, //moveplayertoplayer, //movetoobj,
// //instance and the console's ///teleport and ///teleportto on real Players with real AionConnections (CommandTestSupport.h) in a Poeta map
// instance. The texts are the Java literals. The teleports themselves are TeleportService's (tests/playersvc); the cases cover the commands'
// parsing, their fallbacks and their messages, and the moves that stay on the Poeta map.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/handlers/admincommands/GoTo.h"
#include "aion/gameserver/handlers/admincommands/Instance.h"
#include "aion/gameserver/handlers/admincommands/MovePlayerToPlayer.h"
#include "aion/gameserver/handlers/admincommands/MoveTo.h"
#include "aion/gameserver/handlers/admincommands/MoveToMe.h"
#include "aion/gameserver/handlers/admincommands/MoveToObject.h"
#include "aion/gameserver/handlers/consolecommands/Teleport.h"
#include "aion/gameserver/handlers/consolecommands/Teleportto.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

/** npc_templates.xml's minalinerk row (MonsterCommandsTest's), no spawn of it in SPAWNS_DATA */
constexpr std::string_view NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" /></stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
	</npc_template>
</npc_templates>)xml";

class TeleportCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		// commands.properties' levels of the commands under test
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const auto& [alias, level] : std::vector<std::pair<const char*, int8_t>>{{"moveto", 1}, {"goto", 1}, {"movetome", 1}, {"moveplayertoplayer", 9},
				 {"movetoobj", 9}, {"instance", 9}, {"teleport", 9}, {"teleportto", 9}})
			levels[alias] = level;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, "<spawns/>"));
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
	}

	/**
	 * The test Poeta map is 1024 wide (ItemPacketTestSupport.h) and //goto's Poeta locations lie at y 1242 and 1740: World.createPosition
	 * finds no region there and throws. Its message names the map, the instance and the coordinates the command teleported to.
	 */
	template <class Command>
	static std::string teleportTarget(Command& command, Player& admin, std::vector<std::string> params) {
		try {
			command.execute(admin, params);
		} catch (const runtime::NullPointerException& e) {
			return e.what();
		}
		return "no teleport";
	}

	static std::string melponeh() { return "Failed to create position (invalid coords: x=426.0, y=1740.0, z=119.0 for mapId 210010000 in instanceId 1)"; }

	Player& gm(int32_t objectId, std::string_view name = "Warden") {
		Player& player = connected(objectId, name, 9);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	/** whether the UTF-16 text of the packets the client was sent holds `text` (a line of a long sendInfo, which ChatUtil.split cuts at lines) */
	static bool sentText(TestClient& client, std::string_view text) {
		std::string bytes;
		for (const std::vector<uint8_t>& packet : client->sentBytes())
			bytes.append(packet.begin(), packet.end());
		std::string wanted;
		for (char16_t c : commons::utils::StringUtils::toUtf16(text)) {
			wanted += static_cast<char>(c & 0xFF);
			wanted += static_cast<char>(c >> 8);
		}
		return bytes.find(wanted) != std::string::npos;
	}

	static std::string white(std::string_view text) { return utils::ChatUtil::color(text, utils::JavaColor::WHITE); }

	xml::LoadContext context;
	std::vector<Player*> stored;
};

/** ChatCommand.worldName and name (protected) */
struct ChatNamePeer : handlers::admincommands::GoTo {
	using GoTo::name;
	using GoTo::worldName;
};

constexpr int32_t POETA = 210010000;

/** MoveTo.java:37-41, :84-90: no parameter: the syntax; forward: along the heading, z kept */
TEST_F(TeleportCommandsTest, MoveToSyntaxAndForward) {
	Player& admin = gm(730301);
	handlers::admincommands::MoveTo moveTo;
	EXPECT_TRUE(moveTo.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(moveTo.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(moveTo.process(admin, args({"FORWARD", "12.5"})));
	EXPECT_FLOAT_EQ(admin.getX(), 112.5f) << "heading 0: +x";
	EXPECT_FLOAT_EQ(admin.getY(), 100.0f);
	EXPECT_FLOAT_EQ(admin.getZ(), 50.0f);
	admin.getPosition()->setXYZH(100.0f, 100.0f, 50.0f, int8_t{30}); // 90 degrees
	EXPECT_TRUE(moveTo.process(admin, args({"forward", "2"})));
	EXPECT_NEAR(admin.getX(), 100.0f, 1e-4);
	EXPECT_FLOAT_EQ(admin.getY(), 102.0f);
}

/** MoveTo.java:43-81: what is no position, player or npc ends with the last error */
TEST_F(TeleportCommandsTest, MoveToFallbacks) {
	Player& admin = gm(730302);
	handlers::admincommands::MoveTo moveTo;
	EXPECT_TRUE(moveTo.process(admin, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid player name or player is offline.")) << "one word: no position, no player, no npc";
	client()->clearSent();
	EXPECT_TRUE(moveTo.process(admin, args({"Warden"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid player name or player is offline.")) << "himself";
	client()->clearSent();
	const bool geo = configs::main::GeoDataConfig::GEO_ENABLE.load();
	EXPECT_TRUE(moveTo.process(admin, args({"no", "where"})));
	EXPECT_EQ(client()->sentBytes(), info("Could not find \"no where\".")) << "two words: a position that does not parse, a name with a space";
	client()->clearSent();
	EXPECT_TRUE(moveTo.process(admin, args({"[pos:broken"})));
	EXPECT_EQ(client()->sentBytes(), info(std::string("Invalid map position or ") + (geo ? "missing" : "deactivated") + " geo."))
		<< "a broken position link keeps the position error";
	client()->clearSent();
	EXPECT_TRUE(moveTo.process(admin, args({"798007"})));
	EXPECT_EQ(client()->sentBytes(), info("Could not find " + utils::ChatUtil::path(798007, true) + ".")) << "an npc id without a spawn";
	client()->clearSent();
	EXPECT_TRUE(moveTo.process(admin, args({"Minalinerk"})));
	EXPECT_EQ(client()->sentBytes(), info("Could not find " + utils::ChatUtil::path(798007, true) + ".")) << "an npc name (any case)";
	client()->clearSent();
	EXPECT_TRUE(moveTo.process(admin, args({"x=\"12\"", "abc"})));
	EXPECT_EQ(client()->sentBytes(), info("Could not find \"x=\"12\" abc\"."))
		<< "a second coordinate that does not parse: no position; the name with a space replaces the position error";
}

/** GoTo.java:34-65: exact names (any case), abbreviations of one location, ambiguous or unknown ones */
TEST_F(TeleportCommandsTest, GoToMatches) {
	Player& admin = gm(730311);
	handlers::admincommands::GoTo goTo;
	EXPECT_EQ(teleportTarget(goTo, admin, args({"MELPONEHS", "lager"})), melponeh()) << "an exact identifier on the Poeta map: the current map instance";
	EXPECT_EQ(teleportTarget(goTo, admin, args({" poeta "})),
		"Failed to create position (invalid coords: x=806.0, y=1242.0, z=119.0 for mapId 210010000 in instanceId 1)") << "trimmed";
	EXPECT_EQ(teleportTarget(goTo, admin, args({"lponeh"})), melponeh()) << "one match and five or more characters";
	EXPECT_EQ(teleportTarget(goTo, admin, args({"Melp"})), melponeh()) << "one match that starts with the query";
	client()->clearSent();
	EXPECT_TRUE(goTo.process(admin, args({"pon"})));
	EXPECT_FLOAT_EQ(admin.getX(), 100.0f) << "one match, short, not its start";
	EXPECT_EQ(client()->sentBytes(),
		info("Could not find the specified destination. Possible matches:\n\t- " + white("Melponeh") + " (" + ChatNamePeer::worldName(POETA) + ")"));
	client()->clearSent();
	EXPECT_TRUE(goTo.process(admin, args({"obs"})));
	std::string obs = "Could not find the specified destination. Possible matches:";
	for (const auto& [match, worldId] : std::vector<std::pair<std::string, int32_t>>{{"Eltnen Observatory", 210020000}, {"Obsvillage", 210060000},
			 {"Heiron Observatory", 210040000}, {"Altgard Observatory", 220030000}, {"Morheim Observatory", 220020000}})
		obs += "\n\t- " + white(match) + " (" + ChatNamePeer::worldName(worldId) + ")";
	EXPECT_EQ(client()->sentBytes(), info(obs)) << "the first identifier of each location that holds the query, in insertion order";
	client()->clearSent();
	EXPECT_TRUE(goTo.process(admin, args({"x"})));
	EXPECT_EQ(client()->sentBytes(), info("Could not find the specified destination.")) << "a query of one character matches nothing";
	EXPECT_FLOAT_EQ(admin.getX(), 100.0f);
}

/** GoTo.java:94-121: the list groups the locations by map, in insertion order */
TEST_F(TeleportCommandsTest, GoToList) {
	Player& admin = gm(730312);
	handlers::admincommands::GoTo goTo;
	EXPECT_TRUE(goTo.process(admin, args({})));
	EXPECT_TRUE(sentText(client(), "Available locations:\n" + ChatNamePeer::worldName(110010000) + ":\t" + white("Sanctum") + "\n"))
		<< "one location: a tab, no dash";
	const std::string poeta = "\n" + ChatNamePeer::worldName(POETA) + ":\n\t- " + white("Poeta") + "\n\t- " + white("Melponeh") + " / " +
		white("Melponehs Lager") + "\n";
	EXPECT_TRUE(sentText(client(), poeta)) << "a location once per map, all its identifiers";
	EXPECT_TRUE(sentText(client(), white("Wisplight Abbey") + " / " + white("Zuflucht der R\u00fcckkehrer Elyos") + " / " + white("Zuflucht Elyos")));
	EXPECT_TRUE(sentText(client(), "\nType " + white("//goto <location name>") + " to teleport to a location. Location names can be abbreviated."));
	EXPECT_FALSE(sentText(client(), poeta + "\t- " + white("Melponeh"))) << "not once per identifier";
}

/** Teleportto.java:21-30: //goto's syntax under its own alias, //goto's execution */
TEST_F(TeleportCommandsTest, TeleportToIsGoTo) {
	// immortal, like ChatProcessor.init's commands; CommandSmokeTest may have registered one in this process
	if (!utils::chathandlers::ChatProcessor::getInstance().isCommandExists("//goto"))
		utils::chathandlers::ChatProcessor::getInstance().registerCommand(*new handlers::admincommands::GoTo());
	auto* registered = utils::chathandlers::ChatProcessor::getInstance().getCommand<handlers::admincommands::GoTo>();
	ASSERT_NE(registered, nullptr);
	Player& admin = gm(730313);
	handlers::consolecommands::Teleportto teleportto;
	std::string expected = registered->getSyntaxInfo();
	ASSERT_NE(expected.find("//goto"), std::string::npos) << expected;
	for (size_t at = expected.find("//goto"); at != std::string::npos; at = expected.find("//goto", at + 10))
		expected.replace(at, 6, "teleportto"); // ConsoleCommand.PREFIX is ""
	EXPECT_EQ(teleportto.getSyntaxInfo(), expected);
	EXPECT_EQ(teleportTarget(teleportto, admin, args({"melponeh"})), melponeh());
}

/** MoveToMe.java:24-67 */
TEST_F(TeleportCommandsTest, MoveToMe) {
	Player& admin = gm(730321);
	Player& other = gm(730322, "Rover");
	other.getPosition()->setXYZH(300.0f, 310.0f, 60.0f, int8_t{0});
	admin.getPosition()->setXYZH(120.0f, 130.0f, 55.0f, int8_t{0});
	handlers::admincommands::MoveToMe moveToMe;
	EXPECT_TRUE(moveToMe.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(moveToMe.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(moveToMe.process(admin, args({"nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(serverpackets::SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"), client().con())}));
	client()->clearSent();
	EXPECT_TRUE(moveToMe.process(admin, args({"rover", "x"})));
	EXPECT_EQ(client()->sentBytes(), info(moveToMe.getSyntaxInfo())) << "an unknown team type";
	client()->clearSent();
	EXPECT_TRUE(moveToMe.process(admin, args({"rover", "GRP"})));
	EXPECT_EQ(client()->sentBytes(), info(ChatNamePeer::name(other) + " currently has no team."));
	client()->clearSent();
	EXPECT_TRUE(moveToMe.process(admin, args({"rover", "alliance"})));
	EXPECT_EQ(client()->sentBytes(), info(ChatNamePeer::name(other) + " currently has no team."));
	EXPECT_FLOAT_EQ(other.getX(), 300.0f);
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(moveToMe.process(admin, args({"rover"})));
	EXPECT_FLOAT_EQ(other.getX(), 120.0f);
	EXPECT_FLOAT_EQ(other.getY(), 130.0f);
	EXPECT_FLOAT_EQ(other.getZ(), 55.0f);
	EXPECT_TRUE(sentText(client(), "Teleported " + ChatNamePeer::name(other) + " to your location."));
	EXPECT_TRUE(sentText(client(1), "You have been teleported by " + ChatNamePeer::name(admin) + "."));
}

/** MovePlayerToPlayer.java:21-56 */
TEST_F(TeleportCommandsTest, MovePlayerToPlayer) {
	Player& admin = gm(730331);
	Player& mover = gm(730332, "Mover");
	Player& target = gm(730333, "Target");
	target.getPosition()->setXYZH(200.0f, 210.0f, 52.0f, int8_t{40});
	handlers::admincommands::MovePlayerToPlayer move;
	const std::string syntax = "syntax //moveplayertoplayer <characterNameToMove> <characterNameDestination>";
	EXPECT_TRUE(move.process(admin, args({"mover"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message(syntax)}));
	client()->clearSent();
	EXPECT_TRUE(move.process(admin, args({"nobody", "target"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("The specified player is not online.")}));
	client()->clearSent();
	EXPECT_TRUE(move.process(admin, args({"mover", "nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("The destination player is not online.")}));
	client()->clearSent();
	EXPECT_TRUE(move.process(admin, args({"mover", "MOVER"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Cannot move the specified player to their own position.")}));
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(move.process(admin, args({"mover", "target"})));
	EXPECT_FLOAT_EQ(mover.getX(), 200.0f);
	EXPECT_FLOAT_EQ(mover.getY(), 210.0f);
	EXPECT_FLOAT_EQ(mover.getZ(), 52.0f);
	EXPECT_EQ(mover.getHeading(), 40);
	EXPECT_EQ(client()->sentBytes(), exactly({message("Teleported player Mover to the location of player Target.")}));
	EXPECT_TRUE(sentText(client(1), "You have been teleported by an administrator."));
}

/** MoveToObject.java:19-48 */
TEST_F(TeleportCommandsTest, MoveToObject) {
	Player& admin = gm(730341);
	handlers::admincommands::MoveToObject move;
	EXPECT_TRUE(move.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Syntax : //movetoobj <object id>")}));
	client()->clearSent();
	EXPECT_TRUE(move.process(admin, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Only numbers please!!!"), message("Cannot find object for spawn #0")})) << "Java goes on with 0";
	client()->clearSent();
	EXPECT_TRUE(move.process(admin, args({"999999"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Cannot find object for spawn #999999")}));
}

/** Instance.java:26-105 (with the AUTO_GROUP row the registration broadcast reads) */
TEST_F(TeleportCommandsTest, InstanceRegistration) {
	// a row per AutoGroupType mask: getAGTByMaskId walks the constants and reads each template (the registration broadcast reads mask 3's)
	std::string autoGroups = "<auto_groups>";
	for (int32_t mask : {1, 2, 3, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 38, 39, 40, 41, 42, 43, 44, 45, 101, 102, 103, 107, 108,
			 109, 111})
		autoGroups += R"(<auto_group id=")" + std::to_string(mask) + R"(" instanceId=")" + std::to_string(390000000 + mask) +
			R"(" min_lvl="1" max_lvl="65" register_new="true"/>)";
	dataholders::DataManager::AUTO_GROUP.publish(xml::bindString<dataholders::AutoGroupData>(context, autoGroups + "</auto_groups>"));
	Player& admin = gm(730351);
	handlers::admincommands::Instance instance;
	EXPECT_TRUE(instance.process(admin, args({"open"})));
	EXPECT_EQ(client()->sentBytes(), info(instance.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(instance.process(admin, args({"reopen", "dredgion"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "neither open nor close";
	EXPECT_TRUE(instance.process(admin, args({"OPEN", "Nowhere"})));
	EXPECT_EQ(client()->sentBytes(), info("No instance found for Nowhere"));
	client()->clearSent();
	EXPECT_TRUE(instance.process(admin, args({"close", "nowhere"})));
	EXPECT_EQ(client()->sentBytes(), info("No instance found for nowhere"));
	client()->clearSent();
	EXPECT_TRUE(instance.process(admin, args({"close", "DREDGION"})));
	EXPECT_EQ(client()->sentBytes(), info("Registration for TERATH_DREDGION is not open."));
	client()->clearSent();
	EXPECT_TRUE(instance.process(admin, args({"open", "dredgion"})));
	EXPECT_EQ(client()->sentBytes(),
		exactly({serialized(serverpackets::SM_AUTO_GROUP(3, serverpackets::SM_AUTO_GROUP::WND_ENTRY_ICON, false), client().con()),
			serialized(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_OPEN_IDDREADGION_03(), client().con()),
			message("Registration for TERATH_DREDGION is now open.")}));
	client()->clearSent();
	EXPECT_TRUE(instance.process(admin, args({"open", "dredgion"})));
	EXPECT_EQ(client()->sentBytes(), info("Registration for TERATH_DREDGION is already open."));
	client()->clearSent();
	EXPECT_TRUE(instance.process(admin, args({"Close", "dredgion"})));
	EXPECT_EQ(client()->sentBytes(),
		exactly({serialized(serverpackets::SM_AUTO_GROUP(3, serverpackets::SM_AUTO_GROUP::WND_ENTRY_ICON, true), client().con()),
			message("Registration for TERATH_DREDGION is now closed.")}));
	for (const auto& [name, text] : std::vector<std::pair<std::string, std::string>>{{"eob", "ENGULFED_OPHIDIAN_BRIDGE"}, {"id", "IDGEL_DOME"},
			 {"iww", "IRON_WALL_WARFRONT"}, {"kb", "KAMAR_BATTLEFIELD"}}) {
		client()->clearSent();
		EXPECT_TRUE(instance.process(admin, args({"close", name})));
		EXPECT_EQ(client()->sentBytes(), info("Registration for " + text + " is not open.")) << name;
	}
	dataholders::DataManager::AUTO_GROUP.resetForTests();
}

/** Teleport.java (console):28-55: the current map's coordinates, a map by its client name, an unknown client name */
TEST_F(TeleportCommandsTest, ConsoleTeleport) {
	Player& admin = gm(730361);
	handlers::consolecommands::Teleport teleport;
	EXPECT_TRUE(teleport.process(admin, args({"1", "2"})));
	EXPECT_EQ(client()->sentBytes(), info(teleport.getSyntaxInfo()));
	EXPECT_TRUE(teleport.process(admin, args({"321", "432", "77"})));
	EXPECT_FLOAT_EQ(admin.getX(), 321.0f);
	EXPECT_FLOAT_EQ(admin.getY(), 432.0f);
	EXPECT_FLOAT_EQ(admin.getZ(), 77.0f);
	EXPECT_TRUE(teleport.process(admin, args({"LF1", "330", "440", "78"})));
	EXPECT_FLOAT_EQ(admin.getX(), 330.0f) << "Poeta's client name";
	EXPECT_FLOAT_EQ(admin.getZ(), 78.0f);
	EXPECT_TRUE(teleport.process(admin, args({"XX9", "1", "2", "3"})));
	EXPECT_FLOAT_EQ(admin.getX(), 330.0f) << "an unknown client name: world 0, no move";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
