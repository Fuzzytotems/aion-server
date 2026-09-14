#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/CleaningConfig.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/FallDamageConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/configs/main/ShutdownConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/main/ThreadConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/configs/network/PffConfig.h"
#include "aion/gameserver/services/cron/CronService.h"

#include "ConfigTestSupport.h"

/**
 * The default value of every config field: all config classes are bound against empty properties, so each field with a Java defaultValue gets
 * it, and each field without one keeps the sentinel set before.
 * <p>
 * The expectations were generated once from the Java @Property annotations (values written out by type: Java decode for numbers, comma
 * separated lists split like CommaSeparatedValueTransformer) and are maintained by hand since. JavaAnnotationsTest checks independently that
 * every Java key and default parses and is bound.
 */

using namespace aion::gameserver::configs;
using namespace aion::gameserver::configs::test;
using aion::commons::configuration::ConfigurableProcessor;
using aion::commons::configuration::Properties;
using aion::commons::utils::InetSocketAddress;
namespace fs = std::filesystem;

namespace {

class ConfigDefaultsTest : public ::testing::Test {
protected:
	void SetUp() override {
		// fields without a default value keep these
		main::DropConfig::MIN_ANNOUNCE_QUALITY = detail::ItemQuality::RARE;
		main::DropConfig::DISABLE_RANGE_CHECK_MAPS.set({42});
		main::EventsConfig::DISABLED_EVENTS.set({"sentinel"});
		sentinelZone = std::chrono::locate_zone("UTC");
		main::GSConfig::TIME_ZONE_ID = sentinelZone;
		main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(std::wregex(L"x"));
		main::NameConfig::FORBIDDEN_WORDS.set({"sentinel"});
		main::ShutdownConfig::RESTART_SCHEDULE = &aion::gameserver::services::cron::CronExpressions::getOrCreate("0 0 1 ? * *");

		Properties empty;
		std::vector<ConfigurableProcessor::Binder> binders;
		for (const Config::ConfigClass& config : Config::getClasses())
			binders.emplace_back(config.bind);
		std::set<std::string> unused = ConfigurableProcessor::process(empty, binders);
		ASSERT_TRUE(unused.empty());
	}

	const std::chrono::time_zone* sentinelZone = nullptr;
};

} // namespace

TEST_F(ConfigDefaultsTest, AdminConfig) {
	EXPECT_EQ(
	  *administration::AdminConfig::NAME_TAGS.get(),
	  (std::vector<std::string>{"%s", "\u00BBJDev\u00AB\uE04A%s", "\u00BBDev\u00AB\uE04A%s", "\u00BBJEM\u00AB\uE04A%s", "\u00BBEM\u00AB\uE04A%s",
	                            "\u00BBJGM\u00AB\uE04A%s", "\u00BBGM\u00AB\uE04A%s", "\u00BBSGM\u00AB\uE04A%s", "\u00BBAdmin\u00AB\uE04A%s"}));
	EXPECT_EQ(administration::AdminConfig::UNRESTRICTED_ITEMTRADE.load(), 1);
	EXPECT_EQ(administration::AdminConfig::GM_PANEL.load(), 2);
	EXPECT_EQ(administration::AdminConfig::GM_SKILLS.load(), 8);
	EXPECT_EQ(administration::AdminConfig::FREE_FLIGHT.load(), 1);
	EXPECT_EQ(administration::AdminConfig::UNLIMITED_FLIGHT_TIME.load(), 1);
	EXPECT_EQ(administration::AdminConfig::AUTO_RES.load(), 1);
	EXPECT_EQ(administration::AdminConfig::VIEW_PLAYER_DETAILS.load(), 5);
	EXPECT_EQ(administration::AdminConfig::INSTANCE_ENTER_ALL.load(), 2);
	EXPECT_EQ(administration::AdminConfig::INSTANCE_OPEN_DOORS.load(), 6);
	EXPECT_EQ(administration::AdminConfig::INSTANCE_DOOR_INFO.load(), 9);
	EXPECT_EQ(administration::AdminConfig::HOUSE_ENTER_ALL.load(), 9);
	EXPECT_EQ(administration::AdminConfig::HOUSE_SHOW_ADDRESS.load(), 9);
	EXPECT_EQ(administration::AdminConfig::DIALOG_INFO.load(), 9);
	EXPECT_EQ(administration::AdminConfig::ENCHANT_INFO.load(), 9);
	EXPECT_EQ(administration::AdminConfig::ZONE_INFO.load(), 9);
	EXPECT_EQ(administration::AdminConfig::AUDIT_INFO.load(), 9);
	EXPECT_EQ(administration::AdminConfig::CMD_QUEST_ADV_PARAMS.load(), 9);
	EXPECT_EQ(*administration::AdminConfig::LOGIN_EXECUTE_COMMANDS.get(), (std::vector<std::string>{"//invis", "//invul", "//enemy none", "//see"}));
	EXPECT_EQ(administration::AdminConfig::REVISION_INFO_ON_LOGIN.load(), 9);
	EXPECT_EQ(*administration::AdminConfig::ANNOUNCE_LEVELS.get(), (std::vector<std::string>{"*"}));
	EXPECT_TRUE(administration::AdminConfig::ANNOUNCE_LOGIN_TO_ALL_PLAYERS.load());
	EXPECT_TRUE(administration::AdminConfig::ANNOUNCE_LOGOUT_TO_ALL_PLAYERS.load());
}

TEST_F(ConfigDefaultsTest, CommandsConfig) {
	EXPECT_TRUE(administration::CommandsConfig::ACCESS_LEVELS.get()->empty());
	EXPECT_EQ(*administration::CommandsConfig::HANDLER_DIRECTORIES.get(),
	          (std::vector<fs::path>{"./data/handlers/admincommands", "./data/handlers/playercommands", "./data/handlers/consolecommands"}));
}

