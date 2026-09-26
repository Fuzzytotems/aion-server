// SellLimit.getSellLimit (P4-05; m5c-plan.md P-02's getSellLimit half, W-26): the daily sell limit of an account is the limit of the SellLimit band
// that holds the account's highest character level (Account.getMaxPlayerLevel), passed through Rates.SELL_LIMIT.calcResult(Player, long) - a long
// times the membership's float rate, truncated to a long (SellLimit.java:29-37, Rates.java:143-149).
//
// Expectations: tools/oracle `oracle.py m5c-trade --npc 798007 --no-profile --set gameserver.limits.enable=true --set gameserver.siege.enable=false
// --account-max-level N --membership M` (its freshAccountLimit), plus `--set gameserver.rates.sell_limit=0.7` for the last case. The oracle reads
// the SellLimit bands from SellLimit.java and applies the float product.
//
// Test doubles: PlayerPetsDAO.getPlayerPets (PetList::setPlayerPetsLoaderForTests returns no pets), like tests/stats/StatsTestSupport.h.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/SellLimitInfo.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model {
namespace {

using gameobjects::player::Player;
using gameobjects::player::PlayerCommonData;
using runtime::Ref;

/**
 * player_experience_table.xml:3-68, the values verbatim (all 66 rows: PlayerCommonData.setExp caps at getStartExpForLevel(getMaxLevel()), so level
 * 65 needs the whole table).
 */
const char* const EXPERIENCE_TABLE_XML =
	"<player_experience_table>"
	"<exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp><exp>52010</exp><exp>82982</exp>"
	"<exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp><exp>844378</exp><exp>1083018</exp>"
	"<exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp><exp>4811154</exp><exp>6110198</exp>"
	"<exp>7632340</exp><exp>9377726</exp><exp>11395643</exp><exp>13731725</exp><exp>16339413</exp><exp>19378549</exp><exp>23162749</exp>"
	"<exp>27585843</exp><exp>32841197</exp><exp>39127217</exp><exp>47350762</exp><exp>57829684</exp><exp>70654362</exp><exp>87571065</exp>"
	"<exp>107018757</exp><exp>129815732</exp><exp>157211282</exp><exp>189272188</exp><exp>226933751</exp><exp>267247400</exp>"
	"<exp>310053925</exp><exp>355815203</exp><exp>404823687</exp><exp>456685353</exp><exp>511683757</exp><exp>570162075</exp>"
	"<exp>632268545</exp><exp>701585822</exp><exp>776831823</exp><exp>857090855</exp><exp>947120930</exp><exp>1051346275</exp>"
	"<exp>1175571620</exp><exp>1318550121</exp><exp>1484090156</exp><exp>1674064804</exp><exp>1913274732</exp><exp>2162140395</exp>"
	"<exp>2419819338</exp><exp>2700930959</exp><exp>3209499233</exp><exp>3794060468</exp>"
	"</player_experience_table>";

/** Sets a ConfigValue for the scope and restores the previous value (tests share the process-wide configuration) */
template <class T>
class ConfigValueScope {
public:
	ConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~ConfigValueScope() { config.set(previous ? *previous : T{}); }
	ConfigValueScope(const ConfigValueScope&) = delete;
	ConfigValueScope& operator=(const ConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

std::vector<Ref<gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

/** getSellLimit, or -1 for its NoSuchElementException (no band holds the level), so that one failing case does not hide the next ones */
int64_t sellLimitOf(Player& player) {
	try {
		return getSellLimit(player);
	} catch (const runtime::NoSuchElementException&) {
		return -1;
	}
}

class SellLimitTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, EXPERIENCE_TABLE_XML));
	}

	void TearDown() override {
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	struct Fixture {
		Ref<account::Account> account;
		Ref<PlayerCommonData> playerData;
		/** a second character of the account: Account.getMaxPlayerLevel reads every character's level, not the seller's */
		Ref<PlayerCommonData> otherData;
		Ref<Player> player;
	};

	/**
	 * Java PlayerService.getPlayer's account side: an account with its warehouse and two characters, the seller (fresh common data, level 0) and a
	 * second character (level 0 until the test sets one), then new Player(...) for the seller.
	 */
	static Fixture makeAccount(int32_t accountId, int8_t membership) {
		Fixture f;
		f.account = account::Account::create(accountId);
		f.account->setMembership(membership);
		f.account->setAccountWarehouse(
			std::make_unique<items::storage::PlayerStorage>(*f.account, items::storage::StorageType::ACCOUNT_WAREHOUSE));
		Ref<gameobjects::player::PlayerAppearance> appearance = gameobjects::player::PlayerAppearance::create();
		const int32_t sellerId = accountId * 10;
		f.playerData = PlayerCommonData::create(sellerId);
		f.playerData->setName("Seller" + std::to_string(accountId));
		f.playerData->setRace(Race::ELYOS);
		f.playerData->setPlayerClass(PlayerClass::WARRIOR);
		f.account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*f.account, *f.playerData, *appearance));
		f.otherData = PlayerCommonData::create(sellerId + 1);
		f.otherData->setName("Alt" + std::to_string(accountId));
		f.otherData->setRace(Race::ELYOS);
		f.otherData->setPlayerClass(PlayerClass::WARRIOR);
		f.account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*f.account, *f.otherData, *appearance));
		f.player = gameobjects::VisibleObject::create<Player>(*f.account->getPlayerAccountData(sellerId), *f.account);
		return f;
	}

	runtime::ManualClock clock{0};
	xml::LoadContext context;
};

