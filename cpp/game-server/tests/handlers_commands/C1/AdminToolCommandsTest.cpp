// C1 cp1 (lane A, 2026-10-18): the admin tool commands //ahserion, //changerace, //map, //relinquishcraft, //teleportation, //energy, //html,
// //sys, //fsc and //spawnAssembledNpc (data/handlers/admincommands), on real Players with real AionConnections (CommandTestSupport.h) in a
// Poeta map instance. The texts are the Java literals. Left to the gates and the retest: Ahserion's start (the Panesterra raid), //sys
// restart|shutdown, //html reload and show (the HTML cache's files) and //spawnAssembledNpc of a real template (the assembled npc data).

#include "CommandTestSupport.h"

#include <chrono>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/handlers/admincommands/Ahserion.h"
#include "aion/gameserver/handlers/admincommands/ChangeRace.h"
#include "aion/gameserver/handlers/admincommands/EnergyBuff.h"
#include "aion/gameserver/handlers/admincommands/Fsc.h"
#include "aion/gameserver/handlers/admincommands/Html.h"
#include "aion/gameserver/handlers/admincommands/MapCommand.h"
#include "aion/gameserver/handlers/admincommands/RelinquishCraft.h"
#include "aion/gameserver/handlers/admincommands/SpawnAssembledNpc.h"
#include "aion/gameserver/handlers/admincommands/Sys.h"
#include "aion/gameserver/handlers/admincommands/Teleportation.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using serverpackets::SM_SYSTEM_MESSAGE;

class AdminToolCommandsTest : public CommandTest {
protected:
	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 9);
		spawnInPoeta(player);
		return player;
	}
};

