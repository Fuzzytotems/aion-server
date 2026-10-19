// C2 cp4 (lane A, 2026-10-18): //configure (data/handlers/admincommands/Configure.java) on a real Player with a real AionConnection
// (CommandTestSupport.h). The config classes are the real ones, introspected through ConfigurableProcessor::describe; every case that changes a
// field restores it. The texts are the Java literals, plus the C++ port's refusal for a field that cannot change at runtime.

#include "CommandTestSupport.h"

#include <map>
#include <string>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/TransformationException.h"
#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/handlers/admincommands/Configure.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using configs::detail::AbyssRankEnum;
using configs::main::HTMLConfig;
using configs::main::PeriodicSaveConfig;
using configs::main::RankingConfig;

class ConfigureCommandTest : public CommandTest {
protected:
	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 9);
		spawnInPoeta(player);
		return player;
	}

	/** runs //configure with the given parameters and returns the packets it sent, clearing them */
	std::vector<std::vector<uint8_t>> run(Player& admin, std::initializer_list<std::string_view> params) {
		client()->clearSent();
		command.execute(admin, args(params));
		std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
		client()->clearSent();
		return sent;
	}

	/** the message of the cause of the TransformationException that `transform` throws */
	template <class Transform>
	static std::string causeOf(Transform transform) {
		try {
			transform();
		} catch (const commons::configuration::TransformationException& e) {
			try {
				std::rethrow_exception(e.cause());
			} catch (const std::exception& cause) {
				return cause.what();
			}
		}
		return "";
	}

	handlers::admincommands::Configure command;
};

/** Configure.java:34-37: without parameters it sends the syntax */
TEST_F(ConfigureCommandTest, NoParametersSendTheSyntax) {
	Player& admin = gm(735000);
	EXPECT_EQ(run(admin, {}), info(command.getSyntaxInfo()));
}

/** Configure.java:39-46: the categories, sorted case-insensitively, are the lower-cased simple names without "config" */
TEST_F(ConfigureCommandTest, ListShowsTheCategoriesInCaseInsensitiveOrder) {
	Player& admin = gm(735001);
	EXPECT_EQ(run(admin, {"LIST"}),
		info("List of available configuration names:\n\tadmin\n\tai\n\tautogroup\n\tcleaning\n\tcommands\n\tcommons\n\tcraft\n\tcustom"
				  "\n\tdatabase\n\tdrop\n\tevents\n\tfalldamage\n\tgeodata\n\tgroup\n\tgs\n\thousing\n\thtml\n\tinstance\n\tlegion\n\tlogging"
				  "\n\tmembership\n\tname\n\tnetwork\n\tperiodicsave\n\tpff\n\tplayertransfer\n\tprices\n\tpunishment\n\tranking\n\trates"
				  "\n\truntime\n\tsecurity\n\tshutdown\n\tsiege\n\tthread\n\tworld"));
}

/** Configure.java:48-52 and 66-69: an unknown category and an unknown field */
TEST_F(ConfigureCommandTest, AnUnknownCategoryOrFieldIsTold) {
	Player& admin = gm(735002);
	EXPECT_EQ(run(admin, {"nothing"}),
		info("Invalid configuration name. You can get a list of available configuration categories via the <list> parameter."));
	EXPECT_EQ(run(admin, {"PeriodicSave", "player_nothing"}), info("PeriodicSaveConfig.PLAYER_NOTHING does not exist."));
	// Java upper-cases the field name, so a field with lower-case letters is never found
	EXPECT_EQ(run(admin, {"network", "Flood_Tick"}), info("NetworkConfig.FLOOD_TICK does not exist."));
}

/** Configure.java:53-63: a category lists its fields in bind order with their values */
TEST_F(ConfigureCommandTest, ACategoryListsItsFieldsWithTheirValues) {
	Player& admin = gm(735003);
	const int32_t general = PeriodicSaveConfig::PLAYER_GENERAL.exchange(900);
	const int32_t pets = PeriodicSaveConfig::PLAYER_PETS.exchange(-10);
	EXPECT_EQ(run(admin, {"periodicsave"}), info("List of available properties for PeriodicSaveConfig:\n\tPLAYER_GENERAL\t=\t900\n\tPLAYER_ITEMS\t=\t" +
														  std::to_string(PeriodicSaveConfig::PLAYER_ITEMS.load()) + "\n\tLEGION_ITEMS\t=\t" +
														  std::to_string(PeriodicSaveConfig::LEGION_ITEMS.load()) + "\n\tPLAYER_PETS\t=\t-10"));
	PeriodicSaveConfig::PLAYER_GENERAL = general;
	PeriodicSaveConfig::PLAYER_PETS = pets;
}

