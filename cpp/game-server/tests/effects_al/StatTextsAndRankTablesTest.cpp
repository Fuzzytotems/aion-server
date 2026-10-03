// P5-01's last portable stats bodies (M5b-2's stats group, ported 2026-10-02 on the owner's request): the debug strings of Stat2 and of the stat
// functions (toString, Java's string concatenation with Float.toString for Stat2's floats), and the companions of the generated enums
// AbyssRankEnum and XPLossEnum (utils/stats/AbyssRankEnumInfo.h, XPLossEnumInfo.h) pinned to the Java tables (AbyssRankEnum.java:20-37,
// XPLossEnum.java:8-14). Stat2 needs a creature owner, so the cases run on the DaevaEffectTest fixture.

#include "DaevaEffectsTestSupport.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatAbsFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/utils/stats/XPLossEnumInfo.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using gameserver::model::stats::container::StatEnum;
namespace functions = gameserver::model::stats::calc::functions;
namespace ranks = utils::stats;
using utils::stats::AbyssRankEnum;
using utils::stats::XPLossEnum;

using StatTextsAndRankTablesTest = DaevaEffectTest;

TEST_F(StatTextsAndRankTablesTest, Stat2PrintsItsStatBaseAndBonusAsJavaFloats) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701901);
	std::unique_ptr<gameserver::model::stats::calc::Stat2> stat = npc->getGameStats()->getStat(StatEnum::FLY_TIME, 5);
	stat->addToBonus(2.5f);
	EXPECT_EQ(stat->toString(), "[FLY_TIME base=5.0, bonus=2.5]") << "Stat2.java:121-123";
}

TEST_F(StatTextsAndRankTablesTest, TheStatFunctionsPrintTheirNameStatBonusValueAndPriority) {
	// StatFunction.java:81-83 and the four subclasses' "<Class> [" + super.toString() + "]"
	EXPECT_EQ(functions::RcStatFunction<functions::StatAddFunction>::create(StatEnum::MAXHP, 10, false)->toString(),
		"StatAddFunction [stat=MAXHP, bonus=false, value=10, priority=30]");
	EXPECT_EQ(functions::RcStatFunction<functions::StatRateFunction>::create(StatEnum::MAXHP, 5, true)->toString(),
		"StatRateFunction [stat=MAXHP, bonus=true, value=5, priority=50]");
	EXPECT_EQ(functions::RcStatFunction<functions::StatSetFunction>::create(StatEnum::SPEED, 6000)->toString(),
		"StatSetFunction [stat=SPEED, bonus=false, value=6000, priority=40]");
	EXPECT_EQ(functions::RcStatFunction<functions::StatAbsFunction>::create(StatEnum::SPEED, 6000, false)->toString().rfind("StatAbsFunction [stat=SPEED", 0),
		0u);

	// StatFunctionProxy.java:68-71; a proxy without an owner prints "null"
	Ref<functions::StatFunctionProxy> proxy =
		functions::StatFunctionProxy::create(nullptr, *functions::RcStatFunction<functions::StatAddFunction>::create(StatEnum::MAXHP, 10, false));
	EXPECT_EQ(proxy->toString(), "Proxy [name=MAXHP, bonus=false, value=10, priority=30, owner=null]");
}

TEST_F(StatTextsAndRankTablesTest, TheAbyssRankTableAndItsQuotas) {
	EXPECT_EQ(ranks::pointsGained(AbyssRankEnum::GRADE9_SOLDIER), 300);
	EXPECT_EQ(ranks::pointsLost(AbyssRankEnum::GRADE9_SOLDIER), 90);
	EXPECT_EQ(ranks::requiredAP(AbyssRankEnum::GRADE1_SOLDIER), 150800);
	EXPECT_EQ(ranks::requiredGP(AbyssRankEnum::GRADE1_SOLDIER), 0);
	EXPECT_EQ(ranks::pointsLost(AbyssRankEnum::STAR5_OFFICER), 1482);
	EXPECT_EQ(ranks::requiredAP(AbyssRankEnum::STAR1_OFFICER), 0);
	EXPECT_EQ(ranks::requiredGP(AbyssRankEnum::SUPREME_COMMANDER), 12437);
	EXPECT_EQ(ranks::pointsGained(AbyssRankEnum::SUPREME_COMMANDER), 5916);

	// getQuota: RankingConfig.TOP_RANKING_QUOTA.getOrDefault(this, 0)
	auto previous = configs::main::RankingConfig::TOP_RANKING_QUOTA.get();
	configs::main::RankingConfig::TOP_RANKING_QUOTA.set(std::map<configs::detail::AbyssRankEnum, int32_t>{{configs::detail::AbyssRankEnum::GENERAL, 5}});
	EXPECT_EQ(ranks::quota(AbyssRankEnum::GENERAL), 5);
	EXPECT_EQ(ranks::quota(AbyssRankEnum::COMMANDER), 0) << "not in the map: 0";
	configs::main::RankingConfig::TOP_RANKING_QUOTA.set(previous ? *previous : std::map<configs::detail::AbyssRankEnum, int32_t>{});
}

TEST_F(StatTextsAndRankTablesTest, TheXpLossTable) {
	EXPECT_EQ(ranks::level(XPLossEnum::LEVEL_6), 6);
	EXPECT_EQ(ranks::level(XPLossEnum::LEVEL_40), 40);
	EXPECT_EQ(ranks::level(XPLossEnum::LEVEL_65), 65);
	EXPECT_DOUBLE_EQ(ranks::param(XPLossEnum::LEVEL_30), 1.0);
	EXPECT_DOUBLE_EQ(ranks::param(XPLossEnum::LEVEL_40), 0.35);
	EXPECT_DOUBLE_EQ(ranks::param(XPLossEnum::LEVEL_50), 0.25);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
