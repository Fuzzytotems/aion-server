// P5-08 player services, M5a subset (m5a-plan.md E1-02..E1-04): HDD bans, the security token, multi-clienting in NONE mode, kisk binds, bind point
// cooldowns, prison status, pets, duels, recall requests, abyss skills, the life stats restore tasks and the startup cron schedules.
//
// Expectations are derived by hand from HDDBanService.java, SecurityTokenService.java, MultiClientingService.java:24-61, KiskService.java:66-89,
// BindPointTeleportService.java:30-36,110-137, PunishmentService.java:96-116, PetService.java:63-67, DuelService.java:243-280,
// RecallService.java:49-104, AbyssSkillService.java:20-40, LifeStatsRestoreService.java:24-38, PlayerLimitService.java:51-53 and
// AbyssRankUpdateService.java:28-34.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "PlayerEventsTestSupport.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/services/LifeStatsRestoreService.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"
#include "aion/gameserver/services/abyss/AbyssSkillService.h"
#include "aion/gameserver/services/ban/HDDBanService.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/player/MultiClientingService.h"
#include "aion/gameserver/services/player/PlayerLimitService.h"
#include "aion/gameserver/services/player/SecurityTokenService.h"
#include "aion/gameserver/services/teleport/BindPointTeleportService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/services/toypet/PetService.h"

namespace aion::gameserver::playerevents::test {
namespace {

using runtime::Ptr;
using runtime::Ref;

class PlayerServicesM5aTest : public PlayerEventsTest {
protected:
	void TearDown() override {
		services::cron::CronService::resetForTests();
		PlayerEventsTest::TearDown();
	}

