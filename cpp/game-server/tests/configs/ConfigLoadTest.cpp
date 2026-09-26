#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/PropertiesUtils.h"
#include "aion/commons/configuration/TransformationException.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/configs/main/ShutdownConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/configs/network/PffConfig.h"

#include "ConfigTestSupport.h"

/**
 * Config::load against a copy of the shipped Java game server config directory (game-server/config), with and without mygs.properties and event
 * properties, including runtime rebinding.
 */

using namespace aion::gameserver::configs;
using namespace aion::gameserver::configs::test;
using aion::commons::configs::CommonsConfig;
using aion::commons::configs::DatabaseConfig;
using aion::commons::configuration::ConfigurableProcessor;
using aion::commons::configuration::Properties;
using aion::commons::utils::InetSocketAddress;
namespace fs = std::filesystem;

namespace {

const char* const CONFIG_LOGGER = "com.aionemu.gameserver.configs.Config";
const char* const TEST_LOCAL_IP = "192.0.2.10";

/** Copies the shipped config into a temp directory and loads from there, with a fixed local IPv4 address for the connect address discovery. */
class ConfigLoadTest : public ::testing::Test {
protected:
	void SetUp() override {
		ASSERT_NO_FATAL_FAILURE(copyShippedConfig(dir));
		Config::setLocalIPv4Finder([] { return std::optional<std::string>(TEST_LOCAL_IP); });
	}

	void TearDown() override {
		Config::setEventConfigPropertiesProvider(nullptr);
		Config::setLocalIPv4Finder(nullptr);
	}

	/** Config::load(allowedConfigs) in the temp directory, capturing the Config logger */
	std::string load(std::initializer_list<Config::BindFunction> allowedConfigs = {}) {
		LogCapture log(CONFIG_LOGGER);
		ScopedCurrentPath cwd(dir.path);
		Config::load(allowedConfigs);
		return log.str();
	}

	void writeMygs(std::string_view content) const { dir.write("config/mygs.properties", content); }

	TempDirectory dir;
};

bool matches(const ConfigValue<std::wregex>& pattern, const std::wstring& text) {
	auto regex = pattern.get();
	return std::regex_match(text, *regex);
}

/** a bind function that is not one of Config::getClasses() */
void notAConfig(ConfigurableProcessor&) {}

} // namespace

TEST_F(ConfigLoadTest, ShippedConfigHasNoUnknownProperties) {
	std::string log = load();

	EXPECT_EQ(log.find("is unknown and therefore ignored"), std::string::npos) << log;
	EXPECT_NE(log.find("info|Loading default configuration values from: ./config/administration/*\n"), std::string::npos) << log;
	EXPECT_NE(log.find("info|Loading default configuration values from: ./config/main/*\n"), std::string::npos) << log;
	EXPECT_NE(log.find("info|Loading default configuration values from: ./config/network/*\n"), std::string::npos) << log;
	EXPECT_NE(log.find("info|Loading: ./config/mygs.properties\n"), std::string::npos) << log;
	EXPECT_NE(log.find("info|No override properties found\n"), std::string::npos) << log;
	EXPECT_NE(log.find(std::string("info|No IP for Aion client advertisement configured, using ") + TEST_LOCAL_IP + "\n"), std::string::npos) << log;

	// the only properties no config class binds are the ones logback.xml reads in Java
	Properties properties;
	auto defaults = std::make_shared<Properties>();
	for (const char* folder : {"administration", "main", "network"})
		aion::commons::configuration::PropertiesUtils::loadFromDirectory(*defaults, dir.path / "config" / folder, false);
	properties = Properties(defaults);
	std::vector<ConfigurableProcessor::Binder> binders;
	for (const Config::ConfigClass& config : Config::getClasses())
		binders.emplace_back(config.bind);
	std::set<std::string> unused = ConfigurableProcessor::process(properties, binders);
	std::vector<std::string> loggingKeys = aion::commons::logging::Logging::getPropertyKeys("gameserver");
	EXPECT_EQ(unused, std::set<std::string>(loggingKeys.begin(), loggingKeys.end()));
	EXPECT_GT(properties.stringPropertyNames().size(), 560u);
}