TEST_F(ConfigDefaultsTest, AIConfig) {
	EXPECT_TRUE(main::AIConfig::MOVE_DEBUG.load());
	EXPECT_FALSE(main::AIConfig::EVENT_DEBUG.load());
	EXPECT_FALSE(main::AIConfig::ONCREATE_DEBUG.load());
	EXPECT_TRUE(main::AIConfig::ACTIVE_NPC_MOVEMENT.load());
	EXPECT_EQ(main::AIConfig::MINIMIMUM_DELAY.load(), 3);
	EXPECT_EQ(main::AIConfig::MAXIMUM_DELAY.load(), 15);
	EXPECT_FALSE(main::AIConfig::SHOUTS_ENABLE.load());
	EXPECT_EQ(*main::AIConfig::HANDLER_DIRECTORY.get(), fs::path("./data/handlers/ai"));
}

TEST_F(ConfigDefaultsTest, AutoGroupConfig) {
	EXPECT_TRUE(main::AutoGroupConfig::AUTO_GROUP_ENABLE.load());
	EXPECT_TRUE(main::AutoGroupConfig::START_TIME_ENABLE.load());
	EXPECT_EQ(main::AutoGroupConfig::DREDGION_REGISTRATION_PERIOD.load(), 60);
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::DREDGION_TIMES.get()), (std::vector<std::string>{"0 0 0,12,20 ? * *"}));
	EXPECT_EQ(main::AutoGroupConfig::KAMAR_BATTLEFIELD_REGISTRATION_PERIOD.load(), 60);
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::KAMAR_BATTLEFIELD_TIMES.get()), (std::vector<std::string>{"0 0 0,20 ? * MON,WED,SAT"}));
	EXPECT_EQ(main::AutoGroupConfig::ENGULFED_OPHIDAN_BRIDGE_REGISTRATION_PERIOD.load(), 60);
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::ENGULFED_OPHIDAN_BRIDGE_TIMES.get()), (std::vector<std::string>{"0 0 12,19 ? * *"}));
	EXPECT_EQ(main::AutoGroupConfig::IRON_WALL_WARFRONT_REGISTRATION_PERIOD.load(), 60);
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::IRON_WALL_WARFRONT_TIMES.get()), (std::vector<std::string>{"0 0 0,12 ? * SUN"}));
	EXPECT_EQ(main::AutoGroupConfig::IDGEL_DOME_REGISTRATION_PERIOD.load(), 60);
	EXPECT_EQ(cronTexts(*main::AutoGroupConfig::IDGEL_DOME_TIMES.get()), (std::vector<std::string>{"0 0 23 ? * *"}));
	EXPECT_FALSE(main::AutoGroupConfig::ANNOUNCE_BATTLEGROUND_REGISTRATIONS.load());
}

TEST_F(ConfigDefaultsTest, CleaningConfig) {
	EXPECT_FALSE(main::CleaningConfig::CLEANING_ENABLE.load());
	EXPECT_EQ(main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS.load(), 365);
	EXPECT_EQ(main::CleaningConfig::MAX_DELETABLE_CHAR_LEVEL.load(), 25);
}

TEST_F(ConfigDefaultsTest, CraftConfig) {
	EXPECT_FALSE(main::CraftConfig::DELETE_EXCESS_CRAFT_ENABLE.load());
	EXPECT_EQ(main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.load(), 2);
	EXPECT_EQ(main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.load(), 1);
	EXPECT_FALSE(main::CraftConfig::DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP.load());
	EXPECT_EQ(main::CraftConfig::MAX_CRAFT_FAILURE_CHANCE.load(), 33);
	EXPECT_EQ(main::CraftConfig::MAX_GATHER_FAILURE_CHANCE.load(), 33);
}

