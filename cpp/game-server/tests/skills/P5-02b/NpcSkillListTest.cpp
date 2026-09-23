// The npc side of P5-02 (wave 5a, work item B-05; M5b-1 item E-02): NpcSkillList over the npc skill templates of an npc that has rows in
// npc_skills.xml, and the NpcSkillTemplateEntry it builds. Every spawned npc constructs this list (Npc's constructor), so the whole startup
// walks this path.
//
// Expectations are derived by hand from NpcSkillList.java:27-49 (the entry per template whose skill exists in SKILL_DATA, the "Missing skill"
// warning, and the distinct priorities in descending order) and NpcSkillTemplateEntry.java:37-108,160-200. The accessor cases added by E-02
// follow NpcSkillList.java:53-114 (isEmpty, getRandomSkill, getSkillOnPosition, getPostSpawnSkills, getSkillsByPriority, getChainSkills) and
// NpcSkillEntry.java:31-37 (setLastTimeUsed); getPostSpawnSkills is the AION_PARTIAL of m5b-plan.md D3.

#include <gtest/gtest.h>

#include <spdlog/sinks/ostream_sink.h>

#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::model::skill::test {
namespace {

using runtime::Ptr;
using runtime::Ref;

/** The hit count of one AION_PARTIAL site, found by the reason its macro carries (runtime/base/Unported.h) */
uint64_t partialHitsFor(std::string_view reason) {
	for (const runtime::PartialHit& hit : runtime::partialHits()) {
		if (hit.reason == reason)
			return hit.hits;
	}
	return 0;
}

constexpr std::string_view POST_SPAWN_PARTIAL = "post-spawn npc skills are not cast yet (M5b-2)";

/** Captures one logger's output (the "Missing skill" warning of NpcSkillList) */
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

/** A spawn template of the group, like the spawn data of a map */
class NpcSkillSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit NpcSkillSpawnTemplate(templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
const templates::npc::NpcTemplate* npcTemplate(int32_t npcId) {
	xml::LoadContext context;
	return xml::bindString<templates::npc::NpcTemplate>(context,
		R"(<npc_template name_id="1" npc_id=")" + std::to_string(npcId) +
			R"(" level="4" name="Test" attack_speed="2000" arange="2" rating="NORMAL" tribe="GENERAL">)"
			R"(<stats maxHp="2522" maxMp="100" pdef="130" mdef="70" attack="16" evasion="45" parry="30" block="25" accuracy="200" macc="60")"
			R"( pcrit="10" mcrit="20"><speeds walk="0.8" run="2.0" run_fight="3.0" group_walk="0.5" group_run_fight="2.5" fly="4.0"/></stats>)"
			R"(</npc_template>)")
		.release();
}

class NpcSkillListTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 7));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		// skills 1, 2 and 3 exist; 99 does not, so NpcSkillList warns and skips its template (NpcSkillList.java:37-41)
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, R"(<skill_data>)"
			R"(<skill_template skill_id="1" name="s1" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0" stack="S1"/>)"
			R"(<skill_template skill_id="2" name="s2" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0" stack="S2"/>)"
			R"(<skill_template skill_id="3" name="s3" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0" stack="S3"/>)"
			R"(</skill_data>)"));
		dataholders::DataManager::NPC_SKILL_DATA.publish(xml::bindString<dataholders::NpcSkillData>(npcSkillContext,
			R"(<npc_skill_templates>)"
			R"(<npc_skills npc_ids="700002">)"
			R"(<npc_skill id="1" lv="1" prob="0" prio="3"/>)"
			R"(<npc_skill id="2" lv="2" prob="50" prio="7" min_hp="20" max_hp="80" min_time="1000" max_time="5000" conjunction="OR" cd="4000")"
			R"( is_post_spawn="true" next_skill_time="2000" next_chain_id="9" chain_id="4"/>)"
			R"(<npc_skill id="99" lv="1" prob="25" prio="5"/>)"
			R"(<npc_skill id="3" lv="1" prob="100" prio="3"/>)"
			R"(</npc_skills>)"
			// 700004 (E-02): a chain head (next_chain_id 7) and two chain members (chain_id 7), over two priorities
			R"(<npc_skills npc_ids="700004">)"
			R"(<npc_skill id="1" lv="1" prob="100" prio="5" next_chain_id="7"/>)"
			R"(<npc_skill id="2" lv="1" prob="100" prio="5" chain_id="7"/>)"
			R"(<npc_skill id="3" lv="1" prob="100" prio="2" chain_id="7"/>)"
			R"(</npc_skills>)"
			// 700005 (E-02): exactly one skill, and it is a post-spawn one (D3)
			R"(<npc_skills npc_ids="700005">)"
			R"(<npc_skill id="1" lv="1" prob="100" prio="1" is_post_spawn="true"/>)"
			R"(</npc_skills>)"
			R"(</npc_skill_templates>)"));
	}

	void TearDown() override {
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** Java SpawnEngine: the spawn creates the Npc (and with it its NpcSkillList) and gives it a known list */
	static Ref<gameobjects::Npc> spawnNpc(int32_t npcId, Ref<templates::spawns::SpawnGroup>& group) {
		group = templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		templates::spawns::SpawnTemplate& spawnTemplate = group->addSpawnTemplate(std::make_unique<NpcSkillSpawnTemplate>(*group));
		Ref<gameobjects::Npc> npc = gameobjects::VisibleObject::create<gameobjects::Npc>(std::make_unique<controllers::NpcController>(),
			spawnTemplate, npcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		return npc;
	}

	runtime::ManualClock clock{0};
	xml::LoadContext skillContext;
	xml::LoadContext npcSkillContext;
};

TEST_F(NpcSkillListTest, SkillsAndPrioritiesOfAnNpcWithSkillTemplates) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	LogCapture log("com.aionemu.gameserver.model.skill.NpcSkillList");
	Ref<templates::spawns::SpawnGroup> group;
	Ref<gameobjects::Npc> npc = spawnNpc(700002, group);

	Ptr<NpcSkillList> skillList = npc->getSkillList();
	ASSERT_TRUE(skillList);
	EXPECT_NE(log.text().find("Missing skill 99 for npc 700002"), std::string::npos) << "log: " << log.text();

	Ptr<runtime::RcArrayList<Ref<NpcSkillEntry>>> skills = skillList->getNpcSkills();
	ASSERT_TRUE(skills);
	ASSERT_EQ(skills->size(), 3) << "skill 99 has no template and is left out";
	EXPECT_EQ(skills->get(0)->getSkillId(), 1);
	EXPECT_EQ(skills->get(1)->getSkillId(), 2);
	EXPECT_EQ(skills->get(2)->getSkillId(), 3) << "the data order, without the removed template";

	// prios: 3 and 7 of the kept templates (5 belongs to the removed one), distinct, in descending order
	Ptr<runtime::Array<int32_t>> priorities = skillList->getPriorities();
	ASSERT_TRUE(priorities);
	ASSERT_EQ(priorities->length(), 2);
	EXPECT_EQ(priorities->get(0), 7);
	EXPECT_EQ(priorities->get(1), 3);
}