TEST_F(ConfigLoadTest, ParsedValuesOfShippedConfig) {
	load();

	// values that differ from the Java defaults
	EXPECT_TRUE(CommonsConfig::RUNNABLESTATS_ENABLE.load());
	EXPECT_EQ(main::GSConfig::CHARACTER_REENTRY_TIME.load(), 10);
	EXPECT_EQ(main::CustomConfig::VORTEX_DURATION.load(), 2);
	EXPECT_TRUE(main::CustomConfig::CHALLENGE_TASKS_ENABLED.load());
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL2_REQUIRED_MEMBERS.load(), 6);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL8_REQUIRED_MEMBERS.load(), 6);
	EXPECT_EQ(main::SecurityConfig::SPEEDHACK_COUNTER.load(), 5);
	EXPECT_TRUE(main::SiegeConfig::BALAUR_AUTO_ASSAULT.load());
	EXPECT_EQ(*network::NetworkConfig::LOGIN_PASSWORD.get(), "1234");
	EXPECT_FALSE(main::LoggingConfig::LOG_CRAFT.load());

	// one field of each type
	EXPECT_EQ(main::GSConfig::SERVER_COUNTRY_CODE.load(), 99);
	EXPECT_EQ(main::GSConfig::CHAT_SERVER_MIN_LEVEL.load(), 10);
	EXPECT_EQ(main::CustomConfig::KINAH_CAP_VALUE.load(), 999999999);
	EXPECT_FLOAT_EQ(main::HousingConfig::VISIBILITY_DISTANCE.load(), 200.0f);
	EXPECT_FLOAT_EQ(main::HousingConfig::AUCTION_REGISTRATION_FEE_PERCENT.load(), 0.3f);
	EXPECT_DOUBLE_EQ(main::SiegeConfig::DOOR_REPAIR_HEAL_PERCENT.load(), 0.01);
	EXPECT_EQ(*main::PlayerTransferConfig::BIND_ELYOS.get(), "210010000 1212.9423 1044.8516 140.75568 32");
	EXPECT_EQ(main::GSConfig::TIME_ZONE_ID.load(), std::chrono::current_zone()); // empty value: system time zone
	EXPECT_EQ(*main::GSConfig::QUEST_HANDLER_DIRECTORY.get(), fs::path("./data/handlers/quest"));
	EXPECT_EQ(main::AIConfig::HANDLER_DIRECTORY.get()->filename(), fs::path("ai"));
	EXPECT_EQ(administration::CommandsConfig::HANDLER_DIRECTORIES.get()->size(), 3u);
	const auto nameTags = administration::AdminConfig::NAME_TAGS.get();
	ASSERT_EQ(nameTags->size(), 9u);
	EXPECT_EQ((*nameTags)[0], "%s");
	EXPECT_EQ((*nameTags)[8], "»Admin«%s");
	EXPECT_EQ(*administration::AdminConfig::LOGIN_EXECUTE_COMMANDS.get(), (std::vector<std::string>{"//invis", "//invul", "//enemy none", "//see"}));
	EXPECT_EQ(*administration::AdminConfig::ANNOUNCE_LEVELS.get(), (std::vector<std::string>{"*"}));
	EXPECT_EQ(*main::CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS.get(),
	          (std::unordered_set<int32_t>{210020000, 210040000, 210050000, 210070000, 220020000, 220040000, 220070000, 220080000}));
	EXPECT_TRUE(main::DropConfig::DISABLE_RANGE_CHECK_MAPS.get()->empty());
	EXPECT_TRUE(main::EventsConfig::DISABLED_EVENTS.get()->empty());
	EXPECT_TRUE(main::SecurityConfig::MULTI_CLIENTING_IGNORED_MAC_ADDRESSES.get()->empty());
	EXPECT_EQ(*main::HousingConfig::HOUSE_AUCTION_REGISTER_DAYS.get(), (std::vector<int32_t>{1, 5}));
	EXPECT_EQ(*main::RatesConfig::CRAFT_CRIT_CHANCES.get(), (std::vector<float>{15.0f, 30.0f}));
	EXPECT_EQ(*main::RatesConfig::AP_PVP_LOSS_RATES.get(), (std::vector<float>{1.0f, 1.0f}));
	EXPECT_EQ(main::DropConfig::MIN_ANNOUNCE_QUALITY.load(), detail::ItemQuality::MYTHIC);
	EXPECT_EQ(main::InstanceConfig::INSTANCE_SCALING_NPC_MIN_RATING.load(), detail::NpcRating::ELITE);
	EXPECT_EQ(main::RankingConfig::XFORM_MIN_RANK.load(), detail::AbyssRankEnum::STAR5_OFFICER);
	EXPECT_EQ(main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE.load(), main::SecurityConfig::MultiClientingRestrictionMode::NONE);
	EXPECT_EQ(*network::NetworkConfig::CLIENT_SOCKET_ADDRESS.get(), (InetSocketAddress{"0.0.0.0", 7777}));
	EXPECT_EQ(*network::NetworkConfig::LOGIN_ADDRESS.get(), (InetSocketAddress{"localhost", 9014}));
	// ${gameserver.network.client.socket_address} = 0.0.0.0:7777, replaced by the discovered local address
	EXPECT_EQ(*network::NetworkConfig::CLIENT_CONNECT_ADDRESS.get(), (InetSocketAddress{TEST_LOCAL_IP, 7777}));
	EXPECT_EQ(DatabaseConfig::DATABASE_URL, "jdbc:mysql://localhost:3306/aion_gs?serverTimezone=&characterEncoding=UTF-8");

	// CronExpression and CronExpression[] (quoted values containing commas)
	EXPECT_EQ(cronText(main::CustomConfig::VORTEX_BRUSTHONIN_SCHEDULE.load()), "0 0 16 ? * SAT");
	EXPECT_EQ(cronText(main::CustomConfig::PVP_MAP_RANDOM_BOSS_SCHEDULE.load()), "0 30 14,18,21 ? * *");
	EXPECT_EQ(cronText(main::SiegeConfig::AHSERION_START_SCHEDULE.load()), "0 50 18 ? * SUN");
	EXPECT_EQ(main::ShutdownConfig::RESTART_SCHEDULE.load(), nullptr); // empty value: null
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::DREDGION_TIMES.get()), (std::vector<std::string>{"0 0 0,12,20 ? * *"}));
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::KAMAR_BATTLEFIELD_TIMES.get()), (std::vector<std::string>{"0 0 0,20 ? * MON,WED,SAT"}));
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::IDGEL_DOME_TIMES.get()), (std::vector<std::string>{"0 0 23 ? * *"}));
	// interned: equal texts share one parsed expression
	EXPECT_EQ(main::CustomConfig::LIMITS_UPDATE.load(), main::RankingConfig::TOP_RANKING_UPDATE_RULE.load());

	// patterns (std::wregex counts UTF-16 code units like Java)
	EXPECT_TRUE(matches(main::LegionConfig::LEGION_NAME_PATTERN, L"My Legion"));
	EXPECT_FALSE(matches(main::LegionConfig::LEGION_NAME_PATTERN, L"x"));
	EXPECT_TRUE(matches(main::LegionConfig::SELF_INTRO_PATTERN, std::wstring(32, L'Ж')));
	EXPECT_FALSE(matches(main::LegionConfig::SELF_INTRO_PATTERN, std::wstring(33, L'Ж')));
	EXPECT_TRUE(matches(main::LegionConfig::NICKNAME_PATTERN, L"Nick"));
	EXPECT_TRUE(matches(main::NameConfig::CHAR_NAME_PATTERN, L"Neon"));
	EXPECT_FALSE(matches(main::NameConfig::CHAR_NAME_PATTERN, L"Neon1"));
	EXPECT_TRUE(matches(main::NameConfig::PET_NAME_PATTERN, L"Fluffy"));
	EXPECT_FALSE(main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.get()->has_value()); // empty value: null
	const auto forbiddenWords = main::NameConfig::FORBIDDEN_WORDS.get();
	EXPECT_GT(forbiddenWords->size(), 700u);
	EXPECT_EQ(forbiddenWords->front(), "analcavity");
	EXPECT_EQ(std::ranges::count(*forbiddenWords, ""), 1); // "asskicker,,assLessOne"
}

