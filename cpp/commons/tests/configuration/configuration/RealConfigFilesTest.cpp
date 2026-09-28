#include <gtest/gtest.h>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/PropertiesUtils.h"

/**
 * Loads the configuration files of the Java servers from this repository, exactly like their Config.loadProperties methods, and binds fields of all
 * special types used by the server configs. Skipped if the Java server directories are not found.
 */

using namespace aion::commons;
using namespace aion::commons::configuration;
namespace fs = std::filesystem;

namespace {

std::optional<fs::path> findRepositoryRoot() {
	for (fs::path dir = fs::path(__FILE__).parent_path(); !dir.empty(); dir = dir.parent_path()) {
		if (fs::is_directory(dir / "game-server" / "config") && fs::is_directory(dir / "login-server" / "config"))
			return dir;
		if (dir == dir.parent_path())
			break;
	}
	return std::nullopt;
}

/**
 * Java: Config.loadProperties of the servers. The my*.properties overrides are per installation and gitignored - m5a-plan.md §8 asks the user to
 * write game-server/config/mygs.properties before playing - so the value expectations below load the committed defaults only (overrideFile
 * nullptr). overridesLoad() keeps the local file covered: whatever it contains, it must still parse.
 */
Properties loadServerProperties(const fs::path& configDir, std::initializer_list<const char*> defaultsFolders, const char* overrideFile = nullptr) {
	auto defaults = std::make_shared<Properties>();
	for (const char* folder : defaultsFolders)
		PropertiesUtils::loadFromDirectory(*defaults, configDir / folder, false);
	if (overrideFile == nullptr)
		return Properties(defaults);
	return PropertiesUtils::load(configDir / overrideFile, defaults);
}

/** The installation's own overrides file, when there is one: it must load on top of the defaults without throwing and keep every default key. */
void overridesLoad(const fs::path& configDir, std::initializer_list<const char*> defaultsFolders, const char* overrideFile, size_t defaultCount) {
	if (!fs::exists(configDir / overrideFile))
		return;
	Properties properties = loadServerProperties(configDir, defaultsFolders, overrideFile);
	EXPECT_GE(properties.stringPropertyNames().size(), defaultCount) << overrideFile << " dropped keys of the defaults";
}

/** Port of com.aionemu.loginserver.configs.Config, as a login server port would write it. */
struct LoginServerConfig {
	static inline utils::InetSocketAddress CLIENT_SOCKET_ADDRESS;
	static inline utils::InetSocketAddress GAMESERVER_SOCKET_ADDRESS;
	static inline int32_t LOGIN_TRY_BEFORE_BAN = 0;
	static inline int32_t WRONG_LOGIN_BAN_TIME = 0;
	static inline int32_t NIO_READ_WRITE_THREADS = 0;
	static inline bool ACCOUNT_AUTO_CREATION = false;
	static inline std::string EXTERNAL_AUTH_URL;
	static inline bool ENABLE_BRUTEFORCE_PROTECTION = false;
	static inline bool LOG_LOGINS = false;