TEST_F(NpcSkillListTest, TemplateEntryReadsTheNpcSkillTemplate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> group;
	Ref<gameobjects::Npc> npc = spawnNpc(700002, group);
	Ptr<runtime::RcArrayList<Ref<NpcSkillEntry>>> skills = npc->getSkillList()->getNpcSkills();
	ASSERT_EQ(skills->size(), 3);
	Ptr<NpcSkillEntry> plain = skills->get(0);   // id 1, prob 0, all defaults
	Ptr<NpcSkillEntry> ranged = skills->get(1);  // id 2, the fully configured template
	Ptr<NpcSkillEntry> certain = skills->get(2); // id 3, prob 100

	EXPECT_EQ(ranged->getSkillId(), 2);
	EXPECT_EQ(ranged->getSkillLevel(), 2);
	EXPECT_EQ(ranged->getPriority(), 7);
	EXPECT_EQ(plain->getPriority(), 3);
	ASSERT_TRUE(ranged->getTemplate());
	EXPECT_EQ(ranged->getTemplate()->getSkillId(), 2);
	EXPECT_TRUE(ranged->hasPostSpawnCondition());
	EXPECT_FALSE(plain->hasPostSpawnCondition()) << "is_post_spawn defaults to false";
	EXPECT_EQ(ranged->getNextSkillTime(), 2000);
	EXPECT_EQ(plain->getNextSkillTime(), -1) << "the next_skill_time default";
	EXPECT_TRUE(ranged->hasChain()) << "next_chain_id 9 > 0";
	EXPECT_EQ(ranged->getNextChainId(), 9);
	EXPECT_EQ(ranged->getChainId(), 4);
	EXPECT_FALSE(plain->hasChain());
	EXPECT_EQ(ranged->getConditionTemplate(), nullptr) << "no <cond> element";
	EXPECT_FALSE(ranged->hasCondition());

	// hpReady: min_hp 20, max_hp 80 (NpcSkillTemplateEntry.java:60-67)
	EXPECT_TRUE(ranged->hpReady(50));
	EXPECT_TRUE(ranged->hpReady(20));
	EXPECT_TRUE(ranged->hpReady(80));
	EXPECT_FALSE(ranged->hpReady(19));
	EXPECT_FALSE(ranged->hpReady(81));
	EXPECT_TRUE(plain->hpReady(1)) << "max_hp 100 and min_hp 0: not about hp";

	// timeReady: min_time 1000, max_time 5000 (NpcSkillTemplateEntry.java:70-74)
	EXPECT_FALSE(ranged->timeReady(999));
	EXPECT_TRUE(ranged->timeReady(1000));
	EXPECT_TRUE(ranged->timeReady(5000));
	EXPECT_FALSE(ranged->timeReady(5001));
	EXPECT_TRUE(plain->timeReady(0)) << "both times 0";

	// lastTimeUsed is 0, so the elapsed time is the whole epoch: no cooldown left
	EXPECT_FALSE(ranged->hasCooldown());
	EXPECT_FALSE(plain->hasCooldown());
	EXPECT_FALSE(plain->chanceReady()) << "prob 0: Rnd.chance() is never below 0";
	EXPECT_TRUE(certain->chanceReady()) << "prob 100: Rnd.chance() is always below 100";
	EXPECT_TRUE(certain->isReady(50, 0)) << "AND of the default hp and time ranges";
	EXPECT_FALSE(plain->isReady(50, 0)) << "the chance fails first";
	// the last skill time of a fresh npc is 0, so the elapsed time is far beyond max_chain_time
	EXPECT_FALSE(ranged->canUseNextChain(*npc));
}