TEST_F(ConfigLoadTest, PropertiesMapsOfShippedConfig) {
	load();

	const auto accessLevels = administration::CommandsConfig::ACCESS_LEVELS.get();
	EXPECT_EQ(accessLevels->size(), 152u); // every dotless key of commands.properties
	EXPECT_EQ(accessLevels->at("configure"), 9);
	EXPECT_EQ(accessLevels->at("Bookmark_add"), 9);
	EXPECT_EQ(accessLevels->at("faction"), 10);
	EXPECT_EQ(accessLevels->at("advent"), 0);
	EXPECT_EQ(accessLevels->at("invisible"), 2);
	EXPECT_EQ(accessLevels->find(std::string_view("dispel"))->second, 5); // transparent lookup
	EXPECT_FALSE(accessLevels->contains("gameserver.commands.handler_directories"));

	const auto quota = main::RankingConfig::TOP_RANKING_QUOTA.get();
	using detail::AbyssRankEnum;
	EXPECT_EQ(*quota, (std::map<AbyssRankEnum, int32_t>{{AbyssRankEnum::STAR1_OFFICER, 1000},
	                                                    {AbyssRankEnum::STAR2_OFFICER, 700},
	                                                    {AbyssRankEnum::STAR3_OFFICER, 500},
	                                                    {AbyssRankEnum::STAR4_OFFICER, 300},
	                                                    {AbyssRankEnum::STAR5_OFFICER, 100},
	                                                    {AbyssRankEnum::GENERAL, 30},
	                                                    {AbyssRankEnum::GREAT_GENERAL, 10},
	                                                    {AbyssRankEnum::COMMANDER, 3},
	                                                    {AbyssRankEnum::SUPREME_COMMANDER, 1}}));
	const auto gpLoss = main::RankingConfig::TOP_RANKING_GP_LOSS.get();
	EXPECT_EQ(gpLoss->size(), 9u);
	EXPECT_EQ(gpLoss->at(AbyssRankEnum::STAR1_OFFICER), 9);
	EXPECT_EQ(gpLoss->at(AbyssRankEnum::SUPREME_COMMANDER), 219);
	EXPECT_FALSE(gpLoss->contains(AbyssRankEnum::GRADE1_SOLDIER));

	const auto autoFill = main::HousingConfig::AUCTION_AUTO_FILL_LIMITS.get();
	using detail::HouseType;
	// iterated in ordinal order like Java's EnumMap
	using Entries = std::vector<std::pair<HouseType, int32_t>>;
	const Entries expected{{HouseType::ESTATE, 5}, {HouseType::MANSION, 10}, {HouseType::HOUSE, 20}, {HouseType::PALACE, 1}};
	EXPECT_EQ(Entries(autoFill->begin(), autoFill->end()), expected);

	EXPECT_TRUE(network::PffConfig::THRESHOLD_MILLIS_BY_PACKET_OPCODE.get()->empty());
	EXPECT_EQ(network::PffConfig::PFF_MODE.load(), 1);
}

