// Chunk Q01 (lane C, 2026-10-05): the hand-ported reshanta/_2759TenaciousGuardian, compiled into Q05's test executable by #include as the
// generated Q01 files are (GoldenQ01Handlers.cpp), on QuestHandlerTest's fixture with the real rows of the quest and its npcs. The oracle has
// no document for it: the port corrects the Java (owner's decision 2026-10-05; docs/deviations/Q01.md, "_2759TenaciousGuardian"), so its
// cases are written here by hand, with the Java lines beside them. Java's shared killedMobs list becomes each player's own record (var slot 1):
// two players complete the quest independently, and a guardian counts once per player.

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
#include "aion/gameserver/handlers/quest/reshanta/_2759TenaciousGuardian.cpp"
// clang-format on

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::reshanta {
namespace {

namespace fs = std::filesystem;
namespace DA = ::aion::gameserver::model::DialogAction;
using gameserver::model::Race;
using gameserver::model::gameobjects::Npc;

const fs::path STATIC_DATA = fs::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
constexpr int32_t QUEST = 2759;
constexpr int32_t GUDHARTEN = 264769;
constexpr int32_t GUARDIANS[] = {278588, 278589, 278590};

std::string printedRow(const pugi::xml_node& node) {
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}

struct ReshantaData {
	std::string questsXml;
	std::string npcsXml;
	std::string experienceXml;
};

/** The rows of quest 2759 and of Gudharten and the three guardians, read once per process; the whole experience table (level 38) */
const ReshantaData& reshantaData() {
	static const ReshantaData data = [] {
		ReshantaData d;
		pugi::xml_document quests;
		if (!quests.load_file((STATIC_DATA / "quest_data/quest_data.xml").c_str()))
			throw std::runtime_error("cannot read quest_data.xml");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			if (quest.attribute("id").as_int() == QUEST)
				d.questsXml += printedRow(quest);
		}
		d.questsXml = "<quests>" + d.questsXml + "</quests>";
		std::set<int32_t> npcIds{GUDHARTEN, GUARDIANS[0], GUARDIANS[1], GUARDIANS[2]};
		pugi::xml_document npcs;
		if (!npcs.load_file((STATIC_DATA / "npcs/npc_templates.xml").c_str()))
			throw std::runtime_error("cannot read npc_templates.xml");
		for (pugi::xml_node npc : npcs.document_element().children("npc_template")) {
			if (npcIds.contains(npc.attribute("npc_id").as_int()))
				d.npcsXml += printedRow(npc);
		}
		d.npcsXml = "<npc_templates>" + d.npcsXml + "</npc_templates>";
		std::ifstream experience(STATIC_DATA / "player_experience_table.xml", std::ios::binary);
		if (!experience)
			throw std::runtime_error("cannot read player_experience_table.xml");
		d.experienceXml.assign(std::istreambuf_iterator<char>(experience), std::istreambuf_iterator<char>());
		return d;
	}();
	return data;
}

class ReshantaHandPortsTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		const ReshantaData& data = reshantaData();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), data.questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), data.npcsXml));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), data.experienceXml));
		auto created = ::aion::gameserver::handlers::quest::reshanta::_2759TenaciousGuardian_questFactory();
		handler = created.get();
		QuestEngine::getInstance().addQuestHandler(std::move(created));
	}

	Quester* asmodian() {
		static int32_t nextObjectId = 830901;
		int32_t objectId = nextObjectId++;
		Quester* q = makeQuester(objectId, "Reshanta" + std::to_string(objectId), Race::ASMODIANS, 38);
		q->clearSent();
		return q;
	}

	bool kill(Quester& q, int32_t npcId) {
		envs.push_back(QuestEnv::create(npcOf(npcId), q.player(), QUEST, 0));
		return handler->onKillEvent(*envs.back());
	}

	bool dialog(Quester& q, Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t dialogActionId) {
		q.player().setTarget(target);
		envs.push_back(QuestEnv::create(target, q.player(), QUEST, dialogActionId));
		return QuestEngine::getInstance().onDialog(*envs.back());
	}

	static int32_t varOf(const Quester& q, int32_t slot) {
		Ptr<QuestState> qs = q.player().getQuestStateList()->getQuestState(QUEST);
		return qs ? qs->getQuestVarById(slot) : -1;
	}

	static std::optional<QuestStatus> statusOf(const Quester& q) {
		Ptr<QuestState> qs = q.player().getQuestStateList()->getQuestState(QUEST);
		return qs ? std::optional<QuestStatus>(qs->getStatus()) : std::nullopt;
	}

	AbstractQuestHandler* handler = nullptr;
	std::vector<Ref<QuestEnv>> envs;
};

TEST_F(ReshantaHandPortsTest, TheRegistrationsAreTheJavaOnes) {
	// :26-31
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(GUDHARTEN)->getOnQuestStart().contains(QUEST));
	EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(GUDHARTEN)->getOnTalkEvent().snapshot(), std::vector<int32_t>{QUEST});
	for (int32_t guardian : GUARDIANS)
		EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(guardian)->getOnKillEvent().snapshot(), std::vector<int32_t>{QUEST}) << guardian;
}