	static void bind(ConfigurableProcessor& p) {
		p.bind("loginserver.network.client.socket_address", CLIENT_SOCKET_ADDRESS, "0.0.0.0:2106");
		p.bind("loginserver.network.gameserver.socket_address", GAMESERVER_SOCKET_ADDRESS, "0.0.0.0:9014");
		p.bind("loginserver.network.client.logintrybeforeban", LOGIN_TRY_BEFORE_BAN, "5");
		p.bind("loginserver.network.client.bantimeforbruteforcing", WRONG_LOGIN_BAN_TIME, "15");
		p.bind("loginserver.network.nio.threads", NIO_READ_WRITE_THREADS, "0");
		p.bind("loginserver.accounts.autocreate", ACCOUNT_AUTO_CREATION, "true");
		p.bind("loginserver.accounts.external_auth.url", EXTERNAL_AUTH_URL, "");
		p.bind("loginserver.server.bruteforceprotector", ENABLE_BRUTEFORCE_PROTECTION, "true");
		p.bind("loginserver.log.logins", LOG_LOGINS, "false");
	}
};

enum class AbyssRankEnum {
	GRADE9_SOLDIER,
	GRADE8_SOLDIER,
	GRADE7_SOLDIER,
	GRADE6_SOLDIER,
	GRADE5_SOLDIER,
	GRADE4_SOLDIER,
	GRADE3_SOLDIER,
	GRADE2_SOLDIER,
	GRADE1_SOLDIER,
	STAR1_OFFICER,
	STAR2_OFFICER,
	STAR3_OFFICER,
	STAR4_OFFICER,
	STAR5_OFFICER,
	GENERAL,
	GREAT_GENERAL,
	COMMANDER,
	SUPREME_COMMANDER
};
enum class HouseType { ESTATE, MANSION, HOUSE, STUDIO, PALACE };
enum class ItemQuality { JUNK, COMMON, RARE, LEGEND, UNIQUE, EPIC, MYTHIC };
enum class NpcRating { JUNK, NORMAL, ELITE, HERO, LEGENDARY };
enum class MultiClientingRestrictionMode { NONE, FULL, SAME_FACTION };

/** Fields of several game server config classes, one of each special type. */
struct GameServerSample {
	std::vector<std::string> NAME_TAGS;
	std::vector<std::string> LOGIN_EXECUTE_COMMANDS;
	std::map<std::string, int8_t> ACCESS_LEVELS;
	std::vector<fs::path> HANDLER_DIRECTORIES;
	std::map<AbyssRankEnum, int32_t> TOP_RANKING_QUOTA;
	AbyssRankEnum XFORM_MIN_RANK = AbyssRankEnum::GRADE9_SOLDIER;
	std::map<HouseType, int32_t> AUCTION_AUTO_FILL_LIMITS;
	std::vector<int32_t> HOUSE_AUCTION_REGISTER_DAYS;
	std::map<int32_t, int32_t> THRESHOLD_MILLIS_BY_PACKET_OPCODE;
	std::optional<ItemQuality> MIN_ANNOUNCE_QUALITY;
	std::set<int32_t> DISABLE_RANGE_CHECK_MAPS{1};
	const std::chrono::time_zone* TIME_ZONE_ID = nullptr;
	std::optional<std::regex> CHAR_NAME_PATTERN;
	std::optional<std::regex> FORBIDDEN_SEQUENCE_PATTERN;
	std::vector<std::string> FORBIDDEN_WORDS;
	std::vector<float> CRAFT_CRIT_CHANCES;
	MultiClientingRestrictionMode MULTI_CLIENTING_RESTRICTION_MODE = MultiClientingRestrictionMode::FULL;
	NpcRating INSTANCE_SCALING_NPC_MIN_RATING = NpcRating::JUNK;
	utils::InetSocketAddress CLIENT_CONNECT_ADDRESS;
	double DOOR_REPAIR_HEAL_PERCENT = 0;
	int8_t GM_PANEL = 0;
	int64_t KINAH_CAP_VALUE = 0;
	std::string RESTART_SCHEDULE = "unset"; // CronExpression in Java (game server transformer)