TEST_F(ConfigLoadTest, MygsPropertiesOverrideDefaults) {
	writeMygs("gameserver.players.max.level = 66\n"
	          "gameserver.network.client.connect_address = 127.0.0.1:7778\n"
	          "gameserver.network.pff.packet.0x1A = 300\n"
	          "gameserver.network.pff.packet.0X0f = 50\n"
	          "gameserver.network.pff.packet.26 = 1\n" // not a hex opcode: unknown property
	          "gameserver.name.forbidden_sequences_pattern = (.)\\\\1\\\\1\n"
	          "gameserver.shutdown.restart_schedule = 0 0 5 ? * *\n"
	          "gameserver.drop.announce_quality =\n"
	          "gameserver.dredgion.time = \"0 0 1,2 ? * *\", 0 0 3 ? * *\n"
	          "gameserver.timezone = UTC\n");
	std::string log = load();

	EXPECT_EQ(log.find("No override properties found"), std::string::npos) << log;
	EXPECT_EQ(log.find("No IP for Aion client advertisement"), std::string::npos) << log;
	EXPECT_NE(log.find("warning|Config property gameserver.network.pff.packet.26 is unknown and therefore ignored.\n"), std::string::npos) << log;
	EXPECT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 66);
	EXPECT_EQ(*network::NetworkConfig::CLIENT_CONNECT_ADDRESS.get(), (InetSocketAddress{"127.0.0.1", 7778}));
	EXPECT_EQ(*network::PffConfig::THRESHOLD_MILLIS_BY_PACKET_OPCODE.get(), (std::unordered_map<int32_t, int32_t>{{0x1A, 300}, {0x0F, 50}}));
	EXPECT_EQ(network::PffConfig::getAllowedMillisBetweenPackets(0x1A), 300);
	EXPECT_EQ(network::PffConfig::getAllowedMillisBetweenPackets(0x1B), 0);
	struct FakePacket {
		int32_t getOpCode() const { return 0x0F; }
	};
	EXPECT_EQ(network::PffConfig::getAllowedMillisBetweenPackets(FakePacket{}), 50);
	const auto forbiddenSequence = main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.get();
	ASSERT_TRUE(forbiddenSequence->has_value());
	EXPECT_TRUE(std::regex_search(std::wstring(L"Naaame"), **forbiddenSequence));
	EXPECT_FALSE(std::regex_search(std::wstring(L"Naame"), **forbiddenSequence));
	EXPECT_EQ(cronText(main::ShutdownConfig::RESTART_SCHEDULE.load()), "0 0 5 ? * *");
	EXPECT_EQ(main::DropConfig::MIN_ANNOUNCE_QUALITY.load(), std::nullopt);
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::DREDGION_TIMES.get()), (std::vector<std::string>{"0 0 1,2 ? * *", "0 0 3 ? * *"}));
	EXPECT_EQ(main::GSConfig::TIME_ZONE_ID.load(), std::chrono::locate_zone("UTC"));
	EXPECT_EQ(DatabaseConfig::DATABASE_URL, "jdbc:mysql://localhost:3306/aion_gs?serverTimezone=UTC&characterEncoding=UTF-8");
}

