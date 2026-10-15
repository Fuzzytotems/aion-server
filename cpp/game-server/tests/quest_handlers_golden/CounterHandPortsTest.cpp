// The owner's correction of 2026-10-13 (both branches; lane C): the hand-ported eltnen/_1367MabangtahsFeast and morheim/_24026AHandfromEachSide,
// compiled into Q05's test executable by #include as ReshantaHandPortsTest.cpp compiles 2759, on QuestHandlerTest's fixture with the real
// rows of the quests, their npcs and 1367's items. The oracle has no document for them: the ports correct the Java (docs/deviations/Q05.md
// and Q09.md, "Correction: 1367 and 24026"), so their cases are written here by hand, with the Java lines beside them. Java's counters in the
// singleton handler become each player's own: 1367's item counts are locals of the dialog, 24026's balaur count is the quest's var slot 1.

#include "../quest_handlers/QuestHandlerTestSupport.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"

// clang-format off
#include "aion/gameserver/handlers/quest/eltnen/_1354PraticalAerobatics.cpp"
#include "aion/gameserver/handlers/quest/eltnen/_14026ALoneDefense.cpp"
#include "aion/gameserver/handlers/quest/event_quests/_80030EventAnUnwelcomeGaze.cpp"
#include "aion/gameserver/handlers/quest/event_quests/_80033EventAvertingTheGaze.cpp"
#include "aion/gameserver/handlers/quest/eltnen/_1367MabangtahsFeast.cpp"
#include "aion/gameserver/handlers/quest/morheim/_24026AHandfromEachSide.cpp"
// clang-format on

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::counters {
namespace {

namespace fs = std::filesystem;
namespace DA = ::aion::gameserver::model::DialogAction;
using gameserver::model::Race;
using gameserver::model::gameobjects::Npc;

const fs::path STATIC_DATA = fs::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
constexpr int32_t FEAST = 1367;
constexpr int32_t MABANGTAH = 204023;
constexpr int32_t FEAST_ITEMS[] = {182201331, 182201332, 182201333};
constexpr int32_t DEFENCE = 24026;
constexpr int32_t KARGATE = 204432;
constexpr int32_t BALAUR[] = {213575, 280818};
constexpr int32_t AEROBATICS = 1354;
constexpr int32_t ERACUS = 203983;
constexpr int32_t UNWELCOME_GAZE = 80030;
constexpr int32_t AVERTING_THE_GAZE = 80033;
constexpr int32_t CHARM_CARD = 188051133;
constexpr int32_t LONE_DEFENSE = 14026;

std::string printedRow(const pugi::xml_node& node) {
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}

struct CounterData {
	std::string questsXml;
	std::string npcsXml;
	std::string itemRows;
	std::string experienceXml;
};

/** The rows of quests 1367 and 24026, of their npcs and of 1367's three items, read once per process; the whole experience table */
const CounterData& counterData() {
	static const CounterData data = [] {
		CounterData d;
		pugi::xml_document quests;
		if (!quests.load_file((STATIC_DATA / "quest_data/quest_data.xml").c_str()))
			throw std::runtime_error("cannot read quest_data.xml");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			int32_t id = quest.attribute("id").as_int();
			if (id == FEAST || id == DEFENCE || id == AEROBATICS || id == UNWELCOME_GAZE || id == AVERTING_THE_GAZE || id == LONE_DEFENSE)
				d.questsXml += printedRow(quest);
		}
		d.questsXml = "<quests>" + d.questsXml + "</quests>";
		std::set<int32_t> npcIds{MABANGTAH, KARGATE, 204301, 204403, BALAUR[0], BALAUR[1], ERACUS, 799766, 799781, 203901, 204020, 204044,
			211628, 211630};
		pugi::xml_document npcs;
		if (!npcs.load_file((STATIC_DATA / "npcs/npc_templates.xml").c_str()))
			throw std::runtime_error("cannot read npc_templates.xml");
		for (pugi::xml_node npc : npcs.document_element().children("npc_template")) {
			if (npcIds.contains(npc.attribute("npc_id").as_int()))
				d.npcsXml += printedRow(npc);
		}
		d.npcsXml = "<npc_templates>" + d.npcsXml + "</npc_templates>";
		std::set<int32_t> itemIds(std::begin(FEAST_ITEMS), std::end(FEAST_ITEMS));
		pugi::xml_document items;
		if (!items.load_file((STATIC_DATA / "items/item_templates.xml").c_str()))
			throw std::runtime_error("cannot read item_templates.xml");
		for (pugi::xml_node item : items.document_element().children("item_template")) {
			if (itemIds.contains(item.attribute("id").as_int()))
				d.itemRows += printedRow(item);
		}
		std::ifstream experience(STATIC_DATA / "player_experience_table.xml", std::ios::binary);
		if (!experience)
			throw std::runtime_error("cannot read player_experience_table.xml");
		d.experienceXml.assign(std::istreambuf_iterator<char>(experience), std::istreambuf_iterator<char>());
		return d;
	}();
	return data;
}

class CounterHandPortsTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		const CounterData& data = counterData();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), data.questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), data.npcsXml));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += data.itemRows;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), data.experienceXml));
		auto feast = ::aion::gameserver::handlers::quest::eltnen::_1367MabangtahsFeast_questFactory();
		feastHandler = feast.get();
		QuestEngine::getInstance().addQuestHandler(std::move(feast));
		auto defence = ::aion::gameserver::handlers::quest::morheim::_24026AHandfromEachSide_questFactory();
		defenceHandler = defence.get();
		QuestEngine::getInstance().addQuestHandler(std::move(defence));
		auto aerobatics = ::aion::gameserver::handlers::quest::eltnen::_1354PraticalAerobatics_questFactory();
		aerobaticsHandler = aerobatics.get();
		QuestEngine::getInstance().addQuestHandler(std::move(aerobatics));
		QuestEngine::getInstance().addQuestHandler(::aion::gameserver::handlers::quest::event_quests::_80030EventAnUnwelcomeGaze_questFactory());
		QuestEngine::getInstance().addQuestHandler(::aion::gameserver::handlers::quest::event_quests::_80033EventAvertingTheGaze_questFactory());
		QuestEngine::getInstance().addQuestHandler(::aion::gameserver::handlers::quest::eltnen::_14026ALoneDefense_questFactory());
	}

	Quester* quester(Race race, int32_t level) {
		static int32_t nextObjectId = 831901;
		int32_t objectId = nextObjectId++;
		Quester* q = makeQuester(objectId, "Counter" + std::to_string(objectId), race, level);
		q->clearSent();
		return q;
	}

	bool dialog(Quester& q, Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t questId, int32_t dialogActionId) {
		q.player().setTarget(target);
		envs.push_back(QuestEnv::create(target, q.player(), questId, dialogActionId));
		return QuestEngine::getInstance().onDialog(*envs.back());
	}

	bool kill(Quester& q, int32_t npcId) {
		envs.push_back(QuestEnv::create(npcOf(npcId), q.player(), DEFENCE, 0));
		return defenceHandler->onKillEvent(*envs.back());
	}

	static Ptr<QuestState> stateOf(const Quester& q, int32_t questId) { return q.player().getQuestStateList()->getQuestState(questId); }

	/** a quest at START with step `step` and the balaur count `killed` in var slot 1 */
	static void defending(Quester& q, int32_t step, int32_t killed) {
		Ref<QuestState> qs = hold(q, DEFENCE, QuestStatus::START);
		qs->setQuestVarById(0, step);
		qs->setQuestVarById(1, killed);
	}

	AbstractQuestHandler* feastHandler = nullptr;
	AbstractQuestHandler* defenceHandler = nullptr;
	AbstractQuestHandler* aerobaticsHandler = nullptr;
	std::vector<Ref<QuestEnv>> envs;
	int32_t nextItemObjectId = 832901;
};