	void bind(ConfigurableProcessor& p) {
		p.bind("gameserver.administration.customtags", NAME_TAGS,
		       "%s, »JDev«%s, »Dev«%s, »JEM«%s, »EM«%s, »JGM«%s, »GM«%s, »SGM«%s, »Admin«%s");
		p.bind("gameserver.administration.login.execute_commands", LOGIN_EXECUTE_COMMANDS, "//invis, //invul, //enemy none, //see");
		p.bind("gameserver.administration.gm_panel", GM_PANEL, "2");
		p.bindPattern("^[a-zA-Z0-9_]+$", ACCESS_LEVELS);
		p.bind("gameserver.commands.handler_directories", HANDLER_DIRECTORIES,
		       "./data/handlers/admincommands, ./data/handlers/playercommands, ./data/handlers/consolecommands");
		p.bindPattern("^gameserver\\.topranking\\.quota\\.(.+)", TOP_RANKING_QUOTA);
		p.bind("gameserver.topranking.xform.min_rank", XFORM_MIN_RANK, "STAR5_OFFICER");
		p.bindPattern("^gameserver\\.housing\\.auction\\.auto_fill\\.limit\\.(.+)", AUCTION_AUTO_FILL_LIMITS);
		p.bind("gameserver.housing.auction.register_days", HOUSE_AUCTION_REGISTER_DAYS, "1, 5");
		p.bindPattern("^gameserver\\.network\\.pff\\.packet\\.(0[xX][0-9a-fA-F]+)$", THRESHOLD_MILLIS_BY_PACKET_OPCODE);
		p.bind("gameserver.drop.announce_quality", MIN_ANNOUNCE_QUALITY);
		p.bind("gameserver.drop.disable_range_check_maps", DISABLE_RANGE_CHECK_MAPS);
		p.bind("gameserver.timezone", TIME_ZONE_ID);
		p.bind("gameserver.name.character_pattern", CHAR_NAME_PATTERN, "[a-zA-Z]{2,16}");
		p.bind("gameserver.name.forbidden_sequences_pattern", FORBIDDEN_SEQUENCE_PATTERN);
		p.bind("gameserver.name.forbidden_words", FORBIDDEN_WORDS);
		p.bind("gameserver.rates.crafting.crit_chances", CRAFT_CRIT_CHANCES, "15.0, 30.0");
		p.bind("gameserver.security.multi_clienting.restriction_mode", MULTI_CLIENTING_RESTRICTION_MODE, "NONE");
		p.bind("gameserver.instance.scaling.npc_min_rating", INSTANCE_SCALING_NPC_MIN_RATING, "ELITE");
		p.bind("gameserver.network.client.connect_address", CLIENT_CONNECT_ADDRESS, "0.0.0.0:7777");
		p.bind("gameserver.siege.door.repair.heal.percent", DOOR_REPAIR_HEAL_PERCENT, "0.01");
		p.bind("gameserver.kinah.cap.value", KINAH_CAP_VALUE, "999999999");
		p.bind("gameserver.shutdown.restart_schedule", RESTART_SCHEDULE);
	}
};

} // namespace

TEST(RealConfigFilesTest, LoginServer) {
	std::optional<fs::path> root = findRepositoryRoot();
	if (!root)
		GTEST_SKIP() << "Java server directories not found";
	Properties properties = loadServerProperties(*root / "login-server" / "config", {"main", "network"});
	overridesLoad(*root / "login-server" / "config", {"main", "network"}, "myls.properties", properties.stringPropertyNames().size());
	std::set<std::string> unused =
	  ConfigurableProcessor::process(properties, {&LoginServerConfig::bind, &configs::CommonsConfig::bind, &configs::DatabaseConfig::bind});

	// only used in logback.xml, which the Java server removes from the warnings
	EXPECT_EQ(unused, (std::set<std::string>{"loginserver.log.status.discord.avatar_url", "loginserver.log.status.discord.webhook_url"}));
	EXPECT_EQ(LoginServerConfig::CLIENT_SOCKET_ADDRESS, (utils::InetSocketAddress{.host = "0.0.0.0", .port = 2106}));
	EXPECT_EQ(LoginServerConfig::GAMESERVER_SOCKET_ADDRESS, (utils::InetSocketAddress{.host = "0.0.0.0", .port = 9014}));
	EXPECT_EQ(LoginServerConfig::LOGIN_TRY_BEFORE_BAN, 5);
	EXPECT_EQ(LoginServerConfig::WRONG_LOGIN_BAN_TIME, 15);
	EXPECT_TRUE(LoginServerConfig::ACCOUNT_AUTO_CREATION);
	EXPECT_EQ(LoginServerConfig::EXTERNAL_AUTH_URL, "");
	EXPECT_EQ(configs::DatabaseConfig::DATABASE_URL, "jdbc:mysql://localhost:3306/aion_ls?serverTimezone=&characterEncoding=UTF-8");
	EXPECT_EQ(configs::DatabaseConfig::DATABASE_USER, "root");
	EXPECT_EQ(configs::DatabaseConfig::DATABASE_CONNECTIONS_MAX, 5);
}