TEST_F(ConfigLoadTest, UnknownPropertiesAreWarnedExceptLoggingKeys) {
	writeMygs("gameserver.no.such.key = 1\n"
	          "gameserver.log.status.discord.webhook_url = https://example.invalid/hook\n");
	std::string log = load();

	EXPECT_NE(log.find("warning|Config property gameserver.no.such.key is unknown and therefore ignored.\n"), std::string::npos) << log;
	EXPECT_EQ(log.find("discord"), std::string::npos) << log;
}

TEST_F(ConfigLoadTest, EventPropertiesOverlayAndRebindAtRuntime) {
	writeMygs("gameserver.players.max.level = 66\n");
	load();
	ASSERT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 66);
	const auto wordsBeforeEvent = main::NameConfig::FORBIDDEN_WORDS.get();
	const auto soloRatesBeforeEvent = main::RatesConfig::XP_SOLO_RATES.get();
	const auto dredgionTimesBeforeEvent = main::AutoGroupConfig::DREDGION_TIMES.get();
	ASSERT_GT(wordsBeforeEvent->size(), 700u);

	// an event starts (Java: Event.start -> Config.load() with the event's config properties)
	Config::setEventConfigPropertiesProvider([] {
		Properties eventProperties;
		eventProperties.setProperty("gameserver.players.max.level", "70"); // wins over mygs.properties
		eventProperties.setProperty("gameserver.rates.xp.solo", "5.0, 6.0");
		eventProperties.setProperty("gameserver.name.forbidden_words", "foo, bar");
		eventProperties.setProperty("gameserver.dredgion.time", "0 0 8 ? * *");
		eventProperties.setProperty("gameserver.housing.auction.auto_fill.limit.STUDIO", "7");
		return eventProperties;
	});
	load();

	EXPECT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 70);
	EXPECT_EQ(*main::RatesConfig::XP_SOLO_RATES.get(), (std::vector<float>{5.0f, 6.0f}));
	EXPECT_EQ(*main::NameConfig::FORBIDDEN_WORDS.get(), (std::vector<std::string>{"foo", "bar"}));
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::DREDGION_TIMES.get()), (std::vector<std::string>{"0 0 8 ? * *"}));
	EXPECT_EQ(main::HousingConfig::AUCTION_AUTO_FILL_LIMITS.get()->at(detail::HouseType::STUDIO), 7);
	// snapshots taken before the reload are unaffected and stay valid
	EXPECT_GT(wordsBeforeEvent->size(), 700u);
	EXPECT_EQ(*soloRatesBeforeEvent, (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(cronTexts(*dredgionTimesBeforeEvent), (std::vector<std::string>{"0 0 0,12,20 ? * *"}));

	// the event ends (Java: Event.stop -> Config.load() without its properties)
	Config::setEventConfigPropertiesProvider(nullptr);
	load();

	EXPECT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 66);
	EXPECT_EQ(*main::RatesConfig::XP_SOLO_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(main::NameConfig::FORBIDDEN_WORDS.get()->size(), wordsBeforeEvent->size());
	EXPECT_FALSE(main::HousingConfig::AUCTION_AUTO_FILL_LIMITS.get()->contains(detail::HouseType::STUDIO));
}

TEST_F(ConfigLoadTest, LoadsOnlyAllowedConfigs) {
	load();
	main::GSConfig::PLAYER_MAX_LEVEL = 1;
	administration::CommandsConfig::ACCESS_LEVELS.set({});
	writeMygs("configure = 3\n");

	// Java: ChatProcessor.reload() -> Config.load(CommandsConfig.class)
	std::string log = load({&administration::CommandsConfig::bind});

	EXPECT_EQ(administration::CommandsConfig::ACCESS_LEVELS.get()->at("configure"), 3);
	EXPECT_EQ(administration::CommandsConfig::ACCESS_LEVELS.get()->size(), 152u);
	EXPECT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 1);       // not reloaded
	EXPECT_EQ(log.find("is unknown"), std::string::npos) << log; // no warnings for a partial load

	ScopedCurrentPath cwd(dir.path);
	EXPECT_THROW(Config::load({&notAConfig}), aion::commons::utils::IllegalArgumentException);
	EXPECT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 1);
}

