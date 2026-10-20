// P6 leftovers (lane C, 2026-10-18): the hand-ported event_quests/_80016EventSockHop and _80018EventSockItToEm (docs/deviations/Q12.md,
// "80016 and 80018"), compiled into Q05's test executable by #include as CounterHandPortsTest.cpp compiles its hand ports, on
// QuestHandlerTest's fixture with the real rows of the quests and their npcs. The oracle's traces have no case for them (event quests, as
// 80000 and 80001), so the cases are written here by hand, with the Java lines beside them.

#include "../quest_handlers/QuestHandlerTestSupport.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/rewards/BonusType.h"
#include "aion/gameserver/questEngine/QuestEngine.h"

// clang-format off
#include "aion/gameserver/handlers/quest/event_quests/_80016EventSockHop.cpp"
#include "aion/gameserver/handlers/quest/event_quests/_80018EventSockItToEm.cpp"
// clang-format on

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::eventsocks {
namespace {

namespace fs = std::filesystem;
namespace DA = ::aion::gameserver::model::DialogAction;
using gameserver::model::Race;
using gameserver::model::templates::quest::QuestItems;
using gameserver::model::templates::rewards::BonusType;

const fs::path STATIC_DATA = fs::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
constexpr int32_t SOCK_HOP = 80016;
constexpr int32_t SOCK_IT_TO_EM = 80018;
constexpr int32_t SOCK_HOP_NPC = 799763;
constexpr int32_t SOCK_IT_TO_EM_NPC = 799778;
constexpr int32_t HAT_BOX = 188051106;

std::string printedRow(const pugi::xml_node& node) {
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}

struct SockData {
	std::string questsXml;
	std::string npcsXml;
	std::string experienceXml;
};

/** The rows of quests 80016 and 80018 and of their npcs, read once per process; the whole experience table */
const SockData& sockData() {
	static const SockData data = [] {
		SockData d;
		pugi::xml_document quests;
		if (!quests.load_file((STATIC_DATA / "quest_data/quest_data.xml").c_str()))
			throw std::runtime_error("cannot read quest_data.xml");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			int32_t id = quest.attribute("id").as_int();
			if (id == SOCK_HOP || id == SOCK_IT_TO_EM)
				d.questsXml += printedRow(quest);
		}
		d.questsXml = "<quests>" + d.questsXml + "</quests>";
		std::set<int32_t> npcIds{SOCK_HOP_NPC, SOCK_IT_TO_EM_NPC};
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

class EventSockHandPortsTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		const SockData& data = sockData();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), data.questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), data.npcsXml));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), data.experienceXml));
		auto hop = ::aion::gameserver::handlers::quest::event_quests::_80016EventSockHop_questFactory();
		hopHandler = hop.get();
		QuestEngine::getInstance().addQuestHandler(std::move(hop));
		auto itToEm = ::aion::gameserver::handlers::quest::event_quests::_80018EventSockItToEm_questFactory();
		itToEmHandler = itToEm.get();
		QuestEngine::getInstance().addQuestHandler(std::move(itToEm));
	}

	Quester* quester() {
		static int32_t nextObjectId = 832301;
		int32_t objectId = nextObjectId++;
		Quester* q = makeQuester(objectId, "Sock" + std::to_string(objectId), Race::ELYOS, 30);
		q->clearSent();
		return q;
	}

	QuestEnv& env(Quester& q, int32_t questId, int32_t targetNpc = 0, int32_t dialogActionId = 0) {
		Ptr<gameserver::model::gameobjects::VisibleObject> target;
		if (targetNpc != 0) {
			target = npcOf(targetNpc);
			q.player().setTarget(target);
		}
		envs.push_back(QuestEnv::create(target, q.player(), questId, dialogActionId));
		return *envs.back();
	}

	/** onBonusApplyEvent on a quest at `status` completed `completed` times: the result and the reward list it leaves */
	std::pair<HandlerResult, std::vector<QuestItems>> bonus(AbstractQuestHandler& handler, int32_t questId, QuestStatus status, int32_t completed,
		BonusType type = BonusType::MOVIE, int32_t envQuestId = 0) {
		Quester* q = quester();
		Ref<QuestState> qs = hold(*q, questId, status);
		qs->setCompleteCount(completed);
		std::vector<QuestItems> rewards;
		HandlerResult result = handler.onBonusApplyEvent(env(*q, envQuestId == 0 ? questId : envQuestId), type, rewards);
		return {result, rewards};
	}

	static Ptr<QuestState> stateOf(const Quester& q, int32_t questId) { return q.player().getQuestStateList()->getQuestState(questId); }

	AbstractQuestHandler* hopHandler = nullptr;
	AbstractQuestHandler* itToEmHandler = nullptr;
	std::vector<Ref<QuestEnv>> envs;
};