// --- eltnen/_1367MabangtahsFeast -------------------------------------------------------------------------------------------------------

TEST_F(CounterHandPortsTest, TheFeastRegistrationsAreTheJavaOnes) {
	// :29-31
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(MABANGTAH)->getOnQuestStart().contains(FEAST));
	EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(MABANGTAH)->getOnTalkEvent().snapshot(), std::vector<int32_t>{FEAST});
}

TEST_F(CounterHandPortsTest, EachPlayerGivesHisOwnItemsAndKeepsTheOthers) {
	Npc& mabangtah = npcOf(MABANGTAH);
	Quester* first = quester(Race::ELYOS, 30);
	Quester* second = quester(Race::ELYOS, 30);
	hold(*first, FEAST, QuestStatus::START);
	hold(*second, FEAST, QuestStatus::START);
	holdItem(*first, nextItemObjectId++, FEAST_ITEMS[0], 1);
	holdItem(*first, nextItemObjectId++, FEAST_ITEMS[1], 3);
	holdItem(*second, nextItemObjectId++, FEAST_ITEMS[1], 5);
	holdItem(*second, nextItemObjectId++, FEAST_ITEMS[2], 2);

	// the second player opens the dialog (Java writes the shared fields here, :48-50), then the first picks the first reward
	EXPECT_TRUE(dialog(*second, mabangtah, FEAST, DA::QUEST_SELECT));
	EXPECT_TRUE(dialog(*first, mabangtah, FEAST, DA::SETPRO1));
	EXPECT_EQ(stateOf(*first, FEAST)->getStatus(), QuestStatus::REWARD) << ":57-62";
	EXPECT_EQ(stateOf(*first, FEAST)->getRewardGroup(), 0);
	// removeQuestItems takes the first player's own counts of all three items (:93-97)
	EXPECT_EQ(held(*first, FEAST_ITEMS[0]), 0);
	EXPECT_EQ(held(*first, FEAST_ITEMS[1]), 0);
	// the second player's items are his
	EXPECT_EQ(held(*second, FEAST_ITEMS[1]), 5);
	EXPECT_EQ(held(*second, FEAST_ITEMS[2]), 2);
	EXPECT_EQ(stateOf(*second, FEAST)->getStatus(), QuestStatus::START);

	// the second player's third reward needs 2 of the third item, which he has: his own counts, taken whole
	EXPECT_TRUE(dialog(*second, mabangtah, FEAST, DA::SETPRO3));
	EXPECT_EQ(stateOf(*second, FEAST)->getStatus(), QuestStatus::REWARD) << ":75-80";
	EXPECT_EQ(stateOf(*second, FEAST)->getRewardGroup(), 2);
	EXPECT_EQ(held(*second, FEAST_ITEMS[1]), 0);
	EXPECT_EQ(held(*second, FEAST_ITEMS[2]), 0);
}