/** Ahserion.java:21-43: no parameters is the syntax; stop while no flight runs tells it */
TEST_F(AdminToolCommandsTest, AhserionStopsOnlyARunningFlight) {
	Player& admin = gm(733000);
	handlers::admincommands::Ahserion command;
	command.execute(admin, args({}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(admin, args({"STOP"}));
	EXPECT_EQ(client()->sentBytes(), info("Ahserion's Flight is not running."));
	client()->clearSent();
	command.execute(admin, args({"other"}));
	EXPECT_TRUE(client()->sentBytes().empty());
}

/** ChangeRace.java:15-20: the GM's race becomes the opposite one */
TEST_F(AdminToolCommandsTest, ChangeRaceSwitchesTheFaction) {
	Player& admin = gm(733010);
	admin.getCommonData()->setRace(model::Race::ELYOS);
	handlers::admincommands::ChangeRace command;
	command.execute(admin, args({}));
	EXPECT_EQ(admin.getRace(), model::Race::ASMODIANS);
	command.execute(admin, args({}));
	EXPECT_EQ(admin.getRace(), model::Race::ELYOS);
}

/** MapCommand.java:24-42: freeze tells the frozen map and its walkers, unfreeze the unfrozen map */
TEST_F(AdminToolCommandsTest, MapFreezesAndUnfreezes) {
	Player& admin = gm(733020);
	handlers::admincommands::MapCommand command;
	command.execute(admin, args({}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(admin, args({"Freeze"}));
	std::vector<std::vector<uint8_t>> expected = info("World map is frozen!");
	for (std::vector<uint8_t>& part : info("There are 0 walkers remaining on map 210010000"))
		expected.push_back(std::move(part));
	EXPECT_EQ(client()->sentBytes(), expected);
	client()->clearSent();
	command.execute(admin, args({"unfreeze"}));
	EXPECT_EQ(client()->sentBytes(), info("World map is unfrozen!"));
}

/** RelinquishCraft.java:24-62: the syntax, an unknown player, a gathering skill id, a crafting skill the player is no expert of */
TEST_F(AdminToolCommandsTest, RelinquishCraftChecksItsArguments) {
	Player& admin = gm(733030);
	handlers::admincommands::RelinquishCraft command;
	command.execute(admin, args({"40001"}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(admin, args({"Nobody", "40001", "expert"}));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"), client().con())}));
	client()->clearSent();
	command.execute(admin, args({"30002", "expert"}));
	EXPECT_EQ(client()->sentBytes(), info("Invalid skill ID.")) << "essence tapping is no crafting skill";
	client()->clearSent();
	command.execute(admin, args({"40001", "expert"}));
	EXPECT_EQ(client()->sentBytes(), info(utils::ChatUtil::charName(admin) + " doesn't have COOKING on expert."));
	client()->clearSent();
	command.execute(admin, args({"40001", "master"}));
	EXPECT_EQ(client()->sentBytes(), info(utils::ChatUtil::charName(admin) + " doesn't have COOKING on master."));
	client()->clearSent();
	command.execute(admin, args({"40001", "other"}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
}

/** Teleportation.java:18-25: it toggles the mode and tells the new one */
TEST_F(AdminToolCommandsTest, TeleportationToggles) {
	Player& admin = gm(733040);
	handlers::admincommands::Teleportation command;
	command.execute(admin, args({}));
	EXPECT_TRUE(admin.isInCustomState(CustomPlayerState::TELEPORTATION_MODE));
	EXPECT_EQ(client()->sentBytes(), info("Teleportation mode is now " + utils::ChatUtil::color("active", utils::JavaColor::GREEN) + "."));
	client()->clearSent();
	command.execute(admin, args({}));
	EXPECT_FALSE(admin.isInCustomState(CustomPlayerState::TELEPORTATION_MODE));
	EXPECT_EQ(client()->sentBytes(), info("Teleportation mode is now " + utils::ChatUtil::color("inactive", utils::JavaColor::RED) + "."));
}

/** EnergyBuff.java:18-53: the syntax, the repose info and its add and reset, the salvation reset */
TEST_F(AdminToolCommandsTest, EnergyShowsAndChangesTheEnergies) {
	Player& admin = gm(733050);
	handlers::admincommands::EnergyBuff command;
	const std::vector<uint8_t> syntax = message("//energy repose|salvation|refresh info|reset|add [points]");
	command.execute(admin, args({}));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	command.execute(admin, args({"repose"}));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	command.execute(admin, args({"repose", "add"}));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	admin.getCommonData()->setPlayerClass(model::PlayerClass::GLADIATOR); // a daeva past level 9: repose energy needs level 10
	admin.getCommonData()->setLevel(12);
	admin.getCommonData()->updateMaxRepose();
	ASSERT_GT(admin.getCommonData()->getMaxReposeEnergy(), 1234);
	admin.getCommonData()->setCurrentReposeEnergy(0);
	command.execute(admin, args({"repose", "add", "1234"}));
	EXPECT_EQ(admin.getCommonData()->getCurrentReposeEnergy(), 1234);
	client()->clearSent();
	command.execute(admin, args({"repose", "info"}));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Current EoR: " + std::to_string(admin.getCommonData()->getCurrentReposeEnergy()) +
	                                                  "\n Max EoR: " + std::to_string(admin.getCommonData()->getMaxReposeEnergy()))}));
	command.execute(admin, args({"repose", "reset"}));
	EXPECT_EQ(admin.getCommonData()->getCurrentReposeEnergy(), 0);
	client()->clearSent();
	command.execute(admin, args({"salvation", "info"}));
	EXPECT_EQ(client()->sentBytes(),
		exactly({message("Current EoS: " + std::to_string(admin.getCommonData()->getCurrentSalvationPercent()))}));
}

/** Html.java:18-32: the usages of no parameters and of show without a file name */
TEST_F(AdminToolCommandsTest, HtmlTellsItsUsage) {
	Player& admin = gm(733060);
	handlers::admincommands::Html command;
	command.execute(admin, args({}));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Usage: //html <reload|show>")}));
	client()->clearSent();
	command.execute(admin, args({"show"}));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Usage: //html show <filename>")}));
	client()->clearSent();
	command.execute(admin, args({"other"}));
	EXPECT_TRUE(client()->sentBytes().empty());
}

/** Sys.java:30-60: the syntax for nothing or an unknown argument; info starts with the time; the thread pool's stats, one per line */
TEST_F(AdminToolCommandsTest, SysShowsTheSystem) {
	Player& admin = gm(733070);
	handlers::admincommands::Sys command;
	command.execute(admin, args({}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(admin, args({"other"}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(admin, args({"threadpool"}));
	std::vector<std::vector<uint8_t>> expected;
	for (std::string stat : utils::ThreadPoolManager::getInstance().getStats()) {
		std::erase(stat, '\t');
		for (std::vector<uint8_t>& part : info(stat))
			expected.push_back(std::move(part));
	}
	EXPECT_EQ(client()->sentBytes(), expected);
	EXPECT_FALSE(expected.empty());
	client()->clearSent();
	const std::chrono::time_zone* previousZone = configs::main::GSConfig::TIME_ZONE_ID.exchange(std::chrono::locate_zone("UTC")); // ServerTime's zone
	command.execute(admin, args({"INFO"}));
	configs::main::GSConfig::TIME_ZONE_ID.store(previousZone);
	ASSERT_GE(client()->sentBytes().size(), 3u);
}

/** Fsc.java:18-36: too few parameters; an element code the packet type has not got throws (Java: null at writeImpl); values short of the
 * format throw */
TEST_F(AdminToolCommandsTest, FscChecksItsFormat) {
	Player& admin = gm(733080);
	handlers::admincommands::Fsc command;
	command.execute(admin, args({"0x10", "d"}));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Incorrent number of params in //fsc command")}));
	client()->clearSent();
	EXPECT_THROW(command.execute(admin, args({"0x10", "Z", "1"})), runtime::NullPointerException);
	EXPECT_THROW(command.execute(admin, args({"0x10", "dd", "1"})), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_TRUE(client()->sentBytes().empty());
	command.execute(admin, args({"0x10", "d", "1"}));
	EXPECT_EQ(client()->sentBytes().size(), 1u) << "the custom packet";
}

/** SpawnAssembledNpc.java:20-33: one numeric spawn id or the syntax */
TEST_F(AdminToolCommandsTest, SpawnAssembledNpcWantsOneSpawnId) {
	Player& admin = gm(733090);
	handlers::admincommands::SpawnAssembledNpc command;
	const std::vector<uint8_t> syntax = message("syntax //spawnAssembledNpc <spawnId>");
	command.execute(admin, args({}));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	command.execute(admin, args({"1", "2"}));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	command.execute(admin, args({"x"}));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