TEST(RealConfigFilesTest, GameServer) {
	std::optional<fs::path> root = findRepositoryRoot();
	if (!root)
		GTEST_SKIP() << "Java server directories not found";
	Properties properties = loadServerProperties(*root / "game-server" / "config", {"administration", "main", "network"});
	overridesLoad(*root / "game-server" / "config", {"administration", "main", "network"}, "mygs.properties", properties.stringPropertyNames().size());
	EXPECT_GT(properties.stringPropertyNames().size(), 400u);

	GameServerSample sample;
	std::set<std::string> unused = ConfigurableProcessor::process(properties, {[&](ConfigurableProcessor& p) { sample.bind(p); }});
	EXPECT_FALSE(unused.contains("gameserver.topranking.quota.GENERAL"));
	EXPECT_FALSE(unused.contains("access"));

	ASSERT_EQ(sample.NAME_TAGS.size(), 9u);
	EXPECT_EQ(sample.NAME_TAGS[1], "»JDev«%s");
	EXPECT_EQ(sample.LOGIN_EXECUTE_COMMANDS, (std::vector<std::string>{"//invis", "//invul", "//enemy none", "//see"}));
	EXPECT_EQ(sample.ACCESS_LEVELS.at("configure"), 9);
	EXPECT_EQ(sample.HANDLER_DIRECTORIES.size(), 3u);
	EXPECT_EQ(sample.TOP_RANKING_QUOTA.size(), 9u);
	EXPECT_EQ(sample.TOP_RANKING_QUOTA.at(AbyssRankEnum::SUPREME_COMMANDER), 1);
	EXPECT_EQ(sample.XFORM_MIN_RANK, AbyssRankEnum::STAR5_OFFICER);
	EXPECT_EQ(sample.AUCTION_AUTO_FILL_LIMITS.at(HouseType::PALACE), 1);
	EXPECT_EQ(sample.HOUSE_AUCTION_REGISTER_DAYS, (std::vector<int32_t>{1, 5}));
	EXPECT_EQ(sample.MIN_ANNOUNCE_QUALITY, ItemQuality::MYTHIC);
	EXPECT_TRUE(sample.DISABLE_RANGE_CHECK_MAPS.empty());
	EXPECT_EQ(sample.TIME_ZONE_ID, std::chrono::current_zone());
	ASSERT_TRUE(sample.CHAR_NAME_PATTERN);
	EXPECT_TRUE(std::regex_match("Neon", *sample.CHAR_NAME_PATTERN));
	EXPECT_FALSE(sample.FORBIDDEN_SEQUENCE_PATTERN);
	EXPECT_GT(sample.FORBIDDEN_WORDS.size(), 100u);
	EXPECT_EQ(sample.CRAFT_CRIT_CHANCES, (std::vector<float>{15.0f, 30.0f}));
	EXPECT_EQ(sample.MULTI_CLIENTING_RESTRICTION_MODE, MultiClientingRestrictionMode::NONE);
	EXPECT_EQ(sample.INSTANCE_SCALING_NPC_MIN_RATING, NpcRating::ELITE);
	EXPECT_EQ(sample.CLIENT_CONNECT_ADDRESS,
	          (utils::InetSocketAddress{.host = "0.0.0.0", .port = 7777})); // ${gameserver.network.client.socket_address}
	EXPECT_DOUBLE_EQ(sample.DOOR_REPAIR_HEAL_PERCENT, 0.01);
	EXPECT_EQ(sample.KINAH_CAP_VALUE, 999999999);
	EXPECT_EQ(sample.RESTART_SCHEDULE, "");

	ConfigurableProcessor processor(properties);
	configs::DatabaseConfig::bind(processor);
	EXPECT_EQ(configs::DatabaseConfig::DATABASE_URL,
	          "jdbc:mysql://localhost:3306/aion_gs?serverTimezone=&characterEncoding=UTF-8"); // ${gameserver.timezone}
}

TEST(RealConfigFilesTest, ChatServer) {
	std::optional<fs::path> root = findRepositoryRoot();
	if (!root)
		GTEST_SKIP() << "Java server directories not found";
	Properties properties = loadServerProperties(*root / "chat-server" / "config", {"main", "network"});
	overridesLoad(*root / "chat-server" / "config", {"main", "network"}, "mycs.properties", properties.stringPropertyNames().size());
	utils::InetSocketAddress connect, socket, gameserver;
	ConfigurableProcessor::process(properties, {[&](ConfigurableProcessor& p) {
		                               p.bind("chatserver.network.client.connect_address", connect, "0.0.0.0:10241");
		                               p.bind("chatserver.network.client.socket_address", socket, "0.0.0.0:10241");
		                               p.bind("chatserver.network.gameserver.socket_address", gameserver, "0.0.0.0:9021");
	                               }});
	EXPECT_EQ(socket.port, 10241);
	EXPECT_EQ(gameserver.port, 9021);
	EXPECT_EQ(connect.port, 10241);
}
