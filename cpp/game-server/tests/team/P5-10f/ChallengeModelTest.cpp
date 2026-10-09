// P5-10f (M5h X-08, model half): ChallengeQuest and ChallengeTask against ChallengeQuest.java and ChallengeTask.java, over two task templates
// bound from the shipped data's shape (quest_data/challenge_tasks.xml, tasks 300 and 301).

#include "../P5-10b/TeamTestSupport.h"

#include <chrono>
#include <unordered_map>

#include <gtest/gtest.h>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/ChallengeData.bind.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/challenge/ChallengeQuest.h"
#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/model/templates/challenge/ChallengeQuestTemplate.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::challenge::ChallengeQuest;
using model::challenge::ChallengeTask;
using model::gameobjects::Persistable;

constexpr const char* CHALLENGE_XML = R"(<challenge_tasks>
    <task id="300" type="LEGION" race="ELYOS" min_level="5" max_level="6" name_id="1199506" repeat="true" legion_level_task="true">
        <quest id="17000" repeat_count="2" score="81"/>
        <quest id="17001" repeat_count="1" score="150"/>
        <contrib rank="1" number="2" reward_id="186000199" item_count="46"/>
        <reward type="NONE" msg_id="904485"/>
    </task>
    <task id="301" type="LEGION" race="ELYOS" prev_task="300" min_level="6" max_level="7" name_id="1199508" repeat="true" legion_level_task="true">
        <quest id="17003" repeat_count="12" score="111"/>
        <reward type="NONE" msg_id="904485"/>
    </task>
</challenge_tasks>)";

class ChallengeModelTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		dataholders::DataManager::CHALLENGE_DATA.publish(xml::bindString<dataholders::ChallengeData>(context, CHALLENGE_XML));
	}

	void TearDown() override {
		dataholders::DataManager::CHALLENGE_DATA.resetForTests();
		TeamTest::TearDown();
	}

	const model::templates::challenge::ChallengeTaskTemplate* task(int32_t id) const {
		return dataholders::DataManager::CHALLENGE_DATA->getTaskByTaskId(id);
	}

	xml::LoadContext context;
};

TEST_F(ChallengeModelTest, ANewTaskHasEveryQuestOfItsTemplateAtZeroAndNew) {
	runtime::Ref<ChallengeTask> created = ChallengeTask::create(4711, task(300));
	EXPECT_EQ(created->getTaskId(), 300) << "template.getId()";
	EXPECT_EQ(created->getOwnerId(), 4711);
	EXPECT_EQ(created->getTemplate(), task(300));
	EXPECT_EQ(created->getQuestsCount(), 2);
	runtime::Ptr<ChallengeQuest> quest = created->getQuest(17000);
	ASSERT_TRUE(quest);
	EXPECT_EQ(quest->getQuestId(), 17000);
	EXPECT_EQ(quest->getMaxRepeats(), 2);
	EXPECT_EQ(quest->getScorePerQuest(), 81);
	EXPECT_EQ(quest->getCompleteCount(), 0);
	EXPECT_EQ(quest->getPersistentState(), Persistable::PersistentState::NEW);
	EXPECT_FALSE(created->getQuest(17003)) << "a quest of another task";
	EXPECT_EQ(created->getCompleteTimeEpochSeconds(), 0) << "no complete time yet";
}

TEST_F(ChallengeModelTest, ATaskIsCompletedWhenEveryQuestReachedItsRepeats) {
	runtime::Ref<ChallengeTask> created = ChallengeTask::create(1, task(300));
	runtime::Ptr<ChallengeQuest> first = created->getQuest(17000);
	runtime::Ptr<ChallengeQuest> second = created->getQuest(17001);
	EXPECT_FALSE(created->isCompleted());
	first->increaseCompleteCount();
	first->increaseCompleteCount();
	EXPECT_EQ(first->getCompleteCount(), 2);
	EXPECT_FALSE(created->isCompleted()) << "17001 is still at 0 of 1";
	second->increaseCompleteCount();
	EXPECT_TRUE(created->isCompleted());
	EXPECT_EQ(first->getPersistentState(), Persistable::PersistentState::NEW) << "UPDATE_REQUIRED does not replace NEW";
}

TEST_F(ChallengeModelTest, PersistentStateAndTheCompleteTime) {
	runtime::Ref<ChallengeQuest> quest = ChallengeQuest::create(&task(301)->getQuests()[0], 3);
	EXPECT_EQ(quest->getCompleteCount(), 3);
	quest->setPersistentState(Persistable::PersistentState::UPDATED);
	quest->increaseCompleteCount();
	EXPECT_EQ(quest->getCompleteCount(), 4);
	EXPECT_EQ(quest->getPersistentState(), Persistable::PersistentState::UPDATE_REQUIRED);
	// the DAO constructor: the template is looked up by the task id, the quests and the complete time are kept
	std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> quests;
	quests.emplace(17003, quest);
	const commons::database::Timestamp completed{std::chrono::milliseconds(1'700'000'000'999)};
	runtime::Ref<ChallengeTask> loaded = ChallengeTask::create(301, 77, std::move(quests), completed);
	EXPECT_EQ(loaded->getTemplate(), task(301));
	EXPECT_EQ(loaded->getQuest(17003).get(), quest.get());
	EXPECT_EQ(loaded->getCompleteTimeEpochSeconds(), 1'700'000'000) << "getTime() / 1000";
	EXPECT_FALSE(loaded->isCompleted()) << "4 of 12";
	const int64_t before = commons::utils::currentTimeMillis() / 1000;
	loaded->updateCompleteTime();
	EXPECT_GE(loaded->getCompleteTimeEpochSeconds(), before);
	EXPECT_LE(loaded->getCompleteTimeEpochSeconds(), commons::utils::currentTimeMillis() / 1000);
	EXPECT_EQ(ChallengeTask::create(999, 1, {}, std::nullopt)->getTemplate(), nullptr) << "an unknown task id has no template (Java null)";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
