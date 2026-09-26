// P4-08 (the lease over the questEngine xmlgen shells): the data-only helpers of the XML quest models, bound from fixture XML. Expectations are
// derived by hand from Monster.java, QuestEvent.java, OnKillEvent.java, QuestOperations.java and the getAlternativeNpcs overrides of the XMLQuest
// subclasses (ReportToData.java, ReportOnLevelUpData.java, FountainRewardsData.java, ReportToManyData.java, XmlQuestData.java,
// MonsterHuntData.java, KillSpawnedData.java). The models have no afterUnmarshal hooks. Test double: QuestsData bound from XML text and published
// into DataManager for the quest_kill lookup of MonsterHuntData.

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/questEngine/handlers/models/FountainRewardsData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/KillSpawnedData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.bind.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/models/MonsterHuntData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ReportOnLevelUpData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ReportToData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/ReportToManyData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::handlers::models {
namespace {

template <class T>
std::unique_ptr<T> bindXml(const std::string& text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

TEST(MonsterTest, AddNpcIdsAppendsNewIdsInOrder) {
	Monster monster; // Java: MonsterHuntData.register builds `new Monster()` and fills it
	EXPECT_FALSE(monster.getNpcIds().has_value()) << "Java null";
	monster.addNpcIds({});
	ASSERT_TRUE(monster.getNpcIds().has_value()) << "the list is created even for no ids";
	EXPECT_TRUE(monster.getNpcIds()->empty());
	monster.addNpcIds({210001, 210002, 210001});
	monster.addNpcIds({210003, 210002});
	EXPECT_EQ(*monster.getNpcIds(), (std::vector<int32_t>{210001, 210002, 210003})) << "contains() skips duplicates, also within one call";
	monster.setEndVar(5);
	monster.setVar(2);
	monster.setStep(1);
	EXPECT_EQ(monster.getEndVar(), 5);
	EXPECT_EQ(monster.getVar(), 2);
	EXPECT_EQ(monster.getStep(), 1);

	std::unique_ptr<Monster> bound = bindXml<Monster>(R"(<monster var="1" end_var="3" npc_ids="700 701"/>)");
	bound->addNpcIds({701, 702});
	EXPECT_EQ(*bound->getNpcIds(), (std::vector<int32_t>{700, 701, 702}));
}

TEST(QuestEventTest, IdsAndMonsters) {
	std::unique_ptr<xmlQuest::events::OnTalkEvent> talk = bindXml<xmlQuest::events::OnTalkEvent>(R"(<on_talk_event ids="700001 700002"/>)");
	EXPECT_EQ(talk->getIds(), (std::vector<int32_t>{700001, 700002}));
	EXPECT_TRUE(bindXml<xmlQuest::events::OnTalkEvent>("<on_talk_event/>")->getIds().empty()) << "Java: a new empty list";

	std::unique_ptr<xmlQuest::events::OnKillEvent> kill = bindXml<xmlQuest::events::OnKillEvent>(
		R"(<on_kill_event ids="1"><monster var="0" end_var="2" npc_ids="210001"/><monster var="1" end_var="1" npc_ids="210002 210003"/></on_kill_event>)");
	ASSERT_EQ(kill->getMonsters().size(), 2u);
	EXPECT_EQ(kill->getMonsters()[1].getVar(), 1);
	EXPECT_EQ(*kill->getMonsters()[1].getNpcIds(), (std::vector<int32_t>{210002, 210003}));
	EXPECT_EQ(kill->getIds(), (std::vector<int32_t>{1})) << "inherited from QuestEvent";
	EXPECT_TRUE(bindXml<xmlQuest::events::OnKillEvent>("<on_kill_event/>")->getMonsters().empty()) << "Java: a new empty list";
}

TEST(QuestOperationsTest, OverrideDefaultsToTrue) {
	EXPECT_TRUE(bindXml<xmlQuest::operations::QuestOperations>("<operations/>")->isOverride()) << "Java: null Boolean";
	EXPECT_TRUE(bindXml<xmlQuest::operations::QuestOperations>(R"(<operations override="true"/>)")->isOverride());
	EXPECT_FALSE(bindXml<xmlQuest::operations::QuestOperations>(R"(<operations override="false"/>)")->isOverride());
}

// ---- XMLQuest.getAlternativeNpcs ---------------------------------------------------------------------------------------------------------------

using NpcIds = std::optional<std::unordered_set<int32_t>>;

NpcIds ids(std::initializer_list<int32_t> values) {
	return std::unordered_set<int32_t>(values);
}

TEST(XmlQuestAlternativeNpcsTest, StartAndEndNpcLists) {
	// Java: `list != null && list.size() > 1 && list.contains(npcId)` -> the other ids of that list, start before end, else null
	std::unique_ptr<ReportToData> report = bindXml<ReportToData>(R"(<report_to id="1" start_npc_ids="10 11 12" end_npc_ids="20 21 11"/>)");
	const XMLQuest& quest = *report; // the overrides are virtual
	EXPECT_EQ(quest.getAlternativeNpcs(11), ids({10, 12})) << "the start list is tested first";
	EXPECT_EQ(quest.getAlternativeNpcs(21), ids({20, 11}));
	EXPECT_EQ(quest.getAlternativeNpcs(30), std::nullopt);
	std::unique_ptr<ReportToData> single = bindXml<ReportToData>(R"(<report_to id="2" start_npc_ids="10" end_npc_ids="10 13"/>)");
	EXPECT_EQ(single->getAlternativeNpcs(10), ids({13})) << "a start list of one id is skipped";
	EXPECT_EQ(bindXml<ReportToData>(R"(<report_to id="3" start_npc_ids="10 10"/>)")->getAlternativeNpcs(10), ids({}))
		<< "filter removes every copy: an empty, non-null set";
	EXPECT_EQ(bindXml<ReportToData>(R"(<report_to id="4"/>)")->getAlternativeNpcs(10), std::nullopt) << "null lists";

	std::unique_ptr<FountainRewardsData> fountain = bindXml<FountainRewardsData>(R"(<fountain_rewards id="5" start_npc_ids="730556 804788"/>)");
	EXPECT_EQ(fountain->getAlternativeNpcs(804788), ids({730556}));
	std::unique_ptr<ReportOnLevelUpData> levelUp = bindXml<ReportOnLevelUpData>(R"(<report_on_level_up id="6" end_npc_ids="1 2"/>)");
	EXPECT_EQ(levelUp->getAlternativeNpcs(1), ids({2})) << "only the end list";
}

TEST(XmlQuestAlternativeNpcsTest, NpcInfosAndEvents) {
	std::unique_ptr<ReportToManyData> many = bindXml<ReportToManyData>(
		R"(<report_to_many id="7" start_npc_ids="1"><npc_infos npc_ids="30 31"/><npc_infos npc_ids="31 32"/></report_to_many>)");
	EXPECT_EQ(many->getAlternativeNpcs(31), ids({30})) << "the first npc_infos containing the id";
	EXPECT_EQ(many->getAlternativeNpcs(32), ids({31}));
	EXPECT_EQ(many->getAlternativeNpcs(1), std::nullopt);

	std::unique_ptr<XmlQuestData> xmlQuest = bindXml<XmlQuestData>(R"(<xml_quest id="8" start_npc_ids="1 2">)"
		R"(<on_talk_event ids="40 41"/><on_talk_event ids="42"/><on_kill_event><monster npc_ids="50 51 52"/></on_kill_event></xml_quest>)");
	EXPECT_EQ(xmlQuest->getAlternativeNpcs(2), ids({1}));
	EXPECT_EQ(xmlQuest->getAlternativeNpcs(41), ids({40}));
	EXPECT_EQ(xmlQuest->getAlternativeNpcs(42), std::nullopt) << "an event with one id";
	EXPECT_EQ(xmlQuest->getAlternativeNpcs(52), ids({50, 51}));
	std::unique_ptr<XmlQuestData> withoutNpcIds = bindXml<XmlQuestData>(R"(<xml_quest id="9"><on_kill_event><monster/></on_kill_event></xml_quest>)");
	EXPECT_THROW(static_cast<void>(withoutNpcIds->getAlternativeNpcs(1)), runtime::NullPointerException) << "Java: monster.getNpcIds().size() on null";
}

TEST(XmlQuestAlternativeNpcsTest, MonsterHuntReadsTheQuestKills) {
	std::unique_ptr<MonsterHuntData> hunt = bindXml<MonsterHuntData>(R"(<monster_hunt id="100" start_npc_ids="1 2" aggro_start_npc_ids="3 4"/>)");
	std::unique_ptr<KillSpawnedData> spawned =
		bindXml<KillSpawnedData>(R"(<kill_spawned id="101" end_npc_ids="60 61"><monster end_var="1" npc_ids="61 62"/></kill_spawned>)");
	EXPECT_EQ(hunt->getAlternativeNpcs(4), ids({3})) << "the aggro list after start and end";
	EXPECT_EQ(spawned->getAlternativeNpcs(61), ids({62})) << "KillSpawnedData: its monsters before the lists of MonsterHuntData";
	EXPECT_EQ(spawned->getAlternativeNpcs(60), ids({61})) << "super.getAlternativeNpcs: the end list";
	EXPECT_THROW(static_cast<void>(hunt->getAlternativeNpcs(80)), runtime::NullPointerException) << "QUEST_DATA is not published";

	xml::LoadContext context;
	struct Unpublish { // also when an exception ends the test early
		~Unpublish() { dataholders::DataManager::QUEST_DATA.resetForTests(); }
	} unpublish;
	dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context,
		R"(<quests><quest id="100"><quest_kill npc_ids="80"/><quest_kill npc_ids="80 81 82"/></quest>)"
		R"(<quest id="101"><quest_kill npc_ids="90 91"/></quest></quests>)"));
	EXPECT_EQ(hunt->getAlternativeNpcs(80), ids({81, 82})) << "a quest_kill with one id is skipped";
	EXPECT_EQ(hunt->getAlternativeNpcs(99), std::nullopt);
	EXPECT_EQ(static_cast<const XMLQuest&>(*spawned).getAlternativeNpcs(91), ids({90})) << "through the super call of KillSpawnedData";
	std::unique_ptr<MonsterHuntData> unknownQuest = bindXml<MonsterHuntData>(R"(<monster_hunt id="102"/>)");
	EXPECT_THROW(static_cast<void>(unknownQuest->getAlternativeNpcs(1)), runtime::NullPointerException) << "Java: getQuestById(102) is null";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::models