/** a bind without a name binds a local (CommonsConfig's script compiler flag), not a field: the listing leaves it out */
TEST_F(ConfigureCommandTest, ACategoryListsOnlyNamedBinds) {
	Player& admin = gm(735008);
	const bool stats = commons::configs::CommonsConfig::RUNNABLESTATS_ENABLE.exchange(true);
	EXPECT_EQ(run(admin, {"commons"}), info("List of available properties for CommonsConfig:\n\tRUNNABLESTATS_ENABLE\t=\ttrue"));
	commons::configs::CommonsConfig::RUNNABLESTATS_ENABLE = stats;
}

/** Configure.java:70-87: a field shows its value, and a new value changes it (an atomic and a ConfigValue) */
TEST_F(ConfigureCommandTest, AFieldShowsAndChangesItsValue) {
	Player& admin = gm(735004);
	const int32_t general = PeriodicSaveConfig::PLAYER_GENERAL.exchange(900);
	EXPECT_EQ(run(admin, {"periodicsave", "player_general"}), info("The current value of PeriodicSaveConfig.PLAYER_GENERAL is 900"));
	EXPECT_EQ(run(admin, {"periodicsave", "player_general", "120"}),
		info("The value of PeriodicSaveConfig.PLAYER_GENERAL has been changed from 900 to 120"));
	EXPECT_EQ(PeriodicSaveConfig::PLAYER_GENERAL.load(), 120);
	PeriodicSaveConfig::PLAYER_GENERAL = general;

	const AbyssRankEnum rank = RankingConfig::XFORM_MIN_RANK.exchange(AbyssRankEnum::GRADE9_SOLDIER);
	EXPECT_EQ(run(admin, {"ranking", "xform_min_rank", "GRADE8_SOLDIER"}),
		info("The value of RankingConfig.XFORM_MIN_RANK has been changed from GRADE9_SOLDIER to GRADE8_SOLDIER"));
	EXPECT_EQ(RankingConfig::XFORM_MIN_RANK.load(), AbyssRankEnum::GRADE8_SOLDIER);
	RankingConfig::XFORM_MIN_RANK = rank;

	std::shared_ptr<const std::string> root = HTMLConfig::HTML_ROOT.get();
	HTMLConfig::HTML_ROOT.set(std::string("./data/static_data/HTML/"));
	// the value is the parameters from the third on, joined by spaces
	EXPECT_EQ(run(admin, {"html", "html_root", "./a", "b/"}),
		info("The value of HTMLConfig.HTML_ROOT has been changed from ./data/static_data/HTML/ to ./a b/"));
	EXPECT_EQ(*HTMLConfig::HTML_ROOT.get(), "./a b/");
	HTMLConfig::HTML_ROOT.set(root ? *root : std::string());
}

/** Configure.java:76-79: a value that cannot be transformed leaves the field and tells the cause */
TEST_F(ConfigureCommandTest, AnInvalidValueTellsTheCauseAndKeepsTheField) {
	Player& admin = gm(735005);
	const int32_t general = PeriodicSaveConfig::PLAYER_GENERAL.exchange(900);
	std::string cause;
	try {
		commons::configuration::ConfigurableProcessor::transform<int32_t>("often");
	} catch (const commons::configuration::TransformationException& e) {
		try {
			std::rethrow_exception(e.cause());
		} catch (const std::exception& c) {
			cause = c.what();
		}
	}
	ASSERT_FALSE(cause.empty());
	EXPECT_EQ(run(admin, {"periodicsave", "player_general", "often"}), info("The new value could not be set: " + cause));
	EXPECT_EQ(PeriodicSaveConfig::PLAYER_GENERAL.load(), 900);
	PeriodicSaveConfig::PLAYER_GENERAL = general;
}