TEST_F(ReshantaHandPortsTest, TwoPlayersCompleteIndependentlyAndAGuardianCountsOncePerPlayer) {
	Npc& gudharten = npcOf(GUDHARTEN);
	Quester* first = asmodian();
	Quester* second = asmodian();
	for (Quester* q : {first, second}) {
		EXPECT_TRUE(dialog(*q, gudharten, DA::QUEST_SELECT));
		EXPECT_TRUE(q->sent().size() > 0);
		EXPECT_TRUE(dialog(*q, gudharten, DA::QUEST_ACCEPT_1)) << ":44-45 sendQuestStartDialog";
		EXPECT_EQ(statusOf(*q), QuestStatus::START);
		EXPECT_EQ(varOf(*q, 0), 0);
	}

	// the first player's kill of 278588 counts for him (:79-82) ...
	EXPECT_TRUE(kill(*first, 278588));
	EXPECT_EQ(varOf(*first, 0), 1);
	// ... and does not keep the second player from counting it (Java's shared list would: the correction)
	EXPECT_TRUE(kill(*second, 278588));
	EXPECT_EQ(varOf(*second, 0), 1);
	// no double credit: a guardian counts once per player
	EXPECT_FALSE(kill(*first, 278588));
	EXPECT_EQ(varOf(*first, 0), 1);
	EXPECT_FALSE(kill(*second, 278588));
	EXPECT_EQ(varOf(*second, 0), 1);
	// another npc counts for nobody (the switch's other labels, :78)
	EXPECT_FALSE(kill(*first, GUDHARTEN));
	EXPECT_EQ(varOf(*first, 0), 1);

	// the first player finishes his three guardians, in another order than the second will
	EXPECT_TRUE(kill(*first, 278590));
	EXPECT_TRUE(kill(*first, 278589));
	EXPECT_EQ(varOf(*first, 0), 3);
	EXPECT_FALSE(kill(*first, 278589)) << ":76 var < 3";
	EXPECT_EQ(varOf(*first, 0), 3);
	EXPECT_EQ(varOf(*second, 0), 1) << "the first player's kills are his own";

	// the report at var 3 (:51-53), the reward step (:55-58): REWARD, page 5, the record cleared
	EXPECT_TRUE(dialog(*first, gudharten, DA::QUEST_SELECT));
	EXPECT_TRUE(dialog(*first, gudharten, DA::SELECT_QUEST_REWARD));
	EXPECT_EQ(statusOf(*first), QuestStatus::REWARD);
	EXPECT_EQ(varOf(*first, 1), 0);

	// the second player completes on his own
	EXPECT_TRUE(kill(*second, 278589));
	EXPECT_FALSE(kill(*second, 278589));
	EXPECT_TRUE(kill(*second, 278590));
	EXPECT_EQ(varOf(*second, 0), 3);
	EXPECT_TRUE(dialog(*second, gudharten, DA::SELECT_QUEST_REWARD));
	EXPECT_EQ(statusOf(*second), QuestStatus::REWARD);
	EXPECT_EQ(statusOf(*first), QuestStatus::REWARD);
}

TEST_F(ReshantaHandPortsTest, ARewardSelectedBeforeTheThirdGuardianKeepsTheRecord) {
	// Java clears its list on SELECT_QUEST_REWARD also when changeQuestStep refuses (var < 3, :56-57): a guardian would count twice. Here the
	// record stays and the step does not change
	Npc& gudharten = npcOf(GUDHARTEN);
	Quester* q = asmodian();
	hold(*q, QUEST, QuestStatus::START);
	EXPECT_TRUE(kill(*q, 278588));
	EXPECT_EQ(varOf(*q, 0), 1);
	envs.push_back(QuestEnv::create(gudharten, q->player(), QUEST, DA::SELECT_QUEST_REWARD));
	EXPECT_FALSE(handler->onDialogEvent(*envs.back())) << "page 5 outside REWARD (AbstractQuestHandler.java:331-338)";
	EXPECT_EQ(statusOf(*q), QuestStatus::START);
	EXPECT_EQ(varOf(*q, 0), 1);
	EXPECT_FALSE(kill(*q, 278588)) << "no second credit for 278588";
	EXPECT_EQ(varOf(*q, 0), 1);
	EXPECT_TRUE(kill(*q, 278589));
	EXPECT_EQ(varOf(*q, 0), 2);
}

TEST_F(ReshantaHandPortsTest, AKillOutsideTheStartStateCountsNothing) {
	Quester* none = asmodian();
	EXPECT_FALSE(kill(*none, 278588)) << ":75 no quest";
	Quester* rewarded = asmodian();
	hold(*rewarded, QUEST, QuestStatus::REWARD, 3);
	EXPECT_FALSE(kill(*rewarded, 278588)) << ":75 not START";
	EXPECT_EQ(varOf(*rewarded, 1), 0);
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::reshanta