TEST_F(CounterHandPortsTest, AFeastRewardWithoutTheItemsStaysOnTheStep) {
	Npc& mabangtah = npcOf(MABANGTAH);
	Quester* q = quester(Race::ELYOS, 30);
	hold(*q, FEAST, QuestStatus::START);
	holdItem(*q, nextItemObjectId++, FEAST_ITEMS[1], 4);
	EXPECT_TRUE(dialog(*q, mabangtah, FEAST, DA::SETPRO2)) << ":72 sendQuestDialog(env, 1352)";
	EXPECT_EQ(stateOf(*q, FEAST)->getStatus(), QuestStatus::START);
	EXPECT_EQ(held(*q, FEAST_ITEMS[1]), 4);
}

// --- morheim/_24026AHandfromEachSide ---------------------------------------------------------------------------------------------------

TEST_F(CounterHandPortsTest, TheDefenceRegistrationsAreTheJavaOnes) {
	// :33-42
	for (int32_t npc : {204301, 204403, KARGATE})
		EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(npc)->getOnTalkEvent().snapshot(), std::vector<int32_t>{DEFENCE}) << npc;
	for (int32_t mob : BALAUR)
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(mob)->getOnKillEvent().contains(DEFENCE)) << mob; // 213575 is 14026's too
}

TEST_F(CounterHandPortsTest, EachDefenderCountsHisOwnBalaur) {
	Quester* first = quester(Race::ASMODIANS, 35);
	Quester* second = quester(Race::ASMODIANS, 35);
	defending(*first, 3, 0);
	defending(*second, 3, 0);
	// Java's one counter would reach 2 with these two kills and spawn the second wave for the second player (:138-141); here each player's
	// first kill is his first
	EXPECT_TRUE(kill(*first, BALAUR[0]));
	EXPECT_TRUE(kill(*second, BALAUR[1]));
	EXPECT_EQ(stateOf(*first, DEFENCE)->getQuestVarById(1), 1);
	EXPECT_EQ(stateOf(*second, DEFENCE)->getQuestVarById(1), 1);
	EXPECT_EQ(stateOf(*first, DEFENCE)->getQuestVarById(0), 3);
	EXPECT_EQ(stateOf(*second, DEFENCE)->getQuestVarById(0), 3);
}

TEST_F(CounterHandPortsTest, TheFourthBalaurOfAPlayerEndsHisDefenceAlone) {
	Quester* first = quester(Race::ASMODIANS, 35);
	Quester* second = quester(Race::ASMODIANS, 35);
	defending(*first, 3, 3);
	defending(*second, 3, 1);
	// :142-150: the fourth kill ends the timer; Kargate stands in no map of the fixture, so the step goes back to 2 (and Kargate respawns)
	EXPECT_TRUE(kill(*first, BALAUR[0]));
	EXPECT_EQ(stateOf(*first, DEFENCE)->getQuestVarById(1), 4);
	EXPECT_EQ(stateOf(*first, DEFENCE)->getQuestVarById(0), 2);
	// the second defender's count and step are his own
	EXPECT_EQ(stateOf(*second, DEFENCE)->getQuestVarById(1), 1);
	EXPECT_EQ(stateOf(*second, DEFENCE)->getQuestVarById(0), 3);
}

