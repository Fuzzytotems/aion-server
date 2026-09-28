// P4-09 holder hooks, lookups and post-processing on small fixtures (docs/design/static-data.md §4 V6). Expectations are derived by hand from
// the Java holders (dataholders/*.java), NpcStatCalculation.java, DataManager.java and java.util.HashMap; the fixtures use the XML names of the
// data files and XSDs.

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/AtreianPassportData.bind.h"
#include "aion/gameserver/dataholders/AtreianPassportData.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/ChallengeData.bind.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/CustomDrop.bind.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GlobalDropData.bind.h"
#include "aion/gameserver/dataholders/GlobalDropData.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.bind.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.bind.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcShoutData.bind.h"
#include "aion/gameserver/dataholders/NpcShoutData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/StaticDoorData.bind.h"
#include "aion/gameserver/dataholders/StaticDoorData.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/dataholders/WorldRaidData.bind.h"
#include "aion/gameserver/dataholders/WorldRaidData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/stats/calc/NpcStatCalculation.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/event/AtreianPassport.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcNames.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalRule.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npcshout/NpcShout.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnMap.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dataholders {
namespace {

using model::stats::calc::NpcStatCalculation;
using model::stats::container::StatEnum;
using model::templates::npc::NpcRank;
using model::templates::npc::NpcRating;

template <class T>
std::unique_ptr<T> bindXml(const std::string& text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

template <class T>
std::unique_ptr<T> bindXml(xml::LoadContext& context, const std::string& text) {
	return xml::bindString<T>(context, text);
}

/** Captures the messages of one logger ("level|message" per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

template <class T, class F>
std::vector<int32_t> idsOf(const std::vector<T>& list, F&& id) {
	std::vector<int32_t> ids;
	for (const T& element : list)
		ids.push_back(id(element));
	return ids;
}

// ---- detail::JavaHashMapOrder ------------------------------------------------------------------------------------------------------------------

TEST(JavaHashMapOrderTest, StringHashCodeIsJavas) {
	EXPECT_EQ(detail::javaHashCode(std::string_view("")), 0);
	EXPECT_EQ(detail::javaHashCode(std::string_view("abc")), 96354); // 'a' * 31^2 + 'b' * 31 + 'c'
	EXPECT_EQ(detail::javaHashCode(std::string_view("Aa")), detail::javaHashCode(std::string_view("BB"))) << "the classic collision: 2112";
	EXPECT_EQ(detail::javaHashCode(std::string_view("Aa")), 2112);
	// U+00E9 is one UTF-16 unit (233); U+1F600 is two (0xD83D 0xDE00): 55357 * 31 + 56832 = 1772899
	EXPECT_EQ(detail::javaHashCode(std::string_view("\xC3\xA9")), 233);
	EXPECT_EQ(detail::javaHashCode(std::string_view("\xF0\x9F\x98\x80")), 1772899);
	// "polygenelubricants".hashCode() == Integer.MIN_VALUE (wrapping arithmetic)
	EXPECT_EQ(detail::javaHashCode(std::string_view("polygenelubricants")), INT32_MIN);
}

TEST(JavaHashMapOrderTest, PutReplacesAndPutIfAbsentKeeps) {
	detail::JavaHashMapOrder<std::string, int> order;
	for (std::string_view key : {"BB", "Aa", "c"})
		order.put(std::string(key), static_cast<int>(key.size()), detail::javaHashCode(key));
	order.put("BB", 42, detail::javaHashCode(std::string_view("BB")));
	EXPECT_TRUE(order.putIfAbsent("Aa", 7, detail::javaHashCode(std::string_view("Aa"))));
	EXPECT_FALSE(order.putIfAbsent("d", 8, detail::javaHashCode(std::string_view("d"))));
	// table of 16: "BB" and "Aa" (2112 -> 2112 ^ 0 = 2112, index 0) share bucket 0 in put order; "c" (99, index 3); "d" (100, index 4)
	EXPECT_EQ(order.keys(), (std::vector<std::string>{"BB", "Aa", "c", "d"}));
	EXPECT_EQ(order.values(), (std::vector<int>{42, 2, 1, 8}));
	EXPECT_EQ(order.size(), 4u);
}

TEST(JavaHashMapOrderTest, ComputeIfAbsentInsertsAtTheBucketHeadAndResizesBeforeTheNextCall) {
	// java.util.HashMap.computeIfAbsent: `tab[i] = newNode(hash, key, v, first)` (head of the bucket), and the table is resized at the start of
	// a call while size > threshold; putVal appends and resizes right after the insert that exceeds the threshold
	detail::JavaHashMapOrder<int32_t, int32_t> computed;
	detail::JavaHashMapOrder<int32_t, int32_t> put;
	for (int32_t key : {17, 1, 2}) {
		EXPECT_FALSE(computed.computeIfAbsent(key, key, detail::javaHashCode(key)));
		put.put(key, key, detail::javaHashCode(key));
	}
	EXPECT_TRUE(computed.computeIfAbsent(17, 99, detail::javaHashCode(17))) << "present: the value is kept";
	EXPECT_EQ(computed.keys(), (std::vector<int32_t>{1, 17, 2})) << "17 and 1 share bucket 1; 1 went to its head";
	EXPECT_EQ(computed.values(), (std::vector<int32_t>{1, 17, 2}));
	EXPECT_EQ(put.keys(), (std::vector<int32_t>{17, 1, 2}));

	// 12 keys fill a table of 16 to its threshold; key 16 (bucket 0, at the head before key 0) makes the size 13 without a resize
	detail::JavaHashMapOrder<int32_t, int32_t> lazy;
	for (int32_t key = 0; key < 12; ++key)
		lazy.computeIfAbsent(key, key, detail::javaHashCode(key));
	lazy.computeIfAbsent(16, 16, detail::javaHashCode(16));
	EXPECT_EQ(lazy.keys(), (std::vector<int32_t>{16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11})) << "still 16 buckets";
	EXPECT_TRUE(lazy.computeIfAbsent(5, 5, detail::javaHashCode(5)));
	EXPECT_EQ(lazy.keys(), (std::vector<int32_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16})) << "the next call resized to 32: 16 moved to bucket 16";

	// remove unlinks; a new computeIfAbsent of a key goes to the head again, put appends
	EXPECT_TRUE(computed.remove(1, detail::javaHashCode(1)));
	EXPECT_FALSE(computed.remove(1, detail::javaHashCode(1)));
	computed.computeIfAbsent(33, 33, detail::javaHashCode(33));
	computed.put(1, 1, detail::javaHashCode(1));
	EXPECT_EQ(computed.keys(), (std::vector<int32_t>{33, 17, 1, 2})) << "33 at the head of bucket 1, put appends 1";
	EXPECT_EQ(computed.size(), 4u);
}

TEST(JavaHashMapOrderTest, ComputeIfAbsentTreeifiesSmallTablesAtSevenKeys) {
	// computeIfAbsent: binCount (the keys already in the bucket) >= TREEIFY_THRESHOLD - 1 = 7 -> treeifyBin, which resizes a table below 64
	detail::JavaHashMapOrder<int32_t, int32_t> order;
	for (int32_t key : {0, 16, 32, 48, 64, 80, 96, 112})
		order.computeIfAbsent(key, key, detail::javaHashCode(key));
	// bucket 0 of 16 was [112, 96, 80, 64, 48, 32, 16, 0]; the resize to 32 splits it in order into buckets 0 and 16
	EXPECT_EQ(order.keys(), (std::vector<int32_t>{96, 64, 32, 0, 112, 80, 48, 16}));
	// putVal needs 8 keys already in the bucket: 8 keys in bucket 0 of 16 stay there
	detail::JavaHashMapOrder<int32_t, int32_t> put;
	for (int32_t key : {0, 16, 32, 48, 64, 80, 96, 112})
		put.put(key, key, detail::javaHashCode(key));
	EXPECT_EQ(put.keys(), (std::vector<int32_t>{0, 16, 32, 48, 64, 80, 96, 112}));
}

TEST(JavaHashMapOrderTest, TreeBucketsOfLargeTablesAreReportedOncePerMap) {
	const int32_t before = detail::javaTreeifiedBucketCount();
	detail::JavaHashMapOrder<int32_t, int32_t> order;
	for (int32_t key = 1; key <= 25; ++key) // 25 keys: 64 buckets (threshold 48)
		order.put(key, key, detail::javaHashCode(key));
	for (int32_t i = 0; i < 9; ++i) { // bucket 63: 63, 191, 319, ... (the 9th key finds 8 keys in the bucket)
		int32_t key = 63 + 128 * i;
		order.put(key, key, detail::javaHashCode(key));
		EXPECT_EQ(detail::javaTreeifiedBucketCount(), before + (i == 8 ? 1 : 0)) << "key " << key;
	}
	order.put(63 + 128 * 9, 0, detail::javaHashCode(63 + 128 * 9));
	EXPECT_EQ(detail::javaTreeifiedBucketCount(), before + 1) << "once per map";
	EXPECT_EQ(order.size(), 35u);
}

// ---- NpcStatCalculation ------------------------------------------------------------------------------------------------------------------------

TEST(NpcStatCalculationTest, HandComputedValues) {
	// PHYSICAL_ATTACK, level 10: -0.0007 * 1000 + 0.1 * 100 + 5.3 * 10 = 62.3, NORMAL and NOVICE 1 -> 62
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::PHYSICAL_ATTACK, NpcRating::NORMAL, NpcRank::NOVICE, 10), 62);
	// MAGICAL_ATTACK, level 30: 600 * ELITE 0.5 * DISCIPLINED 1.45 = 435
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::MAGICAL_ATTACK, NpcRating::ELITE, NpcRank::DISCIPLINED, 30), 435);
	// MAGICAL_RESIST, level 65: 0.1 * 4225 + 16.5 * 65 = 1495, LEGENDARY 1.35, VETERAN 1.05: 2119.16 -> 2119
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::MAGICAL_RESIST, NpcRating::LEGENDARY, NpcRank::VETERAN, 65), 2119);
	// PHYSICAL_CRITICAL_RESIST, level 60: (60 - 50) * 2.5 = 25, HERO 13.5, MASTER 1.8: 607.5 -> Math.round 608 (ties up)
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::PHYSICAL_CRITICAL_RESIST, NpcRating::HERO, NpcRank::MASTER, 60), 608);
	// STUNLIKE_RESISTANCE of NORMAL npcs: 100 * 0 = 0; ELITE VETERAN: 100 * 5 * 1.4 = 700
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::STUNLIKE_RESISTANCE, NpcRating::NORMAL, NpcRank::MASTER, 40), 0);
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::STUNLIKE_RESISTANCE, NpcRating::ELITE, NpcRank::VETERAN, 40), 700);
	// below level 50 the critical resists are negative: (10 - 50) * 1.1 = -44 (JUNK 1, EXPERT 1.2) -> -52.8 -> -53
	EXPECT_EQ(NpcStatCalculation::calculateStat(StatEnum::MAGICAL_CRITICAL_RESIST, NpcRating::JUNK, NpcRank::EXPERT, 10), -53);
	try {
		static_cast<void>(NpcStatCalculation::calculateStat(StatEnum::MAXHP, NpcRating::NORMAL, NpcRank::NOVICE, 1));
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Stat calculation for MAXHP is not implemented");
	}
}

// ---- NpcData.init ------------------------------------------------------------------------------------------------------------------------------

const char* const NPCS_XML =
  R"(<npc_templates>)"
  R"(<npc_template npc_id="3" level="10" name_id="1" name="small malek" rank="NOVICE" rating="NORMAL" tribe="GENERAL"><stats maxHp="10"/>)"
  R"(<talk_info func_dialogs="2 999999"/></npc_template>)"
  R"(<npc_template npc_id="1" level="55" name_id="1" name="malek scout" rank="VETERAN" rating="ELITE" tribe="GENERAL">)"
  R"(<stats maxHp="10" attack="1000" mcrit="7"/></npc_template>)"
  R"(<npc_template npc_id="2" level="30" name_id="1" name="big malek" rank="NOVICE" rating="NORMAL" tribe="PET"><stats maxHp="10"/></npc_template>)"
  R"(</npc_templates>)";

TEST(NpcDataTest, InitIndexesAndFillsMissingStats) {
	LogCapture capture("com.aionemu.gameserver.dataholders.NpcData");
	std::unique_ptr<NpcData> data = bindXml<NpcData>(NPCS_XML);
	EXPECT_EQ(data->size(), 3);
	EXPECT_NE(capture.text().find("warning|Unknown dialog action 999999 for Npc 3"), std::string::npos) << capture.text();
	EXPECT_EQ(capture.text().find("action 2 "), std::string::npos) << "2 is DialogAction.BUY";
	EXPECT_TRUE(data->isFunctionDialog(2));
	EXPECT_TRUE(data->isFunctionDialog(999999)) << "Java adds the ids even when they are unknown";
	EXPECT_FALSE(data->isFunctionDialog(3));
	EXPECT_EQ(idsOf(data->getNpcData(), [](auto* npc) { return npc->getTemplateId(); }), (std::vector<int32_t>{1, 2, 3})) << "HashMap order";

	const model::templates::stats::StatsTemplate* novice = data->getNpcTemplate(3)->getStatsTemplate();
	EXPECT_EQ(novice->getAttack(), 62);
	EXPECT_EQ(novice->getAccuracy(), 370);
	EXPECT_EQ(novice->getMagicalAttack(), 80); // 200 * 0.4
	EXPECT_EQ(novice->getMacc(), 250);
	EXPECT_EQ(novice->getMresist(), 175); // 0.1 * 100 + 16.5 * 10
	EXPECT_EQ(novice->getMdef(), 50);
	EXPECT_EQ(novice->getMcrit(), 50);
	EXPECT_EQ(novice->getPcrit(), 10);
	EXPECT_EQ(novice->getPdef(), 170);
	EXPECT_EQ(novice->getParry(), 400);
	EXPECT_EQ(novice->getSpellResist(), 0) << "only from level 50";
	EXPECT_EQ(novice->getStrikeResist(), 0) << "only from level 50";
	EXPECT_EQ(novice->getStunLikeResistance(), 0);

	const model::templates::stats::StatsTemplate* elite = data->getNpcTemplate(1)->getStatsTemplate();
	EXPECT_EQ(elite->getAttack(), 1000) << "a stat of the XML stays";
	EXPECT_EQ(elite->getMcrit(), 7);
	EXPECT_EQ(elite->getPcrit(), 10);
	EXPECT_EQ(elite->getSpellResist(), 58);   // (55 - 50) * 1.1 * 8.5 * 1.25 = 58.44
	EXPECT_EQ(elite->getStrikeResist(), 203); // 5 * 2.5 * 9 * 1.8 = 202.5, ties up
	EXPECT_EQ(elite->getStunLikeResistance(), 700);

	const model::templates::stats::StatsTemplate* pet = data->getNpcTemplate(2)->getStatsTemplate();
	EXPECT_EQ(pet->getAttack(), 0) << "summons and siege weapons have fixed stats";
	EXPECT_EQ(pet->getMcrit(), 0);
	EXPECT_EQ(data->getNpcTemplate(4), nullptr);
}

// ---- GlobalDropData.processRules ---------------------------------------------------------------------------------------------------------------

TEST(GlobalDropDataTest, ProcessRulesReplacesMatchedNpcNames) {
	xml::LoadContext context;
	std::unique_ptr<ItemData> items = bindXml<ItemData>(context, R"(<item_templates><item_template id="182000001"/></item_templates>)");
	std::unique_ptr<NpcData> npcs = bindXml<NpcData>(context, NPCS_XML);
	std::unique_ptr<GlobalDropData> rules = bindXml<GlobalDropData>(
	  context,
	  R"(<global_rules>)"
	  R"(<gd_rule rule_name="names" chance="1"><gd_items><gd_item id="182000001"/></gd_items>)"
	  R"(<gd_npc_names><gd_npc_name value="Malek" function="START_WITH"/><gd_npc_name value="BIG MALEK" function="EQUALS"/></gd_npc_names>)"
	  R"(</gd_rule>)"
	  R"(<gd_rule rule_name="npcs and no match" chance="1"><gd_items><gd_item id="182000001"/></gd_items>)"
	  R"(<gd_npcs><gd_npc npc_id="999"/></gd_npcs><gd_npc_names><gd_npc_name value="nothing" function="CONTAINS"/></gd_npc_names></gd_rule>)"
	  R"(<gd_rule rule_name="no match" chance="1"><gd_items><gd_item id="182000001"/></gd_items>)"
	  R"(<gd_npc_names><gd_npc_name value="scout" function="END_WITH"/><gd_npc_name value="xyz" function="CONTAINS"/></gd_npc_names></gd_rule>)"
	  R"(<gd_rule rule_name="no names" chance="1"><gd_items><gd_item id="182000001"/></gd_items></gd_rule>)"
	  R"(</global_rules>)");
	rules->processRules(npcs->getNpcData());
	const auto& all = rules->getAllRules();
	ASSERT_EQ(all.size(), 4u);
	EXPECT_EQ(rules->size(), 4);
	auto npcIds = [](const model::templates::globaldrops::GlobalRule& rule) {
		return idsOf(rule.getGlobalRuleNpcs()->getGlobalDropNpcs(), [](const auto& npc) { return npc.getNpcId(); });
	};
	ASSERT_NE(all[0].getGlobalRuleNpcs(), nullptr);
	EXPECT_EQ(npcIds(all[0]), (std::vector<int32_t>{1, 2})) << "START_WITH matches 'malek scout', EQUALS ignores the case of 'big malek'";
	EXPECT_TRUE(all[0].getGlobalRuleNpcNames()->getGlobalDropNpcNames().empty()) << "the names are cleared";
	ASSERT_NE(all[1].getGlobalRuleNpcs(), nullptr);
	EXPECT_EQ(npcIds(all[1]), (std::vector<int32_t>{999})) << "the rule's own npcs are allowed although no name matched";
	EXPECT_TRUE(all[1].getGlobalRuleNpcNames()->getGlobalDropNpcNames().empty());
	ASSERT_NE(all[2].getGlobalRuleNpcs(), nullptr);
	EXPECT_EQ(npcIds(all[2]), (std::vector<int32_t>{1})) << "END_WITH lower-cases 'scout': 'malek scout'";
	EXPECT_EQ(all[3].getGlobalRuleNpcs(), nullptr) << "a rule without names is not processed";
	EXPECT_EQ(all[3].getGlobalRuleNpcNames(), nullptr);
}

TEST(GlobalDropDataTest, EndWithMatchesTheLowerCasedValue) {
	xml::LoadContext context;
	std::unique_ptr<ItemData> items = bindXml<ItemData>(context, R"(<item_templates><item_template id="182000001"/></item_templates>)");
	std::unique_ptr<NpcData> npcs = bindXml<NpcData>(context, NPCS_XML);
	std::unique_ptr<GlobalDropData> rules =
	  bindXml<GlobalDropData>(context, R"(<global_rules>)"
	                                   R"(<gd_rule rule_name="end" chance="1"><gd_items><gd_item id="182000001"/></gd_items>)"
	                                   R"(<gd_npc_names><gd_npc_name value="SCOUT" function="END_WITH"/></gd_npc_names></gd_rule>)"
	                                   R"(<gd_rule rule_name="none" chance="1"><gd_items><gd_item id="182000001"/></gd_items>)"
	                                   R"(<gd_npc_names><gd_npc_name value="xyz" function="CONTAINS"/></gd_npc_names></gd_rule>)"
	                                   R"(</global_rules>)");
	rules->processRules(npcs->getNpcData());
	const auto& all = rules->getAllRules();
	ASSERT_NE(all[0].getGlobalRuleNpcs(), nullptr);
	EXPECT_EQ(all[0].getGlobalRuleNpcs()->getGlobalDropNpcs().size(), 1u);
	EXPECT_EQ(all[1].getGlobalRuleNpcs(), nullptr) << "no npc matched and the rule had no npcs: unchanged";
	EXPECT_EQ(all[1].getGlobalRuleNpcNames()->getGlobalDropNpcNames().size(), 1u) << "the names stay";
}

// ---- ItemData, cleanup and decompose ids -------------------------------------------------------------------------------------------------------

TEST(ItemDataTest, ManastonesAndCleanup) {
	std::unique_ptr<ItemData> items =
	  bindXml<ItemData>(R"(<item_templates>)"
	                    R"(<item_template id="167000100" name="Manastone: Attack +1" item_group="MANASTONE" level="10" quality="COMMON" mask="4"/>)"
	                    R"(<item_template id="167000101" name="[Event] Manastone: HP +10" item_group="MANASTONE" level="10" quality="COMMON"/>)"
	                    R"(<item_template id="167000102" name="Manastone: Attack +5" item_group="MANASTONE" level="50" quality="RARE"/>)"
	                    R"(<item_template id="167000563" name="Manastone: Healing Boost +3" item_group="MANASTONE" level="60" quality="LEGEND"/>)"
	                    R"(<item_template id="167010001" name="Ancient Manastone: Attack +8" item_group="SPECIAL_MANASTONE" level="60"/>)"
	                    R"(<item_template id="167010002" name="Ancient Manastone: PvP Attack +8" item_group="SPECIAL_MANASTONE" level="60"/>)"
	                    R"(</item_templates>)");
	auto ids = [](const std::vector<const model::templates::item::ItemTemplate*>* list) {
		return list == nullptr ? std::vector<int32_t>{-1} : idsOf(*list, [](auto* item) { return item->getTemplateId(); });
	};
	EXPECT_EQ(ids(items->getManastones(10)), (std::vector<int32_t>{167000100})) << "names starting with '[' are skipped";
	EXPECT_EQ(ids(items->getManastones(20)), (std::vector<int32_t>{167000100})) << "attack stones of level 10 are also level 20 stones";
	EXPECT_EQ(ids(items->getManastones(50)), (std::vector<int32_t>{167000102}));
	EXPECT_EQ(ids(items->getManastones(60)), (std::vector<int32_t>{167000102, 167000563}));
	EXPECT_EQ(ids(items->getManastones(70)), (std::vector<int32_t>{167000102, 167000563}));
	EXPECT_EQ(items->getManastones(30), nullptr) << "Java: null";
	EXPECT_EQ(ids(items->getAncientManastones(60)), (std::vector<int32_t>{167010001})) << "PvP stones are skipped (lower case contains)";
	EXPECT_EQ(items->size(), 6);
	EXPECT_EQ(items->getItemTemplates().size(), 6u);

	std::unique_ptr<ItemRestrictionCleanupData> cleanup = bindXml<ItemRestrictionCleanupData>(
	  R"(<item_restriction_cleanups><cleanup id="167000100" trade="1" sell="0" awh="0"/></item_restriction_cleanups>)");
	EXPECT_EQ(cleanup->size(), 1);
	EXPECT_TRUE(cleanup->hasAccountOrLegionWhStorabilityDisabled(167000100));
	EXPECT_FALSE(cleanup->hasAccountOrLegionWhStorabilityDisabled(167000102));
	items->cleanup(*cleanup);
	EXPECT_EQ(items->getItemTemplate(167000100)->getMask(), 2) << "4 (SELLABLE) cleared, 2 (TRADEABLE) set, the -1 results change nothing";

	std::unique_ptr<ItemRestrictionCleanupData> unknown =
	  bindXml<ItemRestrictionCleanupData>(R"(<item_restriction_cleanups><cleanup id="1"/></item_restriction_cleanups>)");
	EXPECT_THROW(items->cleanup(*unknown), runtime::NullPointerException);
	EXPECT_EQ(ItemRestrictionCleanupData().size(), 0);
}

TEST(DecomposeActionIdsTest, EveryRandomRewardItemMustExist) {
	try {
		model::templates::item::actions::DecomposeAction::validateRandomItemIds(ItemData());
		FAIL() << "expected IllegalArgumentException";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Decomposable random reward item ID is invalid: 152000051");
	}
}

// The eleven tables validateRandomItemIds reads, in its order (DecomposeAction.java:404-410), copied from DecomposeAction.java: chunkEarth's
// ASMODIANS (:61-67) and ELYOS (:68-74) arrays, chunkSand's (:76-79, :80-83), then chunkRock (:44-45), chunkGemstone (:47), scrolls (:49),
// potion (:51), lesser_potions (:53), potion_50 (:55) and illusion_godstones (:57-58). Java iterates a HashMap<Race, int[]> and leaves the race
// order open; the C++ takes ASMODIANS first (DecomposeAction.cpp). premiumOphidanRecipe (:85-96) is not validated, in Java or C++.
const std::vector<std::vector<int32_t>> DECOMPOSE_RANDOM_REWARD_TABLES = {
  {152000051, 152000052, 152000053, 152000054, 152000055, 152000056, 152000057, 152000058, 152000059, 152000061, 152000062, 152000063,
    152000101, 152000102, 152000104, 152000107, 152000113, 152000201, 152000202, 152000204, 152000207, 152000214, 152000451, 152000453,
    152000455, 152000457, 152000459, 152000461, 152000463, 152000465, 152000468, 152000470, 152000551, 152000552, 152000553, 152000554,
    152000556, 152000651, 152000652, 152000653, 152000654, 152000656, 152000751, 152000752, 152000753, 152000754, 152000755, 152000756,
    152000757, 152000758, 152000759, 152000760, 152000762, 152000763, 152000851, 152000852, 152000853, 152000854, 152000855, 152000856,
    152000857, 152000858, 152000860, 152000861, 152001051, 152001052, 152001053, 152001055, 152001056},
  {152000001, 152000002, 152000003, 152000004, 152000005, 152000006, 152000007, 152000008, 152000009, 152000010, 152000011, 152000012,
    152000101, 152000102, 152000104, 152000107, 152000113, 152000201, 152000202, 152000204, 152000207, 152000214, 152000401, 152000403,
    152000405, 152000407, 152000409, 152000411, 152000413, 152000415, 152000417, 152000419, 152000501, 152000502, 152000503, 152000504,
    152000505, 152000601, 152000602, 152000603, 152000604, 152000605, 152000701, 152000702, 152000703, 152000704, 152000705, 152000706,
    152000707, 152000708, 152000709, 152000710, 152000711, 152000712, 152000801, 152000802, 152000803, 152000804, 152000805, 152000806,
    152000807, 152000808, 152000809, 152000810, 152001001, 152001002, 152001003, 152001004, 152001005},
  {152000452, 152000454, 152000301, 152000302, 152000303, 152000456, 152000458, 152000103, 152000203, 152000304, 152000305, 152000306,
    152000460, 152000462, 152000105, 152000205, 152000307, 152000309, 152000311, 152000464, 152000466, 152000108, 152000208, 152000313,
    152000315, 152000317, 152000469, 152000471, 152000114, 152000215, 152000320, 152000322, 152000324},
  {152000402, 152000404, 152000301, 152000302, 152000303, 152000406, 152000408, 152000103, 152000203, 152000304, 152000305, 152000306,
    152000410, 152000412, 152000105, 152000205, 152000307, 152000309, 152000311, 152000414, 152000416, 152000108, 152000208, 152000313,
    152000315, 152000317, 152000418, 152000420, 152000114, 152000215, 152000320, 152000322, 152000324},
  {152000104, 152000107, 152000113, 152000204, 152000207, 152000214, 152000307, 152000309, 152000311, 152000313, 152000315, 152000317,
    152000320, 152000322, 152000324},
  {152000112, 152000116, 152000212, 152000213, 152000217, 152000326, 152000327, 152000328},
  {164000073, 164000134, 164000076, 164000079, 164000122, 164000131, 164000118},
  {162000045, 162000079, 162000016, 162000021, 162000027, 162000023},
  {162000003, 162000008, 162000042, 162000022, 162000013, 162000018, 162000047},
  {162000075, 162000076, 162000077, 162000078, 162000079, 162000080, 162000081},
  {168000161, 168000162, 168000163, 168000164, 168000165, 168000166, 168000167, 168000168, 168000169, 168000170, 168000171, 168000172,
    168000173, 168000174, 168000175, 168000176, 168000177},
};

/** An item_templates document with one bare item_template per id */
std::string itemTemplatesXml(const std::set<int32_t>& ids) {
	std::string xml = "<item_templates>";
	for (int32_t id : ids)
		xml += R"(<item_template id=")" + std::to_string(id) + R"("/>)";
	return xml + "</item_templates>";
}

TEST(DecomposeActionIdsTest, EveryTableIsValidatedInTurn) {
	// With the ids of the tables before table k known, the first invalid id is the first id of table k that no earlier table holds (read off
	// the tables above). So dropping, reordering or swapping the races of any table read changes a message. chunkRock is the exception: its 15
	// ids are all in chunkEarth or chunkSand, so its check can never fail first, and with tables 0-3 known the message names chunkGemstone's first
	// id.
	const std::vector<int32_t> firstNewIds = {152000051, 152000001, 152000452, 152000402, 152000112, 152000112, 164000073, 162000045, 162000003,
		162000075, 168000161};
	ASSERT_EQ(firstNewIds.size(), DECOMPOSE_RANDOM_REWARD_TABLES.size());
	std::set<int32_t> known;
	for (size_t table = 0; table < DECOMPOSE_RANDOM_REWARD_TABLES.size(); ++table) {
		std::unique_ptr<ItemData> items = bindXml<ItemData>(itemTemplatesXml(known));
		try {
			model::templates::item::actions::DecomposeAction::validateRandomItemIds(*items);
			ADD_FAILURE() << "table " << table << ": expected IllegalArgumentException";
		} catch (const runtime::IllegalArgumentException& e) {
			EXPECT_EQ(std::string(e.what()), "Decomposable random reward item ID is invalid: " + std::to_string(firstNewIds[table]))
			  << "table " << table;
		}
		known.insert(DECOMPOSE_RANDOM_REWARD_TABLES[table].begin(), DECOMPOSE_RANDOM_REWARD_TABLES[table].end());
	}
	EXPECT_EQ(known.size(), 222u) << "the distinct ids of the 271 in the eleven tables";
	std::unique_ptr<ItemData> all = bindXml<ItemData>(itemTemplatesXml(known));
	EXPECT_NO_THROW(model::templates::item::actions::DecomposeAction::validateRandomItemIds(*all))
	  << "every table's ids known; premiumOphidanRecipe's are not, and it is not validated";
}

// ---- NpcShoutData ------------------------------------------------------------------------------------------------------------------------------

TEST(NpcShoutDataTest, ShoutsByWorldAndNpc) {
	using model::templates::npcshout::ShoutEventType;
	std::unique_ptr<NpcShoutData> data = bindXml<NpcShoutData>(
	  R"(<npc_shouts>)"
	  R"(<shout_group client_ai="a"><shout_npcs npc_ids="10 11"><shout string_id="1" when="SEE"/><shout string_id="2" when="DIED" pattern="p"/>)"
	  R"(</shout_npcs><shout_npcs npc_ids="10" restrict_world="300"><shout string_id="3" when="SEE"/></shout_npcs></shout_group>)"
	  R"(<shout_group client_ai="b"><shout_npcs npc_ids="10"><shout string_id="4" when="SEE" skill_no="7"/></shout_npcs></shout_group>)"
	  R"(</npc_shouts>)");
	auto ids = [](const std::optional<std::vector<const model::templates::npcshout::NpcShout*>>& shouts) {
		return !shouts ? std::vector<int32_t>{-1} : idsOf(*shouts, [](auto* shout) { return shout->getStringId(); });
	};
	EXPECT_EQ(data->size(), 4) << "the shouts of every list";
	EXPECT_EQ(ids(data->getNpcShouts(300, 10)), (std::vector<int32_t>{1, 2, 4, 3})) << "global shouts, then the world's";
	EXPECT_EQ(ids(data->getNpcShouts(0, 10)), (std::vector<int32_t>{1, 2, 4, 1, 2, 4})) << "world 0 is the global map: Java adds it twice";
	EXPECT_EQ(ids(data->getNpcShouts(0, 11)), (std::vector<int32_t>{1, 2, 1, 2}));
	EXPECT_EQ(ids(data->getNpcShouts(5, 99)), (std::vector<int32_t>{})) << "an existing global map gives an empty list, not null";
	EXPECT_EQ(ids(data->getNpcShouts(300, 10, ShoutEventType::SEE)), (std::vector<int32_t>{1, 4, 3}));
	EXPECT_EQ(ids(data->getNpcShouts(300, 10, std::nullopt)), (std::vector<int32_t>{1, 2, 4, 3})) << "a null type does not filter";
	EXPECT_EQ(ids(data->getNpcShouts(300, 10, ShoutEventType::DIED, std::string_view("p"), 0)), (std::vector<int32_t>{2}));
	EXPECT_EQ(ids(data->getNpcShouts(300, 10, ShoutEventType::SEE, std::nullopt, 7)), (std::vector<int32_t>{4}));
	EXPECT_EQ(ids(NpcShoutData().getNpcShouts(0, 10)), (std::vector<int32_t>{-1})) << "no map at all: null";
}

// ---- lookups and the load failures of hooks ----------------------------------------------------------------------------------------------------

TEST(QuestsDataTest, QuestTemplatesInJavaHashMapOrderAndFactionQuests) {
	std::unique_ptr<QuestsData> data = bindXml<QuestsData>(
	  R"(<quests>)"
	  R"(<quest id="17" name="a" npcfaction_id="5"/><quest id="1" name="b" npcfaction_id="5"/><quest id="33" name="c"/><quest id="2" name="d"/>)"
	  R"(</quests>)");
	// table of 16: bucket 1 holds 17, 1, 33 in put order; bucket 2 holds 2
	EXPECT_EQ(idsOf(data->getQuestTemplates(), [](auto* quest) { return quest->getId(); }), (std::vector<int32_t>{17, 1, 33, 2}));
	EXPECT_EQ(data->size(), 4);
	EXPECT_EQ(data->getQuestById(33)->getName(), "c");
	EXPECT_EQ(data->getQuestById(3), nullptr);
}

TEST(TribeRelationsDataTest, RelationsUseTheBaseTribes) {
	using model::TribeClass;
	std::unique_ptr<TribeRelationsData> data =
	  bindXml<TribeRelationsData>(R"(<tribe_relations>)"
	                              R"(<tribe name="AB1_DOORKILLER" base="NONE"><aggro>AB1_DOOR_DA</aggro></tribe>)"
	                              R"(<tribe name="AB1_DOOR_DA" base="GENERAL_DARK"/>)"
	                              R"(<tribe name="GENERAL_DARK"><support>GENERAL</support></tribe>)"
	                              R"(<tribe name="GENERAL"/>)"
	                              R"(</tribe_relations>)");
	EXPECT_EQ(data->size(), 4);
	EXPECT_EQ(data->getBaseTribe(TribeClass::AB1_DOOR_DA), TribeClass::GENERAL_DARK);
	EXPECT_EQ(data->getBaseTribe(TribeClass::AB1_DOORKILLER), TribeClass::AB1_DOORKILLER) << "base NONE is the tribe itself (Tribe.getBase)";
	EXPECT_THROW(static_cast<void>(data->getBaseTribe(TribeClass::PET)), runtime::NullPointerException);
	EXPECT_TRUE(data->isAggressiveRelation(TribeClass::AB1_DOORKILLER, TribeClass::AB1_DOOR_DA));
	EXPECT_TRUE(data->isAggressiveRelation(TribeClass::AB1_DOOR_DA, TribeClass::AB1_DOORKILLER)) << "symmetric";
	EXPECT_FALSE(data->isAggressiveRelation(TribeClass::AB1_DOORKILLER, TribeClass::PET)) << "an unknown tribe has no relation";
	EXPECT_TRUE(data->isSupportRelation(TribeClass::GENERAL_DARK, TribeClass::GENERAL));
	EXPECT_TRUE(data->isSupportRelation(TribeClass::GENERAL, TribeClass::GENERAL_DARK)) << "symmetric";
	EXPECT_FALSE(data->isSupportRelation(TribeClass::GENERAL, TribeClass::AB1_DOOR_DA)) << "only the tribes' own support lists count";
	EXPECT_TRUE(data->canSupport(TribeClass::GENERAL_DARK, TribeClass::GENERAL));
	EXPECT_FALSE(data->canSupport(TribeClass::GENERAL, TribeClass::GENERAL_DARK));
}

TEST(PlayerExperienceTableTest, LevelsAndExperience) {
	std::unique_ptr<PlayerExperienceTable> table = bindXml<PlayerExperienceTable>(
	  R"(<player_experience_table><exp>0</exp><exp>400</exp><exp>1500</exp><exp>9000000000</exp></player_experience_table>)");
	EXPECT_EQ(table->getMaxLevel(), 4);
	EXPECT_EQ(table->getStartExpForLevel(0), 0);
	EXPECT_EQ(table->getStartExpForLevel(2), 400);
	EXPECT_EQ(table->getStartExpForLevel(4), 9000000000LL);
	EXPECT_THROW(static_cast<void>(table->getStartExpForLevel(5)), runtime::IllegalArgumentException);
	EXPECT_EQ(table->getLevelForExp(0), 1);
	EXPECT_EQ(table->getLevelForExp(399), 1);
	EXPECT_EQ(table->getLevelForExp(400), 2);
	EXPECT_EQ(table->getLevelForExp(9000000000LL), 3) << "the max level is capped at getMaxLevel() - 1";
	EXPECT_EQ(PlayerExperienceTable().getMaxLevel(), 0);
}

TEST(InstanceCooltimeDataTest, LinkedOrderAndLookups) {
	std::unique_ptr<InstanceCooltimeData> data = bindXml<InstanceCooltimeData>(
	  R"(<instance_cooltimes>)"
	  R"(<instance_cooltime id="1" worldId="300" race="PC_ALL" sync_id="7"><max_member_light>6</max_member_light><max_member_dark>3</max_member_dark></instance_cooltime>)"
	  R"(<instance_cooltime id="2" worldId="100" race="PC_ALL" sync_id="8"/>)"
	  R"(<instance_cooltime id="3" worldId="300" race="PC_ALL" sync_id="9"><max_member_light>12</max_member_light></instance_cooltime>)"
	  R"(</instance_cooltimes>)");
	std::vector<int32_t> worlds;
	for (const auto& [worldId, cooltime] : data->getInstanceCooltimes())
		worlds.push_back(worldId);
	EXPECT_EQ(worlds, (std::vector<int32_t>{300, 100})) << "LinkedHashMap: a repeated key keeps its position";
	EXPECT_EQ(data->size(), 2);
	EXPECT_EQ(data->getInstanceCooltimeByWorldId(300)->getId(), 3) << "and gets the new value";
	EXPECT_EQ(data->getMaxMemberCount(300, model::Race::ELYOS), 12);
	EXPECT_EQ(data->getMaxMemberCount(300, model::Race::ASMODIANS), 0);
	EXPECT_EQ(data->getMaxMemberCount(999, model::Race::ELYOS), 0);
	EXPECT_EQ(data->getWorldId(7), 300);
	EXPECT_EQ(data->getWorldId(8), 100);
	EXPECT_EQ(data->getWorldId(1), 0);
	EXPECT_THROW(static_cast<void>(data->getInstanceMaxCountByWorldId(999)), runtime::NullPointerException);
}

TEST(WorldMapsDataTest, LinkedHashMapIterationAndCNames) {
	std::unique_ptr<WorldMapsData> data =
	  bindXml<WorldMapsData>(R"(<world_maps>)"
	                         R"(<map id="210010000" cName="Poeta" death_level="0" water_level="0" flags="RECALL"/>)"
	                         R"(<map id="110010000" cName="Sanctum" death_level="0" water_level="0" flags="RECALL"/>)"
	                         R"(<map id="210010000" cName="PoetaAgain" death_level="0" water_level="0" flags="RECALL"/>)"
	                         R"(</world_maps>)");
	std::vector<std::string> names;
	for (const model::templates::world::WorldMapTemplate* map : *data)
		names.push_back(map->getCName());
	EXPECT_EQ(names, (std::vector<std::string>{"PoetaAgain", "Sanctum"}));
	EXPECT_EQ(data->size(), 2);
	EXPECT_EQ(data->getWorldIdByCName("sanctum"), 110010000) << "equalsIgnoreCase";
	EXPECT_EQ(data->getWorldIdByCName("Poeta"), 0);
}

TEST(WalkerDataTest, FirstRouteIdWins) {
	LogCapture capture("com.aionemu.gameserver.dataholders.WalkerData");
	std::unique_ptr<WalkerData> data = bindXml<WalkerData>(R"(<npc_walker>)"
	                                                       R"(<walker_template route_id="R1" pool="1"><routestep x="1" y="1" z="1"/></walker_template>)"
	                                                       R"(<walker_template route_id="R2" pool="2"><routestep x="1" y="1" z="1"/></walker_template>)"
	                                                       R"(<walker_template route_id="R1" pool="3"><routestep x="1" y="1" z="1"/></walker_template>)"
	                                                       R"(</npc_walker>)");
	EXPECT_EQ(data->size(), 2);
	EXPECT_EQ(data->getWalkerTemplate("R1")->getPool(), 1) << "putIfAbsent";
	EXPECT_EQ(data->getWalkerTemplate("R3"), nullptr);
	EXPECT_NE(capture.text().find("warning|Duplicate route ID: R1"), std::string::npos) << capture.text();
	std::vector<std::string> routes;
	for (const model::templates::walker::WalkerTemplate* route : data->getTemplates())
		routes.push_back(route->getRouteId());
	EXPECT_EQ(routes, (std::vector<std::string>{"R1", "R2"}));
}

TEST(CustomDropTest, DuplicateNpcDropWarns) {
	LogCapture capture("com.aionemu.gameserver.dataholders.CustomDrop");
	std::unique_ptr<CustomDrop> data =
	  bindXml<CustomDrop>(R"(<custom_drop><npc_drop npc_id="5"/><npc_drop npc_id="6"/><npc_drop npc_id="5"/></custom_drop>)");
	EXPECT_EQ(data->size(), 2);
	EXPECT_NE(data->getNpcDrop(5), nullptr);
	EXPECT_NE(capture.text().find("warning|Tried to set custom drop for npc 5 twice!"), std::string::npos) << capture.text();
}

TEST(StaticDoorDataTest, DuplicateWorldFailsTheLoad) {
	std::unique_ptr<StaticDoorData> data =
	  bindXml<StaticDoorData>(R"(<staticdoor_templates>)"
	                          R"(<world world="300040000"><staticdoor id="33" x="1" y="2" z="3"/><staticdoor id="34" x="1" y="2" z="3"/></world>)"
	                          R"(</staticdoor_templates>)");
	EXPECT_EQ(data->size(), 1);
	EXPECT_EQ(data->getStaticDoors(300040000).size(), 2u);
	EXPECT_TRUE(data->getStaticDoors(1).empty());
	ASSERT_NE(data->getStaticDoor(300040000, 34), nullptr);
	EXPECT_EQ(data->getStaticDoor(300040000, 35), nullptr);
	EXPECT_EQ(data->getStaticDoor(1, 34), nullptr);
	try {
		static_cast<void>(bindXml<StaticDoorData>(R"(<staticdoor_templates><world world="1"/><world world="1"/></staticdoor_templates>)"));
		FAIL() << "expected the duplicate world to fail the load";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("Duplicate static door world 1"), std::string::npos) << e.what();
	}
}

TEST(GlobalNpcExclusionDataTest, EmptyOnlyWithoutAnyList) {
	EXPECT_TRUE(bindXml<GlobalNpcExclusionData>(R"(<global_npc_exclusions/>)")->isEmpty());
	std::unique_ptr<GlobalNpcExclusionData> data =
	  bindXml<GlobalNpcExclusionData>(R"(<global_npc_exclusions><npc_ids>1 2</npc_ids></global_npc_exclusions>)");
	EXPECT_FALSE(data->isEmpty());
	EXPECT_EQ(data->getNpcIds(), (std::unordered_set<int32_t>{1, 2}));
	EXPECT_TRUE(data->getNpcNames().empty()) << "Collections.emptySet()";
}

TEST(ChallengeDataTest, TaskAndQuestSearches) {
	std::unique_ptr<ChallengeData> data = bindXml<ChallengeData>(
	  R"(<challenge_tasks>)"
	  R"(<task id="17" type="LEGION" race="PC_ALL" min_level="1" max_level="65"><quest id="100" repeat_count="1" score="1"/><reward type="NONE"/></task>)"
	  R"(<task id="1" type="LEGION" race="PC_ALL" min_level="1" max_level="65"><quest id="100" repeat_count="1" score="2"/><reward type="NONE"/></task>)"
	  R"(</challenge_tasks>)");
	EXPECT_EQ(data->size(), 2);
	ASSERT_NE(data->getTaskByQuestId(100), nullptr);
	EXPECT_EQ(data->getTaskByQuestId(100)->getId(), 17) << "HashMap order: 17 and 1 share bucket 1, in put order";
	EXPECT_EQ(data->getQuestByQuestId(100)->getScore(), 1);
	EXPECT_EQ(data->getTaskByQuestId(101), nullptr);
	EXPECT_EQ(data->getTaskByTaskId(1)->getId(), 1);
	std::vector<int32_t> taskIds;
	for (const auto& [id, task] : data->getTasks())
		taskIds.push_back(task->getId());
	EXPECT_EQ(taskIds, (std::vector<int32_t>{17, 1})) << "getTasks iterates like Java's HashMap (ChallengeTaskService builds the client list)";
	EXPECT_EQ(*data->getTasks().get(1), data->getTaskByTaskId(1));
}

TEST(HolderMapOrderTest, MapGettersIterateInJavaHashMapOrder) {
	// put order 2, 17, 1: Java's HashMap iterates bucket 1 (17, then 1 in put order) before bucket 2
	const std::vector<int32_t> javaOrder = {17, 1, 2};
	{
		std::unique_ptr<TradeListData> data = bindXml<TradeListData>(
		  R"(<npc_trade_list><tradelist_template npc_id="2"/><tradelist_template npc_id="17"/><tradelist_template npc_id="1"/></npc_trade_list>)");
		std::vector<int32_t> ids;
		for (const auto& [npcId, list] : data->getTradeListTemplate())
			ids.push_back(list->getNpcId());
		EXPECT_EQ(ids, javaOrder) << "TradeListData.getTradeListTemplate()";
		EXPECT_EQ(data->getTradeListTemplate(17)->getNpcId(), 17);
		EXPECT_EQ(data->getTradeListTemplate(3), nullptr);
	}
	{
		std::unique_ptr<WorldRaidData> data = bindXml<WorldRaidData>(
		  R"(<world_raid_locations>)"
		  R"(<world_raid_location location_id="2" map_id="210030000" x="1" y="1" z="1" h="0"><world_raid_npcs/><location_markers/></world_raid_location>)"
		  R"(<world_raid_location location_id="17" map_id="210030000" x="2" y="2" z="2" h="0"><world_raid_npcs/><location_markers/></world_raid_location>)"
		  R"(<world_raid_location location_id="1" map_id="210030000" x="3" y="3" z="3" h="0"><world_raid_npcs/><location_markers/></world_raid_location>)"
		  R"(<world_raid_location location_id="17" map_id="210040000" x="4" y="4" z="4" h="0"><world_raid_npcs/><location_markers/></world_raid_location>)"
		  R"(</world_raid_locations>)");
		std::vector<int32_t> ids;
		for (const auto& [locationId, location] : data->getLocations())
			ids.push_back(location->getLocationId());
		EXPECT_EQ(ids, javaOrder) << "WorldRaidData.getLocations";
		EXPECT_EQ(data->getLocationsById(17)->getMapId(), 210030000) << "putIfAbsent: the first location 17 stays";
		EXPECT_EQ(data->size(), 3);
	}
	{
		std::unique_ptr<AtreianPassportData> data = bindXml<AtreianPassportData>(
		  R"(<login_events>)"
		  R"(<login_event id="2" active="1" period_start="2014-03-01T00:00:00" period_end="2014-05-01T00:00:00" attend_type="DAILY" attend_num="1" reward_item="1" reward_item_num="1"/>)"
		  R"(<login_event id="17" active="1" period_start="2014-03-01T00:00:00" period_end="2014-05-01T00:00:00" attend_type="DAILY" attend_num="2" reward_item="1" reward_item_num="1"/>)"
		  R"(<login_event id="1" active="1" period_start="2014-03-01T00:00:00" period_end="2014-05-01T00:00:00" attend_type="DAILY" attend_num="3" reward_item="1" reward_item_num="1"/>)"
		  R"(</login_events>)");
		std::vector<int32_t> ids;
		for (const auto& [id, passport] : data->getAll())
			ids.push_back(passport->getId());
		EXPECT_EQ(ids, javaOrder) << "AtreianPassportData.getAll (AtreianPassportService.onLogin adds passports in this order)";
		EXPECT_EQ(data->getAtreianPassportId(17)->getAttendNum(), 2);
	}
	{
		// AutoGroupData: recruitableInstanceMaskIdByPortalNpc is filled by computeIfAbsent, which inserts at the head of a bucket: portal npc 17
		// (mask 302) is added before npc 1 (mask 303), so Java iterates 1, 17 and getRecruitableInstanceMaskIds() is [303, 302]
		std::unique_ptr<AutoGroupData> data = bindXml<AutoGroupData>(
		  R"(<auto_groups>)"
		  R"(<auto_group id="302" instanceId="300110000" name_id="1" title_id="1" min_lvl="1" max_lvl="65" npc_ids="17"/>)"
		  R"(<auto_group id="303" instanceId="300110000" name_id="1" title_id="1" min_lvl="1" max_lvl="65" npc_ids="1"/>)"
		  R"(</auto_groups>)");
		EXPECT_EQ(data->getRecruitableInstanceMaskIds(), (std::vector<int32_t>{303, 302}));
	}
}

// ---- holders that create run-time objects ------------------------------------------------------------------------------------------------------

TEST(SpawnsDataTest, CustomSpawnsReplaceTheNpcsGroups) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::unique_ptr<SpawnsData> data =
	  bindXml<SpawnsData>(R"(<spawns>)"
	                      R"(<spawn_map map_id="210010000">)"
	                      R"(<spawn npc_id="1" respawn_time="10"><spot x="1" y="1" z="1"/></spawn>)"
	                      R"(<spawn npc_id="2" respawn_time="10"><spot x="2" y="2" z="2"/><spot x="3" y="3" z="3"/></spawn>)"
	                      R"(<spawn npc_id="1" respawn_time="20"><spot x="4" y="4" z="4"/></spawn>)"
	                      R"(</spawn_map>)"
	                      R"(<spawn_map map_id="220010000"><spawn npc_id="9" respawn_time="10"><spot x="1" y="1" z="1"/></spawn></spawn_map>)"
	                      R"(<spawn_map map_id="210010000">)"
	                      R"(<spawn npc_id="1" respawn_time="30" custom="true"><spot x="5" y="5" z="5"/></spawn>)"
	                      R"(<spawn npc_id="1" respawn_time="40"><spot x="6" y="6" z="6"/></spawn>)"
	                      R"(</spawn_map>)"
	                      R"(</spawns>)");
	EXPECT_EQ(data->size(), 2) << "spawn maps by map id";
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> npc1 = data->getSpawnsForNpc(210010000, 1);
	ASSERT_EQ(npc1.size(), 1u) << "the custom spawn removed the two groups of npc 1 and later spawns of npc 1 in the same map are skipped";
	EXPECT_EQ(npc1[0]->getRespawnTime(), 30);
	EXPECT_EQ(data->getSpawnsForNpc(210010000, 2).size(), 1u);
	EXPECT_EQ(data->getSpawnsForNpc(210010000, 2)[0]->getSpawnTemplates().size(), 2) << "one template per spot";
	EXPECT_TRUE(data->getSpawnsForNpc(210010000, 9).empty());
	EXPECT_TRUE(data->getSpawnsForNpc(1, 1).empty());
	EXPECT_EQ(data->getSpawnsByWorldId(210010000).size(), 2u);
	EXPECT_EQ(data->getBaseSpawnsByLocId(1), nullptr);
	std::unordered_set<int32_t> npcIds;
	data->addAllNpcIdsToSet(npcIds);
	EXPECT_EQ(npcIds, (std::unordered_set<int32_t>{1, 2, 9}));
	std::optional<model::templates::spawns::SpawnSearchResult> first = data->getFirstSpawnByNpcId(210010000, 2);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->getWorldId(), 210010000);
	EXPECT_FLOAT_EQ(first->getSpot().getX(), 2.0f);
}