TEST_F(NpcSkillListTest, NpcWithoutSkillTemplatesGetsAnEmptyList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> group;
	Ref<gameobjects::Npc> npc = spawnNpc(700003, group);

	Ptr<NpcSkillList> skillList = npc->getSkillList();
	ASSERT_TRUE(skillList);
	ASSERT_TRUE(skillList->getNpcSkills());
	EXPECT_EQ(skillList->getNpcSkills()->size(), 0) << "Java: Collections.emptyList()";
	EXPECT_FALSE(skillList->getPriorities()) << "Java leaves the priorities array null";
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// E-02 (m5b-plan.md §4): the accessors SimpleAttackManager and GeneralNpcAI.chooseAttackIntention call.
// ---------------------------------------------------------------------------------------------------------------------------------------

TEST_F(NpcSkillListTest, IsEmptyFollowsTheSkillList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> withSkills;
	Ref<templates::spawns::SpawnGroup> withoutSkills;
	Ref<gameobjects::Npc> skilled = spawnNpc(700002, withSkills);
	Ref<gameobjects::Npc> plain = spawnNpc(700003, withoutSkills);

	// Java: return skills.isEmpty() (NpcSkillList.java:53-55)
	EXPECT_FALSE(skilled->getSkillList()->isEmpty());
	EXPECT_TRUE(plain->getSkillList()->isEmpty()) << "no npc_skills row: Collections.emptyList()";
}

TEST_F(NpcSkillListTest, GetRandomSkillDrawsFromTheListAndIsNullWhenEmpty) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	Ref<templates::spawns::SpawnGroup> threeGroup;
	Ref<templates::spawns::SpawnGroup> oneGroup;
	Ref<templates::spawns::SpawnGroup> emptyGroup;
	Ref<gameobjects::Npc> three = spawnNpc(700002, threeGroup);
	Ref<gameobjects::Npc> one = spawnNpc(700005, oneGroup);
	Ref<gameobjects::Npc> none = spawnNpc(700003, emptyGroup);

	// Java: Rnd.get(skills) -> null for an empty list (Rnd.java:50-52)
	EXPECT_FALSE(none->getSkillList()->getRandomSkill());

	// a one-element list always answers that element (Java takes it without drawing)
	Ptr<NpcSkillEntry> only = one->getSkillList()->getNpcSkills()->get(0);
	for (int32_t i = 0; i < 20; ++i)
		EXPECT_EQ(one->getSkillList()->getRandomSkill().get(), only.get());

	// a three-element list answers only its own entries, and over enough draws every one of them
	std::set<int32_t> drawn;
	for (int32_t i = 0; i < 300; ++i) {
		Ptr<NpcSkillEntry> skill = three->getSkillList()->getRandomSkill();
		ASSERT_TRUE(skill) << "the list is not empty";
		EXPECT_TRUE(skill->getSkillId() == 1 || skill->getSkillId() == 2 || skill->getSkillId() == 3) << "id " << skill->getSkillId();
		drawn.insert(skill->getSkillId());
	}
	EXPECT_EQ(drawn, (std::set<int32_t>{1, 2, 3})) << "every entry can be drawn";
}

TEST_F(NpcSkillListTest, GetSkillOnPositionClampsAndAnswersNullWhenEmpty) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> group;
	Ref<templates::spawns::SpawnGroup> emptyGroup;
	Ref<gameobjects::Npc> npc = spawnNpc(700002, group);
	Ref<gameobjects::Npc> none = spawnNpc(700003, emptyGroup);
	Ptr<NpcSkillList> skillList = npc->getSkillList();

	// Java: null for an empty list, otherwise the position clamped to size - 1 (NpcSkillList.java:61-68)
	EXPECT_FALSE(none->getSkillList()->getSkillOnPosition(0));
	EXPECT_FALSE(none->getSkillList()->getSkillOnPosition(7));
	ASSERT_TRUE(skillList->getSkillOnPosition(0));
	EXPECT_EQ(skillList->getSkillOnPosition(0)->getSkillId(), 1);
	EXPECT_EQ(skillList->getSkillOnPosition(1)->getSkillId(), 2);
	EXPECT_EQ(skillList->getSkillOnPosition(2)->getSkillId(), 3);
	EXPECT_EQ(skillList->getSkillOnPosition(3)->getSkillId(), 3) << "clamped to the last entry";
	EXPECT_EQ(skillList->getSkillOnPosition(99)->getSkillId(), 3) << "clamped to the last entry";
}

TEST_F(NpcSkillListTest, PostSpawnSkillsAreThePartialThatReturnsAnEmptyList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> group;
	Ref<gameobjects::Npc> npc = spawnNpc(700005, group);
	// the npc really has a post-spawn skill, so Java's filter would answer one entry (NpcSkillList.java:70-76)
	ASSERT_EQ(npc->getSkillList()->getNpcSkills()->size(), 1);
	ASSERT_TRUE(npc->getSkillList()->getNpcSkills()->get(0)->hasPostSpawnCondition());

	// m5b-plan.md D3 / docs/deviations/P5-02.md: an AION_PARTIAL that returns empty, because SkillEngine::getSkill is unported until M5b-2
	uint64_t before = partialHitsFor(POST_SPAWN_PARTIAL);
	EXPECT_TRUE(npc->getSkillList()->getPostSpawnSkills().empty());
	EXPECT_EQ(partialHitsFor(POST_SPAWN_PARTIAL), before + 1) << "the site is marked, so the gate's allow-list sees it";
	EXPECT_TRUE(npc->getSkillList()->getPostSpawnSkills().empty());
	EXPECT_EQ(partialHitsFor(POST_SPAWN_PARTIAL), before + 2) << "one hit per call";
}