TEST_F(ConfigDefaultsTest, CustomConfig) {
	EXPECT_FALSE(main::CustomConfig::CHALLENGE_TASKS_ENABLED.load());
	EXPECT_TRUE(main::CustomConfig::ENABLE_ENCHANT_ANNOUNCE.load());
	EXPECT_FALSE(main::CustomConfig::SPEAKING_BETWEEN_FACTIONS.load());
	EXPECT_EQ(main::CustomConfig::LEVEL_TO_WHISPER.load(), 10);
	EXPECT_EQ(main::CustomConfig::BROKER_REGISTRATION_EXPIRATION_DAYS.load(), 8);
	EXPECT_FALSE(main::CustomConfig::FACTIONS_SEARCH_MODE.load());
	EXPECT_FALSE(main::CustomConfig::SEARCH_GM_LIST.load());
	EXPECT_EQ(main::CustomConfig::LEVEL_TO_SEARCH.load(), 10);
	EXPECT_FALSE(main::CustomConfig::ENABLE_CROSS_FACTION_BINDING.load());
	EXPECT_FALSE(main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.load());
	EXPECT_FALSE(main::CustomConfig::SKILL_CHAIN_DISABLE_TRIGGERRATE.load());
	EXPECT_EQ(main::CustomConfig::BASE_FLYTIME.load(), 60);
	EXPECT_FALSE(main::CustomConfig::FRIENDLIST_GM_RESTRICT.load());
	EXPECT_EQ(main::CustomConfig::FRIENDLIST_SIZE.load(), 90);
	EXPECT_EQ(main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.load(), 40);
	EXPECT_EQ(main::CustomConfig::CUBE_EXPANSION_LIMIT.load(), 11);
	EXPECT_EQ(main::CustomConfig::NPC_CUBE_EXPANDS_SIZE_LIMIT.load(), 5);
	EXPECT_FALSE(main::CustomConfig::ENABLE_KINAH_CAP.load());
	EXPECT_EQ(main::CustomConfig::KINAH_CAP_VALUE.load(), 999999999);
	EXPECT_FALSE(main::CustomConfig::ENABLE_AP_CAP.load());
	EXPECT_EQ(main::CustomConfig::AP_CAP_VALUE.load(), 1000000);
	EXPECT_FALSE(main::CustomConfig::MENTOR_GROUP_AP.load());
	EXPECT_EQ(main::CustomConfig::FACTION_USE_PRICE.load(), 10000);
	EXPECT_TRUE(main::CustomConfig::FACTION_CMD_CHANNEL.load());
	EXPECT_FALSE(main::CustomConfig::FACTION_CHAT_CHANNEL.load());
	EXPECT_EQ(main::CustomConfig::PVP_DAY_DURATION.load(), 86400000);
	EXPECT_EQ(main::CustomConfig::MAX_DAILY_PVP_KILLS.load(), 5);
	EXPECT_FALSE(main::CustomConfig::ENABLE_KILL_REWARD.load());
	EXPECT_FALSE(main::CustomConfig::KEEP_BUFFS_IN_COLISEUM.load());
	EXPECT_TRUE(main::CustomConfig::ENABLE_KISK_RESTRICTION.load());
	EXPECT_TRUE(main::CustomConfig::RIFT_ENABLED.load());
	EXPECT_EQ(main::CustomConfig::RIFT_DURATION.load(), 1);
	EXPECT_TRUE(main::CustomConfig::VORTEX_ENABLED.load());
	EXPECT_EQ(cronText(main::CustomConfig::VORTEX_BRUSTHONIN_SCHEDULE.load()), "0 0 16 ? * SAT");
	EXPECT_EQ(cronText(main::CustomConfig::VORTEX_THEOBOMOS_SCHEDULE.load()), "0 0 16 ? * SUN");
	EXPECT_EQ(main::CustomConfig::VORTEX_DURATION.load(), 1);
	EXPECT_TRUE(main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load());
	EXPECT_EQ(*main::CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS.get(),
	          (std::unordered_set<int32_t>{210020000, 210040000, 210050000, 210070000, 220020000, 220040000, 220070000, 220080000}));
	EXPECT_EQ(main::CustomConfig::CONQUEROR_AND_PROTECTOR_LEVEL_DIFF.load(), 5);
	EXPECT_EQ(main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL.load(), 10);
	EXPECT_EQ(main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_COUNT.load(), 1);
	EXPECT_EQ(main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK1.load(), 1);
	EXPECT_EQ(main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK2.load(), 10);
	EXPECT_EQ(main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK3.load(), 20);
	EXPECT_TRUE(main::CustomConfig::LIMITS_ENABLED.load());
	EXPECT_FALSE(main::CustomConfig::LIMITS_ENABLE_DYNAMIC_CAP.load());
	EXPECT_EQ(cronText(main::CustomConfig::LIMITS_UPDATE.load()), "0 0 0 ? * *");
	EXPECT_FALSE(main::CustomConfig::ABYSSXFORM_LOGOUT.load());
	EXPECT_TRUE(main::CustomConfig::ENABLE_RIDE_RESTRICTION.load());
	EXPECT_TRUE(main::CustomConfig::SELLING_APITEMS_ENABLED.load());
	EXPECT_EQ(main::CustomConfig::CHARACTER_DELETION_TIME_MINUTES.load(), 5);
	EXPECT_FALSE(main::CustomConfig::IGNORE_POTIONS_AT_FULL_HEALTH.load());
	EXPECT_TRUE(main::CustomConfig::CANCEL_ITEM_USE_ON_TARGET_CHANGE.load());
	EXPECT_FALSE(main::CustomConfig::ENABLE_STARTER_KIT.load());
	EXPECT_FALSE(main::CustomConfig::PVP_MAP_ENABLED.load());
	EXPECT_FLOAT_EQ(main::CustomConfig::PVP_MAP_AP_MULTIPLIER.load(), 2.0f);
	EXPECT_FLOAT_EQ(main::CustomConfig::PVP_MAP_PVE_AP_MULTIPLIER.load(), 1.0f);
	EXPECT_EQ(main::CustomConfig::PVP_MAP_RANDOM_BOSS_BASE_RATE.load(), 40);
	EXPECT_EQ(cronText(main::CustomConfig::PVP_MAP_RANDOM_BOSS_SCHEDULE.load()), "0 30 14,18,21 ? * *");
	EXPECT_FLOAT_EQ(main::CustomConfig::GODSTONE_ACTIVATION_RATE.load(), 1.0f);
	EXPECT_EQ(main::CustomConfig::GODSTONE_EVALUATION_COOLDOWN_MILLIS.load(), 750);
	EXPECT_FALSE(main::CustomConfig::COUNT_SUMMON_EFFECTS_FOR_CUMULATIVE_RESIST.load());
}

TEST_F(ConfigDefaultsTest, DropConfig) {
	EXPECT_EQ(main::DropConfig::MIN_ANNOUNCE_QUALITY.load(), detail::ItemQuality::RARE);             // no default: keeps its value
	EXPECT_EQ(*main::DropConfig::DISABLE_RANGE_CHECK_MAPS.get(), (std::unordered_set<int32_t>{42})); // no default: keeps its value
}

TEST_F(ConfigDefaultsTest, EventsConfig) {
	EXPECT_EQ(*main::EventsConfig::DISABLED_EVENTS.get(), (std::unordered_set<std::string>{"sentinel"})); // no default: keeps its value
	EXPECT_FALSE(main::EventsConfig::ENABLE_EVENT_ARCADE.load());
	EXPECT_EQ(main::EventsConfig::ARCADE_RESUME_TOKEN.load(), 3);
	EXPECT_TRUE(main::EventsConfig::ENABLE_WORLDRAID.load());
	EXPECT_TRUE(main::EventsConfig::WORLDRAID_ENABLE_SPAWNMSG.load());
	EXPECT_FALSE(main::EventsConfig::ENABLE_HEADHUNTING.load());
	EXPECT_EQ(*main::EventsConfig::HEADHUNTING_MAPS.get(), (std::unordered_set<int32_t>{}));
	EXPECT_EQ(main::EventsConfig::HEADHUNTING_CONSOLATION_PRIZE_KILLS.load(), 50);
	EXPECT_FALSE(main::EventsConfig::ENABLE_ADVENT_CALENDAR.load());
}

TEST_F(ConfigDefaultsTest, FallDamageConfig) {
	EXPECT_FLOAT_EQ(main::FallDamageConfig::FALL_DAMAGE_PERCENTAGE.load(), 1.0f);
	EXPECT_EQ(main::FallDamageConfig::MINIMUM_DISTANCE_DAMAGE.load(), 10);
	EXPECT_EQ(main::FallDamageConfig::MAXIMUM_DISTANCE_DAMAGE.load(), 50);
	EXPECT_EQ(main::FallDamageConfig::MAXIMUM_DISTANCE_MIDAIR.load(), 200);
}