TEST(SpawnsDataTest, RegularSpawnsChangeWhileOtherThreadsReadThem) {
	// Java race (D6): Event start (addRegularSpawns) changes the npc HashMap and the ArrayLists of allSpawnMaps inside ConcurrentHashMap.compute
	// while SpawnEngine and handlers read them without a lock (getSpawnsForNpc, getSpawnsByWorldId); C++ guards them with the collection shims
	// and returns snapshots, so readers see a consistent, growing number of groups
	std::unique_ptr<SpawnsData> data;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		data = bindXml<SpawnsData>(R"(<spawns><spawn_map map_id="1"><spawn npc_id="1" respawn_time="10"><spot x="1" y="1" z="1"/></spawn>)"
		                           R"(<spawn npc_id="2" respawn_time="10"><spot x="2" y="2" z="2"/></spawn></spawn_map></spawns>)");
	}
	const model::templates::spawns::SpawnMap& map = data->getTemplates().at(0);
	const int iterations = 300;
	std::atomic<bool> writerDone{false};
	std::atomic<int> readerFailures{0};
	std::vector<std::thread> readers;
	for (int t = 0; t < 2; ++t) {
		readers.emplace_back([&] {
			size_t lastNpc1 = 0;
			while (!writerDone.load()) {
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				size_t npc1 = data->getSpawnsForNpc(1, 1).size();
				size_t all = data->getSpawnsByWorldId(1).size();
				if (npc1 < lastNpc1 || all < npc1)
					readerFailures.fetch_add(1);
				lastNpc1 = npc1;
			}
		});
	}
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		for (int i = 0; i < iterations; ++i)
			data->addRegularSpawns(map);
	}
	writerDone.store(true);
	for (std::thread& reader : readers)
		reader.join();
	EXPECT_EQ(readerFailures.load(), 0);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_EQ(data->getSpawnsForNpc(1, 1).size(), static_cast<size_t>(iterations + 1));
	EXPECT_EQ(data->getSpawnsByWorldId(1).size(), static_cast<size_t>(2 * (iterations + 1)));
	model::templates::event::EventTemplate otherEvent;
	data->removeEventSpawnObjects(otherEvent);
	EXPECT_EQ(data->getSpawnsByWorldId(1).size(), static_cast<size_t>(2 * (iterations + 1))) << "no group belongs to the event";
}