TEST_F(SellLimitTest, TheBandOfTheAccountsHighestCharacterLevelGivesTheLimit) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// gameserver.rates.sell_limit = 1.0, 2.0 (config/main/rates.properties:170); membership 0 picks 1.0
	ConfigValueScope<std::vector<float>> rates(configs::main::RatesConfig::SELL_LIMIT_RATES, {1.0f, 2.0f});
	Fixture f = makeAccount(71, 0);

	// both characters at level 0: Account.getMaxPlayerLevel starts at 1 (Account.java:219-227), the band LIMIT_1_30
	EXPECT_EQ(sellLimitOf(*f.player), 5300047) << "account max level " << f.account->getMaxPlayerLevel();

	// the other character's level decides; each band's both ends. PlayerCommonData.setLevel on an offline starting-class character above the start
	// of level 10 gives the daeva levels (PlayerCommonData.java:276)
	struct Case {
		int32_t level;
		int64_t limit;
	};
	for (const Case& c : {Case{30, 5300047}, Case{31, 7100047}, Case{40, 7100047}, Case{41, 12050047}, Case{55, 12050047}, Case{56, 14600047},
			 Case{60, 14600047},
			 // LIMIT_61_65's 17150047 is not a float: (float) 17150047 is 17150048 (24-bit significand), and (long) (17150047 * 1.0f) keeps it
			 Case{61, 17150048}, Case{65, 17150048}}) {
		f.otherData->setLevel(c.level);
		EXPECT_EQ(sellLimitOf(*f.player), c.limit) << "account max level " << f.account->getMaxPlayerLevel() << " (set " << c.level << ")";
	}
}

TEST_F(SellLimitTest, TheMembershipRatePicksTheFactorOfAFloatProduct) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	{
		ConfigValueScope<std::vector<float>> rates(configs::main::RatesConfig::SELL_LIMIT_RATES, {1.0f, 2.0f});
		Fixture premium = makeAccount(72, 1);
		premium.otherData->setLevel(30);
		EXPECT_EQ(sellLimitOf(*premium.player), 10600094) << "5300047 * 2.0f";
		premium.otherData->setLevel(65);
		EXPECT_EQ(sellLimitOf(*premium.player), 34300096) << "17150047 * 2.0f = 17150048f * 2";
		// Rates.get: a membership above the configured rates takes the last one (Rates.java:166-173)
		Fixture vip = makeAccount(73, 9);
		vip.otherData->setLevel(65);
		EXPECT_EQ(sellLimitOf(*vip.player), 34300096);
	}
	{
		ConfigValueScope<std::vector<float>> rates(configs::main::RatesConfig::SELL_LIMIT_RATES, {0.7f});
		Fixture reduced = makeAccount(74, 0);
		reduced.otherData->setLevel(30);
		EXPECT_EQ(sellLimitOf(*reduced.player), 3710032) << "(long) (5300047 * 0.7f)";
	}
}

} // namespace
} // namespace aion::gameserver::model