TEST_F(ConfigLoadTest, FailsWithoutLocalIpForWildcardConnectAddress) {
	Config::setLocalIPv4Finder([] { return std::optional<std::string>(); });
	try {
		load();
		FAIL() << "expected an exception";
	} catch (const aion::commons::utils::Exception& e) {
		EXPECT_EQ(std::string(e.what()), "No IP for Aion client advertisement configured and local IP discovery failed. Please configure "
		                                 "gameserver.network.client.connect_address");
	}
}

TEST_F(ConfigLoadTest, InvalidValueFailsWithTransformationException) {
	writeMygs("gameserver.players.max.level = sixty\n");
	EXPECT_THROW(load(), aion::commons::configuration::TransformationException);
}

TEST_F(ConfigLoadTest, FailsWithoutConfigDirectory) {
	TempDirectory empty;
	ScopedCurrentPath cwd(empty.path);
	try {
		Config::load();
		FAIL() << "expected an exception";
	} catch (const aion::commons::utils::Exception& e) {
		EXPECT_EQ(std::string(e.what()), "Can't load gameserver configuration:");
	}
}

TEST_F(ConfigLoadTest, ReadersStayValidDuringConcurrentReloads) {
	load();
	std::atomic<bool> stop{false};
	std::atomic<int64_t> reads{0};
	std::vector<std::jthread> readers;
	for (int i = 0; i < 4; ++i) {
		readers.emplace_back([&] {
			while (!stop.load()) {
				auto words = main::NameConfig::FORBIDDEN_WORDS.get();
				size_t length = 0;
				for (const std::string& word : *words)
					length += word.size();
				auto rates = main::RatesConfig::DROP_RATES.get();
				auto accessLevels = administration::CommandsConfig::ACCESS_LEVELS.get();
				auto times = main::AutoGroupConfig::KAMAR_BATTLEFIELD_TIMES.get();
				if (length == 0 || rates->size() != 2 || accessLevels->empty() || times->size() != 1 || times->front() == nullptr ||
				    main::GSConfig::PLAYER_MAX_LEVEL.load() < 65)
					ADD_FAILURE() << "inconsistent snapshot";
				reads.fetch_add(1);
			}
		});
	}
	// full and partial reloads with event properties, two threads at once
	Config::setEventConfigPropertiesProvider([] {
		Properties eventProperties;
		eventProperties.setProperty("gameserver.rates.drop", "3.0, 4.0");
		eventProperties.setProperty("gameserver.players.max.level", "80");
		return eventProperties;
	});
	{
		ScopedCurrentPath cwd(dir.path); // the working directory is process-wide: set once for both reload threads
		std::jthread first([&] {
			for (int i = 0; i < 5; ++i)
				Config::load();
		});
		std::jthread second([&] {
			for (int i = 0; i < 5; ++i)
				Config::load({&administration::CommandsConfig::bind, &main::RatesConfig::bind, &main::NameConfig::bind});
		});
	}
	stop = true;
	readers.clear();
	EXPECT_GT(reads.load(), 0);
	EXPECT_EQ(*main::RatesConfig::DROP_RATES.get(), (std::vector<float>{3.0f, 4.0f}));
}