TEST_F(ConfigDefaultsTest, GSConfig) {
	EXPECT_EQ(main::GSConfig::SERVER_COUNTRY_CODE.load(), 99);
	EXPECT_EQ(main::GSConfig::PLAYER_MAX_LEVEL.load(), 65);
	EXPECT_EQ(main::GSConfig::TIME_ZONE_ID.load(), sentinelZone); // no default: keeps its value
	EXPECT_FALSE(main::GSConfig::ENABLE_CHAT_SERVER.load());
	EXPECT_EQ(main::GSConfig::CHAT_SERVER_MIN_LEVEL.load(), 10);
	EXPECT_EQ(main::GSConfig::CHARACTER_CREATION_MODE.load(), 0);
	EXPECT_EQ(main::GSConfig::CHARACTER_LIMIT_COUNT.load(), 8);
	EXPECT_EQ(main::GSConfig::CHARACTER_FACTION_LIMITATION_MODE.load(), 0);
	EXPECT_FALSE(main::GSConfig::ENABLE_RATIO_LIMITATION.load());
	EXPECT_EQ(main::GSConfig::RATIO_MIN_VALUE.load(), 60);
	EXPECT_EQ(main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.load(), 10);
	EXPECT_EQ(main::GSConfig::RATIO_MIN_CHARACTERS_COUNT.load(), 50);
	EXPECT_EQ(main::GSConfig::RATIO_HIGH_PLAYER_COUNT_DISABLING.load(), 500);
	EXPECT_EQ(main::GSConfig::CHARACTER_REENTRY_TIME.load(), 20);
	EXPECT_EQ(main::GSConfig::MIN_SKILL_CAST_INTERVAL_MILLIS.load(), 350);
	EXPECT_EQ(main::GSConfig::ITEM_WRAP_LIMIT.load(), 0);
	EXPECT_FALSE(main::GSConfig::ENABLE_WEB_REWARDS.load());
	EXPECT_TRUE(main::GSConfig::ANALYZE_QUESTHANDLERS.load());
	EXPECT_EQ(*main::GSConfig::QUEST_HANDLER_DIRECTORY.get(), fs::path("./data/handlers/quest"));
}

TEST_F(ConfigDefaultsTest, GeoDataConfig) {
	EXPECT_TRUE(main::GeoDataConfig::GEO_ENABLE.load());
	EXPECT_TRUE(main::GeoDataConfig::CANSEE_ENABLE.load());
	EXPECT_TRUE(main::GeoDataConfig::FEAR_ENABLE.load());
	EXPECT_TRUE(main::GeoDataConfig::GEO_NPC_MOVE.load());
	EXPECT_TRUE(main::GeoDataConfig::GEO_MATERIALS_ENABLE.load());
	EXPECT_FALSE(main::GeoDataConfig::GEO_MATERIALS_SHOWDETAILS.load());
	EXPECT_TRUE(main::GeoDataConfig::GEO_SHIELDS_ENABLE.load());
}

TEST_F(ConfigDefaultsTest, GroupConfig) {
	EXPECT_EQ(main::GroupConfig::GROUP_REMOVE_TIME.load(), 600);
	EXPECT_EQ(main::GroupConfig::GROUP_MAX_DISTANCE.load(), 100);
	EXPECT_FALSE(main::GroupConfig::GROUP_INVITEOTHERFACTION.load());
	EXPECT_EQ(main::GroupConfig::ALLIANCE_REMOVE_TIME.load(), 600);
	EXPECT_FALSE(main::GroupConfig::ALLIANCE_INVITEOTHERFACTION.load());
	EXPECT_FALSE(main::GroupConfig::FORM_INSTANCE_GROUP_ANYWHERE.load());
}

TEST_F(ConfigDefaultsTest, HousingConfig) {
	EXPECT_FLOAT_EQ(main::HousingConfig::VISIBILITY_DISTANCE.load(), 200.0f);
	EXPECT_TRUE(main::HousingConfig::ENABLE_HOUSE_AUCTIONS.load());
	EXPECT_TRUE(main::HousingConfig::ENABLE_HOUSE_PAY.load());
	EXPECT_EQ(cronText(main::HousingConfig::HOUSE_AUCTION_END_TIME.load()), "0 0 12 ? * SUN");
	EXPECT_EQ(*main::HousingConfig::HOUSE_AUCTION_REGISTER_DAYS.get(), (std::vector<int32_t>{1, 5}));
	EXPECT_EQ(cronText(main::HousingConfig::HOUSE_MAINTENANCE_TIME.load()), "0 0 0 ? * MON");
	EXPECT_EQ(main::HousingConfig::HOUSE_MIN_BID.load(), 0);
	EXPECT_EQ(main::HousingConfig::MANSION_MIN_BID.load(), 0);
	EXPECT_EQ(main::HousingConfig::ESTATE_MIN_BID.load(), 0);
	EXPECT_EQ(main::HousingConfig::PALACE_MIN_BID.load(), 0);
	EXPECT_EQ(main::HousingConfig::HOUSE_MIN_BID_LEVEL.load(), 0);
	EXPECT_EQ(main::HousingConfig::MANSION_MIN_BID_LEVEL.load(), 0);
	EXPECT_EQ(main::HousingConfig::ESTATE_MIN_BID_LEVEL.load(), 0);
	EXPECT_EQ(main::HousingConfig::PALACE_MIN_BID_LEVEL.load(), 0);
	EXPECT_FLOAT_EQ(main::HousingConfig::AUCTION_REGISTRATION_FEE_PERCENT.load(), 0.3f);
	EXPECT_FLOAT_EQ(main::HousingConfig::AUCTION_SALES_COMMISION_PERCENT.load(), 0.1f);
	EXPECT_FLOAT_EQ(main::HousingConfig::AUCTION_GRACE_END_REFUND_PERCENT.load(), 0.5f);
	EXPECT_FLOAT_EQ(main::HousingConfig::AUCTION_BID_STEP_LIMIT.load(), 100.0f);
	EXPECT_EQ(cronText(main::HousingConfig::AUCTION_AUTO_FILL_TIME.load()), "0 0 0 ? * MON");
	EXPECT_TRUE(main::HousingConfig::AUCTION_AUTO_FILL_LIMITS.get()->empty());
}