TEST_F(NpcSkillListTest, SkillsByPriorityAndChainSkillsFilterTheList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> group;
	Ref<templates::spawns::SpawnGroup> emptyGroup;
	Ref<gameobjects::Npc> npc = spawnNpc(700004, group);
	Ref<gameobjects::Npc> none = spawnNpc(700003, emptyGroup);
	Ptr<NpcSkillList> skillList = npc->getSkillList();

	const auto ids = [](const std::vector<Ptr<NpcSkillEntry>>& entries) {
		std::vector<int32_t> result;
		for (const Ptr<NpcSkillEntry>& entry : entries)
			result.push_back(entry->getSkillId());
		return result;
	};

	// Java: the entries whose getPriority() equals the argument, in list order (NpcSkillList.java:82-94)
	EXPECT_EQ(ids(skillList->getSkillsByPriority(5)), (std::vector<int32_t>{1, 2}));
	EXPECT_EQ(ids(skillList->getSkillsByPriority(2)), (std::vector<int32_t>{3}));
	EXPECT_TRUE(skillList->getSkillsByPriority(9).empty()) << "no entry has priority 9";
	EXPECT_TRUE(none->getSkillList()->getSkillsByPriority(0).empty()) << "Java: Collections.emptyList() for an empty skill list";

	Ptr<runtime::RcArrayList<Ref<NpcSkillEntry>>> skills = skillList->getNpcSkills();
	ASSERT_EQ(skills->size(), 3);
	Ptr<NpcSkillEntry> head = skills->get(0);   // next_chain_id 7
	Ptr<NpcSkillEntry> member = skills->get(1); // chain_id 7, next_chain_id 0
	ASSERT_EQ(head->getNextChainId(), 7);
	ASSERT_EQ(member->getNextChainId(), 0);

	// Java: the entries whose getChainId() equals curSkill.getNextChainId(), and only when that id is > 0 (NpcSkillList.java:100-114)
	EXPECT_EQ(ids(skillList->getChainSkills(*head)), (std::vector<int32_t>{2, 3}));
	EXPECT_TRUE(skillList->getChainSkills(*member).empty()) << "next chain id 0 is not a chain, although every entry's chain id defaults to 0";
	EXPECT_TRUE(none->getSkillList()->getChainSkills(*head).empty()) << "Java: Collections.emptyList() for an empty skill list";
}

TEST_F(NpcSkillListTest, SetLastTimeUsedStartsTheCooldown) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<templates::spawns::SpawnGroup> group;
	Ref<gameobjects::Npc> npc = spawnNpc(700002, group);
	Ptr<NpcSkillEntry> ranged = npc->getSkillList()->getNpcSkills()->get(1); // id 2, cd 4000

	// a fresh entry has never been used, so the elapsed time is the whole epoch and NpcSkillTemplateEntry::hasCooldown is false
	ASSERT_EQ(ranged->getLastTimeUsed(), 0);
	ASSERT_FALSE(ranged->hasCooldown());

	// Java: this.lastTimeUsed = System.currentTimeMillis() (NpcSkillEntry.java:35-37)
	int64_t before = commons::utils::currentTimeMillis();
	ranged->setLastTimeUsed();
	int64_t after = commons::utils::currentTimeMillis();
	EXPECT_GE(ranged->getLastTimeUsed(), before);
	EXPECT_LE(ranged->getLastTimeUsed(), after);
	EXPECT_TRUE(ranged->hasCooldown()) << "cd 4000 has just started";
}

} // namespace
} // namespace aion::gameserver::model::skill::test