	static void startCronService() {
		services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
	}
};

TEST_F(PlayerServicesM5aTest, HddBansExpireWithTheirBanTime) {
	services::ban::HDDBanService& service = services::ban::HDDBanService::getInstance();
	const int64_t now = commons::utils::currentTimeMillis();
	EXPECT_FALSE(service.isBanned("SERIAL-A")) << "unknown serial";

	service.loadBan("SERIAL-A", now + 3'600'000);
	service.loadBan("SERIAL-B", now - 1'000);
	EXPECT_TRUE(service.isBanned("SERIAL-A"));
	EXPECT_FALSE(service.isBanned("SERIAL-B")) << "Java: banTime > currentTimeMillis";

	// addBan / removeBan also inform the login server; without a link sendPacket returns false (LoginServer.sendPacket)
	service.addBan("SERIAL-C", commons::database::Timestamp(std::chrono::milliseconds(now + 60'000)));
	EXPECT_TRUE(service.isBanned("SERIAL-C"));
	service.removeBan("SERIAL-C");
	EXPECT_FALSE(service.isBanned("SERIAL-C"));
	service.loadBan("SERIAL-A", now - 1); // a reload replaces the ban time
	EXPECT_FALSE(service.isBanned("SERIAL-A"));

	EXPECT_THROW(service.addBan("SERIAL-D", std::nullopt), runtime::NullPointerException) << "Java: banTime.getTime() on null";
}

TEST_F(PlayerServicesM5aTest, SecurityTokenIsBase64OfSixteenRandomBytes) {
	Ref<model::account::Account> account = model::account::Account::create(7);
	services::player::SecurityTokenService::generateToken(*account);
	const std::string token = account->getSecurityToken();
	ASSERT_EQ(token.size(), 24u) << "Base64 of 16 bytes: 22 characters and \"==\"";
	EXPECT_EQ(token.substr(22), "==");
	EXPECT_TRUE(std::ranges::all_of(token.substr(0, 22), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '/'; }));
	// the 22nd character encodes the last 2 bits of byte 16 plus 4 zero bits: one of the 4 characters A, Q, g, w
	EXPECT_NE(std::string("AQgw").find(token[21]), std::string::npos);

	services::player::SecurityTokenService::generateToken(*account);
	EXPECT_NE(account->getSecurityToken(), token) << "a new random token";
}

TEST_F(PlayerServicesM5aTest, MultiClientingInNoneModeAlwaysAllowsTheLogin) {
	PlayerFixture f = makePlayer(1, 100);
	AtomicConfigScope<configs::main::SecurityConfig::MultiClientingRestrictionMode> mode(configs::main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE,
		configs::main::SecurityConfig::MultiClientingRestrictionMode::NONE);
	// NONE reads no connection data (MultiClientingService.java:25,37): a null connection is never dereferenced
	EXPECT_TRUE(services::player::MultiClientingService::tryEnterWorld(*f.player, nullptr));
	services::player::MultiClientingService::onLeaveWorld(*f.player); // no session of the account: nothing to update
}

TEST_F(PlayerServicesM5aTest, KiskBindingOfAnOfflinePlayerAndOwnerRegistration) {
	PlayerFixture f = makePlayer(2, 200);
	services::KiskService& service = services::KiskService::getInstance();
	service.onLogin(*f.player);  // no stored binding
	service.onLogout(*f.player); // no kisk: nothing stored
	service.onLogin(*f.player);
	EXPECT_FALSE(service.haveKisk(2));
	EXPECT_THROW(service.haveKisk(std::nullopt), runtime::NullPointerException) << "Java: ConcurrentHashMap.containsKey(null)";

	// bind points: without a kisk no packet is built; the obelisk bind point of a player reads BindPointPosition
	services::teleport::TeleportService::sendKiskBindPoint(*f.player);
	f.player->setBindPoint(Ptr<model::gameobjects::player::BindPointPosition>(
		model::gameobjects::player::BindPointPosition::create(210010000, 1.0f, 2.0f, 3.0f, 4)));
	services::teleport::TeleportService::sendObeliskBindPoint(*f.player);
}

TEST_F(PlayerServicesM5aTest, LoginWithoutCooldownPrisonOrPetsSendsNothing) {
	PlayerFixture f = makePlayer(3, 300);
	runtime::resetUnportedHitsForTests();
	services::teleport::BindPointTeleportService::onLogin(*f.player);
	f.player->setPrisonEndTimeMillis(0);
	services::PunishmentService::updatePrisonStatus(*f.player);
	services::toypet::PetService::getInstance().onPlayerLogin(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(PlayerServicesM5aTest, DuelAndRecallStateOfAPlayerWithoutDuelOrRequest) {
	PlayerFixture first = makePlayer(4, 400);
	PlayerFixture second = makePlayer(5, 500);
	services::DuelService& duels = services::DuelService::getInstance();
	EXPECT_FALSE(duels.getOpponentId(*first.player));
	EXPECT_FALSE(duels.isDueling(*first.player));
	EXPECT_FALSE(duels.isDueling(*first.player, *second.player));

	services::RecallService& recall = services::RecallService::getInstance();
	EXPECT_FALSE(recall.hasPendingRequest(*first.player));
	recall.cancel(*first.player, services::RecallService::CancelReason::CANCELLED); // PlayerLeaveWorldService: no request, nothing to notify
	recall.accept(*first.player);
}

TEST_F(PlayerServicesM5aTest, CreationLearnsTheAutolearnSkillsOfTheStartingClass) {
	auto skill = [](int32_t id, int32_t lvl) {
		return "<skill_template skill_id=\"" + std::to_string(id) + "\" name=\"s" + std::to_string(id) + "\" nameId=\"1\" lvl=\"" + std::to_string(lvl) +
			R"(" skilltype="PHYSICAL" skillsubtype="NONE" activation="ACTIVE" duration="0" stack="S)" + std::to_string(id) + "\"/>";
	};
	PublishedHolder skillData(dataholders::DataManager::SKILL_DATA,
		bindXml<dataholders::SkillData>("<skill_data>" + skill(101, 1) + skill(102, 2) + skill(103, 1) + skill(104, 1) + skill(30001, 1) + "</skill_data>"));
	// Warrior level 1: 101 (autolearn), 102 (not autolearn), 30001 (human gathering, autolearn for the starting class); level 2: 104 (autolearn);
	// 103 belongs to the Gladiator
	PublishedHolder skillTree(dataholders::DataManager::SKILL_TREE_DATA,
		bindXml<dataholders::SkillTreeData>(R"(<skill_tree><skill classId="WARRIOR" skillId="101" minLevel="1" autolearn="true"/>)"
											R"(<skill classId="WARRIOR" skillId="102" minLevel="1"/>)"
											R"(<skill classId="WARRIOR" skillId="30001" minLevel="1" autolearn="true"/>)"
											R"(<skill classId="WARRIOR" skillId="104" minLevel="2" autolearn="true"/>)"
											R"(<skill classId="GLADIATOR" skillId="103" minLevel="1" autolearn="true"/></skill_tree>)"));
	PlayerFixture f = makePlayer(9, 900);
	f.commonData->setPlayerClass(model::PlayerClass::WARRIOR);
	f.player->setSkillList(Ptr<model::skill::PlayerSkillList>(model::skill::PlayerSkillList::create()));

	runtime::resetUnportedHitsForTests();
	SKIP_IF_UNPORTED(services::SkillLearnService::learnNewSkills(*f.player, 1, 1));
	Ptr<model::skill::PlayerSkillList> skills = f.player->getSkillList();
	EXPECT_TRUE(skills->isSkillPresent(101));
	EXPECT_EQ(skills->getSkillLevel(101), 1) << "the skill level comes from the skill template";
	EXPECT_TRUE(skills->isSkillPresent(30001)) << "human gathering for a starting class";
	EXPECT_FALSE(skills->isSkillPresent(102)) << "not autolearn";
	EXPECT_FALSE(skills->isSkillPresent(103)) << "another class";
	EXPECT_FALSE(skills->isSkillPresent(104)) << "a higher level";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "creation path: no effect controller packets, no passive effects, no recipes";
}

TEST_F(PlayerServicesM5aTest, AbyssSkillsOfAFreshCharacterAreOnlyRemoved) {
	PlayerFixture f = makePlayer(6, 600);
	// without an abyss rank the update returns at once (AbyssSkillService.java:22)
	services::abyss::AbyssSkillService::updateSkills(*f.player);
}

TEST_F(PlayerServicesM5aTest, TheRestoreTaskRetainsTheLifeStatsOwnerUntilItIsCancelled) {
	PlayerFixture f = makePlayer(7, 700);
	// the TestLifeStats double is read through Creature (Player::getLifeStats narrows to PlayerLifeStats)
	Ptr<model::stats::container::CreatureLifeStats> lifeStats = static_cast<model::gameobjects::Creature&>(*f.player).getLifeStats();
	ASSERT_TRUE(lifeStats);
	const uint32_t before = f.player->refCount();
	runtime::FutureRef task = services::LifeStatsRestoreService::getInstance().scheduleRestoreTask(*lifeStats);
	ASSERT_TRUE(task);
	EXPECT_GT(f.player->refCount(), before) << "the pending task holds the life stats, a part that retains its player (plan B-03, Q3)";
	EXPECT_EQ(executor->pendingTaskCount(), 1u);

	task->cancel(false);
	task.reset();
	lifeStats = nullptr;
	f.account.reset();
	f.commonData.reset();
	f.player.reset();
	scope.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedTestPlayers.load(), 1) << "cancelling releases the task and with it the player";
	scope.emplace(AION_TASK_INFO(runtime::TaskKind::TEST));
}

TEST_F(PlayerServicesM5aTest, TheFpAndHpTasksAreScheduledAtJavasRates) {
	PlayerFixture f = makePlayer(8, 800);
	// the TestLifeStats double is read through Creature (Player::getLifeStats narrows to PlayerLifeStats)
	Ptr<model::stats::container::CreatureLifeStats> lifeStats = static_cast<model::gameobjects::Creature&>(*f.player).getLifeStats();
	ASSERT_TRUE(lifeStats);
	runtime::FutureRef hp = services::LifeStatsRestoreService::getInstance().scheduleHpRestoreTask(*lifeStats);
	ASSERT_TRUE(hp);
	EXPECT_EQ(executor->pendingTaskCount(), 1u);
	EXPECT_FALSE(hp->isDone());
	hp->cancel(false);
	EXPECT_TRUE(hp->isCancelled());
	EXPECT_EQ(executor->pendingTaskCount(), 0u);
}

TEST_F(PlayerServicesM5aTest, StartupSchedulesOfLimitsAndAbyssRanks) {
	startCronService();
	services::cron::CronExpression limits("0 0 0 ? * *");
	services::cron::CronExpression ranking("0 0 0 ? * *");
	services::cron::CronExpression gpLoss("0 0 12 ? * *");
	AtomicConfigScope<const services::cron::CronExpression*> limitsUpdate(configs::main::CustomConfig::LIMITS_UPDATE, &limits);
	AtomicConfigScope<const services::cron::CronExpression*> rankingRule(configs::main::RankingConfig::TOP_RANKING_UPDATE_RULE, &ranking);
	AtomicConfigScope<const services::cron::CronExpression*> gpLossTime(configs::main::RankingConfig::TOP_RANKING_DAILY_GP_LOSS_TIME, &gpLoss);

	services::player::PlayerLimitService::getInstance().scheduleUpdate();
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 1u);
	services::abyss::AbyssRankUpdateService::scheduleUpdate();
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 3u) << "the rank update and the daily GP loss";

	AtomicConfigScope<const services::cron::CronExpression*> noRule(configs::main::RankingConfig::TOP_RANKING_UPDATE_RULE, nullptr);
	EXPECT_THROW(services::abyss::AbyssRankUpdateService::scheduleUpdate(), runtime::NullPointerException);
}

TEST_F(PlayerServicesM5aTest, TheRankUpdateJobsAreWarnStubs) {
	runtime::resetPartialHitsForTests();
	runtime::resetUnportedHitsForTests();
	services::abyss::AbyssRankUpdateService::performUpdate();
	EXPECT_EQ(runtime::partialHitCount(), 1u) << "AION_PARTIAL, the cron job must not throw";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