TEST_F(ConfigLoadTest, LoggingConfigFromGameserverLoggingAndMygsProperties) {
	writeMygs("gameserver.timezone = Europe/Berlin\n"
	          "gameserver.log.status.discord.webhook_url =  https://example.invalid/hook \n");
	ScopedCurrentPath cwd(dir.path);
	aion::commons::logging::Logging::Config config = Config::loadLoggingConfig();

	EXPECT_EQ(config.timeZone, std::chrono::locate_zone("Europe/Berlin"));
	EXPECT_EQ(config.statusDiscordWebhookUrl, "https://example.invalid/hook");
	EXPECT_EQ(config.statusDiscordAvatarUrl, "");

	fs::remove(dir.path / "config" / "mygs.properties");
	config = Config::loadLoggingConfig();
	EXPECT_EQ(config.timeZone, nullptr); // shipped gameserver.timezone is empty: system default
	EXPECT_EQ(config.statusDiscordWebhookUrl, "");
}

TEST(ConfigClassesTest, AllConfigClasses) {
	std::span<const Config::ConfigClass> classes = Config::getClasses();
	ASSERT_EQ(classes.size(), 36u); // 33 game server classes, CommonsConfig, DatabaseConfig and RuntimeConfig
	std::set<std::string_view> names;
	std::set<Config::BindFunction> binders;
	for (const Config::ConfigClass& config : classes) {
		names.insert(config.simpleName);
		binders.insert(config.bind);
		EXPECT_TRUE(config.name.ends_with(config.simpleName)) << config.name;
	}
	EXPECT_EQ(names.size(), classes.size());
	EXPECT_EQ(binders.size(), classes.size());
	EXPECT_EQ(classes[0].name, "com.aionemu.gameserver.configs.administration.AdminConfig");
	EXPECT_EQ(classes[11].name, "com.aionemu.gameserver.configs.main.GSConfig");
	EXPECT_EQ(classes[34].name, "com.aionemu.gameserver.configs.network.PffConfig");
}