TEST_F(EventSockHandPortsTest, TheRegistrationsAreTheJavaOnes) {
	// _80016EventSockHop.java:28-30, _80018EventSockItToEm.java:28-30
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(SOCK_HOP_NPC)->getOnQuestStart().contains(SOCK_HOP));
	EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(SOCK_HOP_NPC)->getOnTalkEvent().snapshot(), std::vector<int32_t>{SOCK_HOP});
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(SOCK_IT_TO_EM_NPC)->getOnQuestStart().contains(SOCK_IT_TO_EM));
	EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(SOCK_IT_TO_EM_NPC)->getOnTalkEvent().snapshot(), std::vector<int32_t>{SOCK_IT_TO_EM});
}

TEST_F(EventSockHandPortsTest, TheTenthRewardAddsTheHatBox) {
	// _80016EventSockHop.java:73-88: a REWARD state completed 9 times adds the hat box to the reward list
	for (auto [handler, questId] : {std::pair{hopHandler, SOCK_HOP}, std::pair{itToEmHandler, SOCK_IT_TO_EM}}) {
		auto [result, rewards] = bonus(*handler, questId, QuestStatus::REWARD, 9);
		EXPECT_EQ(result, HandlerResult::SUCCESS) << questId;
		ASSERT_EQ(rewards.size(), 1u) << questId;
		EXPECT_EQ(rewards[0].getItemId(), HAT_BOX) << questId;
		EXPECT_EQ(rewards[0].getCount(), 1) << questId;
		auto [other, none] = bonus(*handler, questId, QuestStatus::REWARD, 8);
		EXPECT_EQ(other, HandlerResult::SUCCESS) << questId;
		EXPECT_TRUE(none.empty()) << questId << ": not the tenth";
	}
}

TEST_F(EventSockHandPortsTest, TheBonusIsOnlyAMoviesOfARewardState) {
	// _80016EventSockHop.java:74-75, 79, 87
	for (auto [handler, questId] : {std::pair{hopHandler, SOCK_HOP}, std::pair{itToEmHandler, SOCK_IT_TO_EM}}) {
		EXPECT_EQ(bonus(*handler, questId, QuestStatus::REWARD, 9, BonusType::BOSS).first, HandlerResult::UNKNOWN) << questId;
		EXPECT_EQ(bonus(*handler, questId, QuestStatus::REWARD, 9, BonusType::MOVIE, 80000).first, HandlerResult::UNKNOWN) << questId;
		auto [started, rewards] = bonus(*handler, questId, QuestStatus::START, 9);
		EXPECT_EQ(started, HandlerResult::FAILED) << questId;
		EXPECT_TRUE(rewards.empty()) << questId;
		Quester* q = quester();
		std::vector<QuestItems> noState;
		EXPECT_EQ(handler->onBonusApplyEvent(env(*q, questId), BonusType::MOVIE, noState), HandlerResult::FAILED) << questId << ": no state";
	}
}

TEST_F(EventSockHandPortsTest, TheDialogsAreTheJavaOnes) {
	// _80016EventSockHop.java:34-69: no state, the npc: page 1011 (USE_OBJECT); another npc: nothing; at START, var 0: page 2375
	for (auto [questId, npc, other] : {std::tuple{SOCK_HOP, SOCK_HOP_NPC, SOCK_IT_TO_EM_NPC}, std::tuple{SOCK_IT_TO_EM, SOCK_IT_TO_EM_NPC, SOCK_HOP_NPC}}) {
		Quester* q = quester();
		AbstractQuestHandler& handler = questId == SOCK_HOP ? *hopHandler : *itToEmHandler;
		EXPECT_TRUE(handler.onDialogEvent(env(*q, questId, npc, DA::USE_OBJECT))) << questId;
		EXPECT_FALSE(handler.onDialogEvent(env(*q, questId, other, DA::USE_OBJECT))) << questId << ": another npc";
		hold(*q, questId, QuestStatus::START);
		EXPECT_TRUE(handler.onDialogEvent(env(*q, questId, npc, DA::QUEST_SELECT))) << questId << ": page 2375";
		stateOf(*q, questId)->setQuestVarById(0, 1);
		EXPECT_FALSE(handler.onDialogEvent(env(*q, questId, npc, DA::QUEST_SELECT))) << questId << ": var 1";
	}
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::eventsocks