TEST_F(ConfigDefaultsTest, HTMLConfig) {
	EXPECT_FALSE(main::HTMLConfig::ENABLE_HTML_WELCOME.load());
	EXPECT_FALSE(main::HTMLConfig::ENABLE_GUIDES.load());
	EXPECT_EQ(*main::HTMLConfig::HTML_ROOT.get(), "./data/static_data/HTML/");
	EXPECT_EQ(*main::HTMLConfig::HTML_CACHE_FILE.get(), "./cache/html.cache");
	EXPECT_EQ(*main::HTMLConfig::HTML_ENCODING.get(), "UTF-8");
}

TEST_F(ConfigDefaultsTest, InstanceConfig) {
	EXPECT_EQ(main::InstanceConfig::INSTANCE_COOLDOWN_RATE.load(), 1);
	EXPECT_EQ(*main::InstanceConfig::INSTANCE_COOLDOWN_RATE_EXCLUDED_MAPS.get(), (std::unordered_set<int32_t>{}));
	EXPECT_EQ(main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.load(), 600);
	EXPECT_EQ(main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.load(), 600);
	EXPECT_TRUE(main::InstanceConfig::INSTANCE_DUEL_ENABLE.load());
	EXPECT_FALSE(main::InstanceConfig::INSTANCE_SCALING_ENABLE.load());
	EXPECT_EQ(main::InstanceConfig::INSTANCE_SCALING_MAX_LEVEL_DIFF.load(), 5);
	EXPECT_EQ(main::InstanceConfig::INSTANCE_SCALING_NPC_MIN_RATING.load(), detail::NpcRating::ELITE);
	EXPECT_FLOAT_EQ(main::InstanceConfig::INSTANCE_SCALING_HP_SCALE_FACTOR.load(), 0.75f);
	EXPECT_FLOAT_EQ(main::InstanceConfig::INSTANCE_SCALING_HP_FLOOR.load(), 0.5f);
	EXPECT_FLOAT_EQ(main::InstanceConfig::INSTANCE_SCALING_DMG_SCALE_FACTOR.load(), 0.5f);
	EXPECT_FLOAT_EQ(main::InstanceConfig::INSTANCE_SCALING_DMG_FLOOR.load(), 0.75f);
	EXPECT_EQ(*main::InstanceConfig::INSTANCE_SCALING_EXCLUDED_MAPS.get(), (std::unordered_set<int32_t>{}));
	EXPECT_EQ(*main::InstanceConfig::HANDLER_DIRECTORY.get(), fs::path("./data/handlers/instance"));
}

TEST_F(ConfigDefaultsTest, LegionConfig) {
	// LEGION_NAME_PATTERN: std::wregex keeps no source text, see the pattern checks in ConfigLoadTest
	// SELF_INTRO_PATTERN: std::wregex keeps no source text, see the pattern checks in ConfigLoadTest
	// NICKNAME_PATTERN: std::wregex keeps no source text, see the pattern checks in ConfigLoadTest
	EXPECT_EQ(main::LegionConfig::LEGION_DISBAND_TIME.load(), 86400);
	EXPECT_EQ(main::LegionConfig::LEGION_CREATE_REQUIRED_KINAH.load(), 10000);
	EXPECT_EQ(main::LegionConfig::LEGION_EMBLEM_REQUIRED_KINAH.load(), 800000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL2_REQUIRED_KINAH.load(), 100000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL3_REQUIRED_KINAH.load(), 1000000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL4_REQUIRED_KINAH.load(), 5000000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL5_REQUIRED_KINAH.load(), 25000000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL6_REQUIRED_KINAH.load(), 50000000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL7_REQUIRED_KINAH.load(), 75000000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL8_REQUIRED_KINAH.load(), 100000000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL2_REQUIRED_MEMBERS.load(), 10);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL3_REQUIRED_MEMBERS.load(), 20);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL4_REQUIRED_MEMBERS.load(), 30);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL5_REQUIRED_MEMBERS.load(), 40);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL6_REQUIRED_MEMBERS.load(), 50);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL7_REQUIRED_MEMBERS.load(), 60);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL8_REQUIRED_MEMBERS.load(), 70);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL2_REQUIRED_CONTRIBUTION.load(), 0);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL3_REQUIRED_CONTRIBUTION.load(), 20000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL4_REQUIRED_CONTRIBUTION.load(), 100000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL5_REQUIRED_CONTRIBUTION.load(), 500000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL6_REQUIRED_CONTRIBUTION.load(), 2500000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL7_REQUIRED_CONTRIBUTION.load(), 12500000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL8_REQUIRED_CONTRIBUTION.load(), 62500000);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL1_MAX_MEMBERS.load(), 30);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL2_MAX_MEMBERS.load(), 60);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL3_MAX_MEMBERS.load(), 90);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL4_MAX_MEMBERS.load(), 120);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL5_MAX_MEMBERS.load(), 150);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL6_MAX_MEMBERS.load(), 180);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL7_MAX_MEMBERS.load(), 210);
	EXPECT_EQ(main::LegionConfig::LEGION_LEVEL8_MAX_MEMBERS.load(), 240);
	EXPECT_TRUE(main::LegionConfig::LEGION_WAREHOUSE.load());
	EXPECT_FALSE(main::LegionConfig::LEGION_INVITEOTHERFACTION.load());
	EXPECT_TRUE(main::LegionConfig::ENABLE_GUILD_TASK_REQ.load());
	EXPECT_TRUE(main::LegionConfig::REQUIRE_KEY_FOR_STONESPEAR_REACH.load());
	EXPECT_EQ(main::LegionConfig::STONESPEAR_REACH_MIN_POINTS_FOR_TERRITORY.load(), 0);
}