TEST(ZoneDataTest, AreasAndWeatherZoneIds) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::unique_ptr<ZoneData> data = bindXml<ZoneData>(
	  R"(<zones>)"
	  R"(<zone mapid="1" name="P4_09_W1" area_type="SPHERE" zone_type="WEATHER"><sphere x="1" y="1" z="1" r="5"/></zone>)"
	  R"(<zone mapid="1" name="P4_09_EMPTY" area_type="SPHERE" zone_type="WEATHER"><sphere x="1" y="1" z="1" r="0"/></zone>)"
	  R"(<zone mapid="1" name="P4_09_W2" area_type="CYLINDER" zone_type="WEATHER"><cylinder x="1" y="1" r="5" bottom="0" top="10"/></zone>)"
	  R"(<zone mapid="2" name="P4_09_W3" area_type="SEMISPHERE" zone_type="WEATHER"><semisphere x="1" y="1" z="1" r="5"/></zone>)"
	  R"(<zone mapid="2" name="P4_09_SUB" area_type="POLYGON" zone_type="SUB"><points bottom="0" top="10">)"
	  R"(<point x="0" y="0"/><point x="10" y="0"/><point x="10" y="10"/></points></zone>)"
	  R"(</zones>)");
	EXPECT_EQ(data->size(), 4) << "a sphere with r <= 0 creates no area";
	ASSERT_EQ(data->getZones().at(1).size(), 2u);
	ASSERT_EQ(data->getZones().at(2).size(), 2u);
	const model::templates::zone::ZoneTemplate* w2 = data->getZones().at(1)[1]->getZoneTemplate();
	const model::templates::zone::ZoneTemplate* w3 = data->getZones().at(2)[0]->getZoneTemplate();
	const model::templates::zone::ZoneTemplate* sub = data->getZones().at(2)[1]->getZoneTemplate();
	EXPECT_EQ(data->getWeatherZoneId(*data->getZones().at(1)[0]->getZoneTemplate()), 1);
	EXPECT_EQ(data->getWeatherZoneId(*w2), 2) << "the empty sphere takes no id";
	EXPECT_EQ(data->getWeatherZoneId(*w3), 1) << "numbering restarts per map";
	EXPECT_EQ(data->getWeatherZoneId(*sub), 0);
}