TEST_F(CounterHandPortsTest, ABalaurOutsideTheDefenceCountsNothing) {
	Quester* none = quester(Race::ASMODIANS, 35);
	EXPECT_FALSE(kill(*none, BALAUR[0])) << ":129 no quest";
	Quester* early = quester(Race::ASMODIANS, 35);
	defending(*early, 2, 0);
	EXPECT_FALSE(kill(*early, BALAUR[0])) << ":136 step 2";
	EXPECT_EQ(stateOf(*early, DEFENCE)->getQuestVarById(1), 0);
	Quester* other = quester(Race::ASMODIANS, 35);
	defending(*other, 3, 0);
	EXPECT_FALSE(kill(*other, KARGATE)) << ":137 not a balaur";
	EXPECT_EQ(stateOf(*other, DEFENCE)->getQuestVarById(1), 0);
}

// --- eltnen/_1354PraticalAerobatics (hand port, no correction; lane C, 2026-10-14) ------------------------------------------------------
// The ring path's applyFlightRingEffect needs the player's life stats and the skill engine, which this fixture does not build: the
// registrations, the dialog and the timer reset are checked here

TEST_F(CounterHandPortsTest, TheAerobaticsRegistrationsAreTheJavaOnes) {
	// :30-37: Eracus's start and talk, the timer end, the seven Eracus Temple rings
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(ERACUS)->getOnQuestStart().contains(AEROBATICS));
	EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(ERACUS)->getOnTalkEvent().snapshot(), std::vector<int32_t>{AEROBATICS});
}

TEST_F(CounterHandPortsTest, TheAerobaticsDialogStartsTheTimerAndItsEndResetsTheStep) {
	Npc& eracus = npcOf(ERACUS);
	Quester* q = quester(Race::ELYOS, 30);
	hold(*q, AEROBATICS, QuestStatus::START);
	EXPECT_TRUE(dialog(*q, eracus, AEROBATICS, DA::SETPRO1)) << ":107-111 questTimerStart, defaultCloseDialog(env, 0, 1)";
	EXPECT_EQ(stateOf(*q, AEROBATICS)->getQuestVarById(0), 1);
	EXPECT_FALSE(dialog(*q, eracus, AEROBATICS, DA::SETPRO1)) << ":112 var 1";
	envs.push_back(QuestEnv::create(nullptr, q->player(), AEROBATICS, 0));
	EXPECT_TRUE(aerobaticsHandler->onQuestTimerEndEvent(*envs.back())) << ":68-77";
	EXPECT_EQ(stateOf(*q, AEROBATICS)->getQuestVarById(0), 0);
}

// --- event_quests/_80030EventAnUnwelcomeGaze, _80033EventAvertingTheGaze (hand ports, no correction; lane C, 2026-10-15) ---------------
// The charm card's scheduled check needs the event service's active events and a ten second task: the registrations are checked here

TEST_F(CounterHandPortsTest, TheGazeRegistrationsAreTheJavaOnes) {
	// _80030EventAnUnwelcomeGaze.java:28-32, _80033EventAvertingTheGaze.java:28-32
	for (const auto& [quest, npc] : {std::pair{UNWELCOME_GAZE, 799766}, std::pair{AVERTING_THE_GAZE, 799781}}) {
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npc)->getOnQuestStart().contains(quest)) << quest;
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npc)->getOnTalkEvent().contains(quest)) << quest;
	}
	EXPECT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(CHARM_CARD));
}

// --- eltnen/_14026ALoneDefense (hand port, no correction; lane C, 2026-10-15) ---------------------------------------------------------
// The kill path spawns a mob and walks it to Kargate's helper (spawn data, a map with known lists): the registrations are checked here

TEST_F(CounterHandPortsTest, TheLoneDefenseRegistrationsAreTheJavaOnes) {
	// _14026ALoneDefense.java:38-51: the four talk npcs (700141 an object) and the three mobs' kills
	for (int32_t npc : {203901, 204020, 204044, 700141})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npc)->getOnTalkEvent().contains(LONE_DEFENSE)) << npc;
	for (int32_t mob : {211628, 211630, 213575})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(mob)->getOnKillEvent().contains(LONE_DEFENSE)) << mob;
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::counters