TEST_F(ConfigDefaultsTest, LoggingConfig) {
	EXPECT_TRUE(main::LoggingConfig::LOG_AUDIT.load());
	EXPECT_TRUE(main::LoggingConfig::LOG_CRAFT.load());
	EXPECT_TRUE(main::LoggingConfig::LOG_GMAUDIT.load());
	EXPECT_TRUE(main::LoggingConfig::LOG_GENERAL_CHATS.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_PRIVATE_CHATS.load());
	EXPECT_TRUE(main::LoggingConfig::LOG_ITEM.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_KILL.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_PL.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_MAIL.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_PLAYER_EXCHANGE.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_BROKER_EXCHANGE.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_SIEGE.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_SYSMAIL.load());
	EXPECT_TRUE(main::LoggingConfig::LOG_HOUSE_AUCTION.load());
	EXPECT_FALSE(main::LoggingConfig::LOG_TAMPERING.load());
}

TEST_F(ConfigDefaultsTest, MembershipConfig) {
	EXPECT_EQ(*main::MembershipConfig::MEMBERSHIP_TYPES.get(), (std::vector<std::string>{"Premium"}));
	EXPECT_EQ(main::MembershipConfig::GATHERING_ALLOW_ON_MOUNT.load(), 10);
	EXPECT_EQ(main::MembershipConfig::INSTANCES_TITLE_REQ.load(), 10);
	EXPECT_EQ(main::MembershipConfig::INSTANCES_RACE_REQ.load(), 10);
	EXPECT_EQ(main::MembershipConfig::INSTANCES_LEVEL_REQ.load(), 10);
	EXPECT_EQ(main::MembershipConfig::INSTANCES_GROUP_REQ.load(), 10);
	EXPECT_EQ(main::MembershipConfig::INSTANCES_QUEST_REQ.load(), 10);
	EXPECT_EQ(main::MembershipConfig::INSTANCES_COOLDOWN.load(), 10);
	EXPECT_EQ(main::MembershipConfig::EMOTIONS_ALL.load(), 10);
	EXPECT_EQ(main::MembershipConfig::STIGMA_SLOT_QUEST.load(), 10);
	EXPECT_EQ(main::MembershipConfig::DISABLE_SOULSICKNESS.load(), 10);
	EXPECT_EQ(main::MembershipConfig::STIGMA_AUTOLEARN.load(), 10);
	EXPECT_EQ(main::MembershipConfig::QUEST_LIMIT_DISABLED.load(), 10);
	EXPECT_EQ(main::MembershipConfig::CHARACTER_ADDITIONAL_ENABLE.load(), 10);
	EXPECT_EQ(main::MembershipConfig::CHARACTER_ADDITIONAL_COUNT.load(), 8);
}

TEST_F(ConfigDefaultsTest, NameConfig) {
	EXPECT_FALSE(main::NameConfig::ALLOW_CUSTOM_NAMES.load());
	// CHAR_NAME_PATTERN: std::wregex keeps no source text, see the pattern checks in ConfigLoadTest
	// PET_NAME_PATTERN: std::wregex keeps no source text, see the pattern checks in ConfigLoadTest
	EXPECT_TRUE(main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.get()->has_value());                // no default: keeps its value
	EXPECT_EQ(*main::NameConfig::FORBIDDEN_WORDS.get(), (std::vector<std::string>{"sentinel"})); // no default: keeps its value
	EXPECT_EQ(main::NameConfig::RESERVE_OLD_NAME_DAYS.load(), 30);
}

TEST_F(ConfigDefaultsTest, PeriodicSaveConfig) {
	EXPECT_EQ(main::PeriodicSaveConfig::PLAYER_GENERAL.load(), 900);
	EXPECT_EQ(main::PeriodicSaveConfig::PLAYER_ITEMS.load(), 900);
	EXPECT_EQ(main::PeriodicSaveConfig::LEGION_ITEMS.load(), 1200);
	EXPECT_EQ(main::PeriodicSaveConfig::PLAYER_PETS.load(), 10);
}

TEST_F(ConfigDefaultsTest, PlayerTransferConfig) {
	EXPECT_EQ(main::PlayerTransferConfig::MAX_KINAH.load(), 0);
	EXPECT_EQ(*main::PlayerTransferConfig::BIND_ELYOS.get(), "210010000 1212.9423 1044.8516 140.75568 32");
	EXPECT_EQ(*main::PlayerTransferConfig::BIND_ASMO.get(), "220010000 571.0388 2787.3420 299.8750 32");
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_EMOTIONS.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_MOTIONS.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_MACRO.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_NPCFACTIONS.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_PETS.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_RECIPES.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_SKILLS.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_TITLES.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_QUESTS.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_INV.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_WAREHOUSE.load());
	EXPECT_TRUE(main::PlayerTransferConfig::ALLOW_STIGMA.load());
	EXPECT_FALSE(main::PlayerTransferConfig::BLOCK_SAMENAME.load());
	EXPECT_EQ(main::PlayerTransferConfig::REUSE_HOURS.load(), 0);
	EXPECT_EQ(*main::PlayerTransferConfig::REMOVE_SKILL_LIST.get(), "*");
}

TEST_F(ConfigDefaultsTest, PricesConfig) {
	EXPECT_EQ(main::PricesConfig::DEFAULT_PRICES.load(), 100);
	EXPECT_EQ(main::PricesConfig::DEFAULT_MODIFIER.load(), 100);
	EXPECT_EQ(main::PricesConfig::DEFAULT_TAXES.load(), 100);
	EXPECT_EQ(main::PricesConfig::VENDOR_BUY_MODIFIER.load(), 100);
	EXPECT_EQ(main::PricesConfig::VENDOR_SELL_MODIFIER.load(), 20);
}

TEST_F(ConfigDefaultsTest, PunishmentConfig) {
	EXPECT_FALSE(main::PunishmentConfig::PUNISHMENT_ENABLE.load());
	EXPECT_EQ(main::PunishmentConfig::PUNISHMENT_TYPE.load(), 1);
	EXPECT_EQ(main::PunishmentConfig::PUNISHMENT_TIME.load(), 1440);
}