// ---- TradeListData.validateBuyLists and DataManager helpers ------------------------------------------------------------------------------------

TEST(TradeListDataTest, ValidateBuyListsLogsMissingListsSorted) {
	std::unique_ptr<NpcData> npcs = bindXml<NpcData>(
	  R"(<npc_templates>)"
	  R"(<npc_template npc_id="30" level="1" name_id="1" rank="NOVICE" rating="NORMAL" tribe="PET"><stats/><talk_info func_dialogs="2 78"/></npc_template>)"
	  R"(<npc_template npc_id="20" level="1" name_id="1" rank="NOVICE" rating="NORMAL" tribe="PET"><stats/><talk_info func_dialogs="2"/></npc_template>)"
	  R"(<npc_template npc_id="10" level="1" name_id="1" rank="NOVICE" rating="NORMAL" tribe="PET"><stats/><talk_info func_dialogs="2"/></npc_template>)"
	  R"(</npc_templates>)");
	std::unique_ptr<TradeListData> lists =
	  bindXml<TradeListData>(R"(<npc_trade_list><tradelist_template npc_id="20"/><purchase_template npc_id="30"/></npc_trade_list>)");
	LogCapture capture("com.aionemu.gameserver.dataholders.TradeListData");
	lists->validateBuyLists(npcs->getNpcData());
	EXPECT_NE(capture.text().find("warning|Missing trade lists for these npcs: [10, 30]"), std::string::npos) << capture.text();
	EXPECT_NE(capture.text().find("warning|Missing trade-in lists for these npcs: [30]"), std::string::npos) << capture.text();
	EXPECT_EQ(lists->size(), 1);
	EXPECT_NE(lists->getPurchaseTemplate(30), nullptr);
}

TEST(DataManagerTest, FormatSecondsLikeJavaFormatter) {
	EXPECT_EQ(DataManager::formatSeconds(12.25f), "12.3") << "HALF_UP";
	EXPECT_EQ(DataManager::formatSeconds(12.24f), "12.2");
	EXPECT_EQ(DataManager::formatSeconds(0.0f), "0.0");
	EXPECT_EQ(DataManager::formatSeconds(9.95f), "9.9") << "9.95f is 9.94999980926513671875";
	EXPECT_EQ(DataManager::formatSeconds(9.96f), "10.0");
	EXPECT_EQ(DataManager::formatSeconds(3.0f), "3.0");
}

} // namespace
} // namespace aion::gameserver::dataholders
