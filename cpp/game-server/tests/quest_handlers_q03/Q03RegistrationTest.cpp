// Q03's registration checks (P6-Q slice 2, 2026-09-29): every Java handler of the chunk's two directories (game-server/data/handlers/quest/
// verteron and heiron, 78 files) has its C++ handler in Q03's library but the 4 held back (Q03Handlers.h), each AION_QUEST_HANDLER marker
// creates a handler of the Java class's quest id (Java: super(id) in the constructor), and all 74 register together in one QuestEngine without
// a duplicate (QuestEngine.init with the registry), the held-back ones not at all. The registration trace of each generated handler against
// its Java register() is the golden harness's (tests/quest_handlers_golden, RegistrationTraceMatchesJavaRegister); the two hand ports' own are
// in HeironHandPortsTest.cpp.

#include "Q03Handlers.h"
#include "Q03QuestTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "aion/gameserver/model/templates/quest/QuestNpc.h"

namespace aion::gameserver::questEngine::handlers::q03::test {
namespace {

namespace fs = std::filesystem;

const fs::path JAVA_QUEST_DIR = fs::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/quest";

TEST(Q03RegistrationTest, EveryJavaHandlerOfTheChunkHasItsCppHandlerAndNoOther) {
	std::set<std::string> java, cpp;
	for (std::string_view dir : {"verteron", "heiron"}) {
		ASSERT_TRUE(fs::is_directory(JAVA_QUEST_DIR / dir)) << JAVA_QUEST_DIR / dir;
		for (const fs::directory_entry& entry : fs::directory_iterator(JAVA_QUEST_DIR / dir)) {
			if (entry.path().extension() == ".java")
				java.insert(std::string(dir) + "/" + entry.path().stem().string());
		}
	}
	for (const Q03Handler& handler : q03Handlers())
		cpp.insert(std::string(handler.directory) + "/" + std::string(handler.javaClass));
	std::set<std::string> heldBack;
	for (const Q03HeldBack& held : Q03_HELD_BACK) {
		std::string name = std::string(held.directory) + "/" + std::string(held.javaClass);
		EXPECT_FALSE(cpp.contains(name)) << name << " is held back but in the library";
		heldBack.insert(name);
	}
	EXPECT_EQ(java.size(), 78u);
	EXPECT_EQ(cpp.size(), 74u);
	std::set<std::string> all = cpp;
	all.insert(heldBack.begin(), heldBack.end());
	EXPECT_EQ(all, java);
	EXPECT_EQ(std::count_if(q03Handlers().begin(), q03Handlers().end(), [](const Q03Handler& h) { return !h.generated; }), 2)
		<< "the two hand ports: heiron/_1643TheStarOfHeiron and heiron/_3200PriceOfGoodwill";
}

/** The rows of quest_data.xml for the chunk's quests (registerOnLevelChanged reads the template's race, QuestEngine.java) */
std::string questRows() {
	pugi::xml_document quests;
	if (!quests.load_file((fs::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data/quest_data/quest_data.xml").c_str()))
		throw std::runtime_error("cannot read quest_data.xml");
	std::set<int32_t> ids;
	for (const Q03Handler& handler : q03Handlers())
		ids.insert(handler.questId);
	std::string rows;
	for (pugi::xml_node quest : quests.document_element().children("quest")) {
		if (ids.contains(quest.attribute("id").as_int())) {
			std::ostringstream out;
			quest.print(out, "", pugi::format_raw);
			rows += out.str();
		}
	}
	return "<quests>" + rows + "</quests>";
}

class Q03RegistryTest : public Q03QuestTest {
protected:
	/** The fixture's two quest rows replaced by the chunk's 74 (the fixture's TearDown resets the holder) */
	void publishChunkQuests() {
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), questRows()));
	}
};

TEST_F(Q03RegistryTest, EveryMarkerCreatesAHandlerOfItsJavaQuestId) {
	publishChunkQuests(); // the constructor reads the template (AbstractQuestHandler.java: its work and action items)
	std::set<int32_t> ids;
	for (const Q03Handler& entry : q03Handlers()) {
		std::unique_ptr<AbstractQuestHandler> created = entry.factory();
		ASSERT_TRUE(created) << entry.javaClass;
		EXPECT_EQ(created->getQuestId(), entry.questId) << entry.javaClass;
		// Java: the class name starts with _<questId> (the chunk's naming convention, which QuestHandlerLoader does not check)
		EXPECT_TRUE(entry.javaClass.starts_with("_" + std::to_string(entry.questId))) << entry.javaClass;
		EXPECT_TRUE(ids.insert(entry.questId).second) << "duplicate quest id " << entry.questId;
	}
	EXPECT_EQ(ids.size(), 74u);
}

TEST_F(Q03RegistryTest, AllSeventyFourRegisterTogetherLikeTheRegistryAndTheHeldBackOnesNot) {
	publishChunkQuests();
	QuestEngine& engine = QuestEngine::getInstance();
	engine.clear();
	for (const Q03Handler& entry : q03Handlers())
		engine.addQuestHandler(entry.factory());
	EXPECT_EQ(engine.getQuestHandlerCount(), 74);
	for (const Q03Handler& entry : q03Handlers())
		EXPECT_TRUE(engine.isHaveHandler(entry.questId)) << entry.javaClass;
	for (const Q03HeldBack& held : Q03_HELD_BACK)
		EXPECT_FALSE(engine.isHaveHandler(held.questId)) << held.javaClass << " is held back (docs/deviations/Q03.md)";
	// a spot check of the merged lists: 1643's and 3200's givers, 3200's scroll and 1197's book (by the engine's own lists, not per handler:
	// the golden harness checks each generated handler alone)
	EXPECT_TRUE(engine.getQuestNpc(204545)->getOnQuestStart().contains(1643));
	EXPECT_TRUE(engine.getQuestNpc(204658)->getOnQuestStart().contains(3200));
	EXPECT_TRUE(engine.isRegisteredQuestItem(182209082));
	EXPECT_TRUE(engine.isRegisteredQuestItem(182200558));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::q03::test