TEST_F(ConfigDefaultsTest, RankingConfig) {
	EXPECT_EQ(cronText(main::RankingConfig::TOP_RANKING_UPDATE_RULE.load()), "0 0 0 ? * *");
	EXPECT_EQ(cronText(main::RankingConfig::TOP_RANKING_DAILY_GP_LOSS_TIME.load()), "0 0 12 ? * *");
	EXPECT_EQ(main::RankingConfig::RANKING_LIST_LEGION_LIMIT.load(), 50);
	EXPECT_EQ(main::RankingConfig::TOP_RANKING_MAX_OFFLINE_DAYS.load(), 0);
	EXPECT_EQ(main::RankingConfig::XFORM_MIN_RANK.load(), detail::AbyssRankEnum::STAR5_OFFICER);
	EXPECT_TRUE(main::RankingConfig::TOP_RANKING_QUOTA.get()->empty());
	EXPECT_TRUE(main::RankingConfig::TOP_RANKING_GP_LOSS.get()->empty());
}

TEST_F(ConfigDefaultsTest, RatesConfig) {
	EXPECT_EQ(*main::RatesConfig::CRAFT_CRIT_CHANCES.get(), (std::vector<float>{15.0f, 30.0f}));
	EXPECT_EQ(*main::RatesConfig::CRAFT_COMBO_CHANCES.get(), (std::vector<float>{25.0f, 50.0f}));
	EXPECT_EQ(*main::RatesConfig::MANASTONE_CHANCES.get(), (std::vector<float>{75.0f, 75.0f}));
	EXPECT_EQ(*main::RatesConfig::ENCHANTMENT_STONE_BASE_CHANCES.get(), (std::vector<float>{65.0f, 65.0f}));
	EXPECT_EQ(*main::RatesConfig::ENCHANTMENT_STONE_AMPLIFIED_CHANCES.get(), (std::vector<float>{61.0f, 61.0f}));
	EXPECT_EQ(*main::RatesConfig::TEMPERING_CHANCES.get(), (std::vector<float>{65.0f, 65.0f}));
	EXPECT_EQ(*main::RatesConfig::XP_SOLO_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::XP_GROUP_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::XP_QUEST_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::XP_GATHERING_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::XP_CRAFTING_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::XP_PVP_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::SKILL_XP_GATHERING_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::SKILL_XP_CRAFTING_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::AP_PVP_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::AP_PVP_LOSS_RATES.get(), (std::vector<float>{1.0f, 1.0f}));
	EXPECT_EQ(*main::RatesConfig::AP_PVE_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::AP_QUEST_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::AP_DREDGION_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::GP_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::DP_PVE_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::DP_PVP_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::QUEST_KINAH_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::DROP_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::GATHERING_COUNT_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::PVP_ARENA_DISCIPLINE_REWARD_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::PVP_ARENA_CHAOS_REWARD_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::PVP_ARENA_HARMONY_REWARD_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::PVP_ARENA_GLORY_REWARD_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
	EXPECT_EQ(*main::RatesConfig::SELL_LIMIT_RATES.get(), (std::vector<float>{1.0f, 2.0f}));
}

TEST_F(ConfigDefaultsTest, SecurityConfig) {
	EXPECT_FALSE(main::SecurityConfig::AION_BIN_CHECK.load());
	EXPECT_FALSE(main::SecurityConfig::TELEPORTATION.load());
	EXPECT_FALSE(main::SecurityConfig::SPEEDHACK.load());
	EXPECT_EQ(main::SecurityConfig::SPEEDHACK_COUNTER.load(), 1);
	EXPECT_FALSE(main::SecurityConfig::ABNORMAL.load());
	EXPECT_EQ(main::SecurityConfig::ABNORMAL_COUNTER.load(), 1);
	EXPECT_EQ(main::SecurityConfig::PUNISH.load(), 0);
	EXPECT_TRUE(main::SecurityConfig::CHECK_ANIMATIONS.load());
	EXPECT_FALSE(main::SecurityConfig::CAPTCHA_ENABLE.load());
	EXPECT_EQ(*main::SecurityConfig::CAPTCHA_APPEAR.get(), "OD");
	EXPECT_EQ(main::SecurityConfig::CAPTCHA_APPEAR_RATE.load(), 5);
	EXPECT_EQ(main::SecurityConfig::CAPTCHA_EXTRACTION_BAN_TIME.load(), 3000);
	EXPECT_EQ(main::SecurityConfig::CAPTCHA_EXTRACTION_BAN_ADD_TIME.load(), 600);
	EXPECT_EQ(main::SecurityConfig::CAPTCHA_BONUS_FP_TIME.load(), 5);
	EXPECT_FALSE(main::SecurityConfig::PASSKEY_ENABLE.load());
	EXPECT_EQ(main::SecurityConfig::PASSKEY_WRONG_MAXCOUNT.load(), 5);
	EXPECT_TRUE(main::SecurityConfig::PINGCHECK_KICK.load());
	EXPECT_EQ(main::SecurityConfig::FLOOD_DELAY.load(), 1);
	EXPECT_EQ(main::SecurityConfig::FLOOD_MSG.load(), 6);
	EXPECT_FALSE(main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.load());
	EXPECT_EQ(main::SecurityConfig::SURVEY_DELAY.load(), 20);
	EXPECT_EQ(main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE.load(), main::SecurityConfig::MultiClientingRestrictionMode::NONE);
	EXPECT_EQ(*main::SecurityConfig::MULTI_CLIENTING_IGNORED_MAC_ADDRESSES.get(), (std::unordered_set<std::string>{}));
	EXPECT_EQ(main::SecurityConfig::MULTI_CLIENTING_FACTION_SWITCH_COOLDOWN_MINUTES.load(), 20);
	EXPECT_FALSE(main::SecurityConfig::HDD_SERIAL_LOCK_ENABLE.load());
	EXPECT_FALSE(main::SecurityConfig::HDD_SERIAL_LOCK_UNLOCKED_ACCOUNTS.load());
}

TEST_F(ConfigDefaultsTest, ShutdownConfig) {
	EXPECT_EQ(main::ShutdownConfig::DELAY.load(), 120);
	EXPECT_EQ(cronText(main::ShutdownConfig::RESTART_SCHEDULE.load()), "0 0 1 ? * *"); // no default: keeps its value
}

TEST_F(ConfigDefaultsTest, SiegeConfig) {
	EXPECT_TRUE(main::SiegeConfig::SIEGE_ENABLED.load());
	EXPECT_FALSE(main::SiegeConfig::BALAUR_AUTO_ASSAULT.load());
	EXPECT_FLOAT_EQ(main::SiegeConfig::BALAUR_ASSAULT_RATE.load(), 1.0f);
	EXPECT_EQ(cronText(main::SiegeConfig::MOLTENUS_SPAWN_SCHEDULE.load()), "0 0 22 ? * SUN");
	EXPECT_FLOAT_EQ(main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load(), 1.0f);
	EXPECT_FLOAT_EQ(main::SiegeConfig::ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER.load(), 1.0f);
	EXPECT_FLOAT_EQ(main::SiegeConfig::BASE_PROTECTOR_HEALTH_MULTIPLIER.load(), 1.0f);
	EXPECT_FLOAT_EQ(main::SiegeConfig::SIEGE_DIFFICULTY_MULTIPLIER.load(), 1.0f);
	EXPECT_EQ(main::SiegeConfig::PANESTERRA_MAX_PLAYERS_PER_TEAM.load(), 100);
	EXPECT_EQ(main::SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load(), 100);
	EXPECT_EQ(cronText(main::SiegeConfig::AHSERION_START_SCHEDULE.load()), "0 50 18 ? * SUN");
	EXPECT_EQ(main::SiegeConfig::LEGION_GP_CAP_PER_MEMBER.load(), 200);
	EXPECT_DOUBLE_EQ(main::SiegeConfig::DOOR_REPAIR_HEAL_PERCENT.load(), 0.01);
	EXPECT_FALSE(main::SiegeConfig::SIEGE_REWARD_BALAUR_VICTORY.load());
	EXPECT_FALSE(main::SiegeConfig::IGNORE_STAFF_ON_LOCATION_CLEAR.load());
}

TEST_F(ConfigDefaultsTest, ThreadConfig) {
	EXPECT_EQ(main::ThreadConfig::BASE_THREAD_POOL_SIZE.load(), 0);
	EXPECT_EQ(main::ThreadConfig::SCHEDULED_THREAD_POOL_SIZE.load(), 0);
	EXPECT_EQ(main::ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING.load(), 5000);
	EXPECT_FALSE(main::ThreadConfig::USE_PRIORITIES.load());
}

TEST_F(ConfigDefaultsTest, WorldConfig) {
	EXPECT_EQ(main::WorldConfig::WORLD_REGION_SIZE.load(), 128);
	EXPECT_EQ(main::WorldConfig::WORLD_MAX_TWINS_USUAL.load(), 1);
	EXPECT_EQ(main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.load(), -1);
	EXPECT_TRUE(main::WorldConfig::WORLD_EMULATE_FASTTRACK.load());
	EXPECT_EQ(*main::WorldConfig::ZONE_HANDLER_DIRECTORY.get(), fs::path("./data/handlers/zone"));
}

TEST_F(ConfigDefaultsTest, NetworkConfig) {
	EXPECT_EQ(*network::NetworkConfig::CLIENT_CONNECT_ADDRESS.get(), (InetSocketAddress{"0.0.0.0", 7777}));
	EXPECT_EQ(*network::NetworkConfig::CLIENT_SOCKET_ADDRESS.get(), (InetSocketAddress{"0.0.0.0", 7777}));
	EXPECT_EQ(*network::NetworkConfig::LOGIN_ADDRESS.get(), (InetSocketAddress{"localhost", 9014}));
	EXPECT_EQ(*network::NetworkConfig::CHAT_ADDRESS.get(), (InetSocketAddress{"localhost", 9021}));
	EXPECT_EQ(*network::NetworkConfig::CHAT_PASSWORD.get(), "");
	EXPECT_EQ(network::NetworkConfig::GAMESERVER_ID.load(), 1);
	EXPECT_EQ(*network::NetworkConfig::LOGIN_PASSWORD.get(), "");
	EXPECT_EQ(network::NetworkConfig::MIN_ACCESS_LEVEL.load(), 0);
	EXPECT_EQ(network::NetworkConfig::MAX_ONLINE_PLAYERS.load(), 100);
	EXPECT_EQ(network::NetworkConfig::NIO_READ_WRITE_THREADS.load(), 1);
	EXPECT_FALSE(network::NetworkConfig::NIO_READ_WRITE_THREADS_UNSAFE_ALLOW.load());
	EXPECT_EQ(network::NetworkConfig::PACKET_PROCESSOR_MIN_THREADS.load(), 4);
	EXPECT_EQ(network::NetworkConfig::PACKET_PROCESSOR_MAX_THREADS.load(), 4);
	EXPECT_EQ(network::NetworkConfig::PACKET_PROCESSOR_THREAD_KILL_THRESHOLD.load(), 3);
	EXPECT_EQ(network::NetworkConfig::PACKET_PROCESSOR_THREAD_SPAWN_THRESHOLD.load(), 50);
	EXPECT_FALSE(network::NetworkConfig::LOG_UNKNOWN_PACKETS.load());
	EXPECT_FALSE(network::NetworkConfig::LOG_IGNORED_PACKETS.load());
	EXPECT_FALSE(network::NetworkConfig::ENABLE_FLOOD_CONNECTIONS.load());
	EXPECT_EQ(network::NetworkConfig::Flood_Tick.load(), 1000);
	EXPECT_EQ(network::NetworkConfig::Flood_SWARN.load(), 10);
	EXPECT_EQ(network::NetworkConfig::Flood_SReject.load(), 20);
	EXPECT_EQ(network::NetworkConfig::Flood_STick.load(), 10);
	EXPECT_EQ(network::NetworkConfig::Flood_LWARN.load(), 30);
	EXPECT_EQ(network::NetworkConfig::Flood_LReject.load(), 60);
	EXPECT_EQ(network::NetworkConfig::Flood_LTick.load(), 60);
}

TEST_F(ConfigDefaultsTest, PffConfig) {
	EXPECT_EQ(network::PffConfig::PFF_MODE.load(), 1);
	EXPECT_TRUE(network::PffConfig::THRESHOLD_MILLIS_BY_PACKET_OPCODE.get()->empty());
}