/** Configure.java:74-75 and 97-115: a pattern field takes key=value pairs, in braces and by commas or by spaces */
TEST_F(ConfigureCommandTest, APatternFieldTakesKeyValuePairs) {
	Player& admin = gm(735006);
	std::shared_ptr<const std::map<AbyssRankEnum, int32_t>> quota = RankingConfig::TOP_RANKING_QUOTA.get();
	RankingConfig::TOP_RANKING_QUOTA.set(std::map<AbyssRankEnum, int32_t>{{AbyssRankEnum::GRADE9_SOLDIER, 1}});
	EXPECT_EQ(run(admin, {"ranking", "top_ranking_quota", "{GRADE8_SOLDIER=5", ",", "GRADE9_SOLDIER", "=", "7}"}),
		info("The value of RankingConfig.TOP_RANKING_QUOTA has been changed from {GRADE9_SOLDIER=1} to {GRADE9_SOLDIER=7, GRADE8_SOLDIER=5}"));
	EXPECT_EQ(run(admin, {"ranking", "top_ranking_quota", "GRADE7_SOLDIER=2", "GRADE9_SOLDIER=3"}),
		info("The value of RankingConfig.TOP_RANKING_QUOTA has been changed from {GRADE9_SOLDIER=7, GRADE8_SOLDIER=5} to "
				  "{GRADE9_SOLDIER=3, GRADE7_SOLDIER=2}"));
	EXPECT_EQ(run(admin, {"ranking", "top_ranking_quota", "GRADE7_SOLDIER=2", "GRADE9_SOLDIER"}),
		info("The new value could not be set: Missing value after GRADE9_SOLDIER (format: key=value)"));
	EXPECT_EQ(RankingConfig::TOP_RANKING_QUOTA.get()->size(), 2u);
	// Java's String.split drops the trailing empty entry, and a value of separators only is no entry at all
	EXPECT_EQ(run(admin, {"ranking", "top_ranking_quota", "GRADE6_SOLDIER=4,"}),
		info("The value of RankingConfig.TOP_RANKING_QUOTA has been changed from {GRADE9_SOLDIER=3, GRADE7_SOLDIER=2} to {GRADE6_SOLDIER=4}"));
	EXPECT_EQ(run(admin, {"ranking", "top_ranking_quota", "{", "}"}),
		info("The value of RankingConfig.TOP_RANKING_QUOTA has been changed from {GRADE6_SOLDIER=4} to {}"));
	EXPECT_EQ(run(admin, {"ranking", "top_ranking_quota", "=5"}),
		info("The new value could not be set: " + causeOf([] { commons::configuration::ConfigurableProcessor::transformMap<std::map<AbyssRankEnum, int32_t>>(
															 std::map<std::string, std::string>{{"", "5"}}); })));
	RankingConfig::TOP_RANKING_QUOTA.set(quota ? *quota : std::map<AbyssRankEnum, int32_t>());
}

/** C++ port: a plain field (neither std::atomic nor ConfigValue) shows its value but refuses a new one (docs/deviations/C2.md, C2 cp4) */
TEST_F(ConfigureCommandTest, APlainFieldCannotBeChangedAtRuntime) {
	Player& admin = gm(735007);
	using commons::configs::DatabaseConfig;
	const std::string url = DatabaseConfig::DATABASE_URL;
	DatabaseConfig::DATABASE_URL = "jdbc:mariadb://localhost:3306/aion_gs";
	EXPECT_EQ(run(admin, {"database", "database_url"}),
		info("The current value of DatabaseConfig.DATABASE_URL is jdbc:mariadb://localhost:3306/aion_gs"));
	EXPECT_EQ(run(admin, {"database", "database_url", "jdbc:mariadb://elsewhere/aion"}),
		info("DatabaseConfig.DATABASE_URL cannot be changed at runtime in the C++ port."));
	EXPECT_EQ(DatabaseConfig::DATABASE_URL, "jdbc:mariadb://localhost:3306/aion_gs");
	const int32_t timeout = DatabaseConfig::DATABASE_TIMEOUT;
	EXPECT_EQ(run(admin, {"database", "database_timeout", "1"}), info("DatabaseConfig.DATABASE_TIMEOUT cannot be changed at runtime in the C++ port."));
	EXPECT_EQ(DatabaseConfig::DATABASE_TIMEOUT, timeout);
	const std::optional<int32_t> socketTimeout = DatabaseConfig::DATABASE_SOCKET_TIMEOUT;
	DatabaseConfig::DATABASE_SOCKET_TIMEOUT.reset();
	EXPECT_EQ(run(admin, {"database", "database_socket_timeout"}), info("The current value of DatabaseConfig.DATABASE_SOCKET_TIMEOUT is null"));
	DatabaseConfig::DATABASE_SOCKET_TIMEOUT = 30;
	EXPECT_EQ(run(admin, {"database", "database_socket_timeout"}), info("The current value of DatabaseConfig.DATABASE_SOCKET_TIMEOUT is 30"));
	DatabaseConfig::DATABASE_SOCKET_TIMEOUT = socketTimeout;
	DatabaseConfig::DATABASE_URL = url;
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
