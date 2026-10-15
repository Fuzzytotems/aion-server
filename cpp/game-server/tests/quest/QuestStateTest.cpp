// P5-06a, M5d E-01 (m5d-plan.md §7): the quest state model - QuestVars' 6-bit packing (QuestVars.java), QuestState's setters, persistent
// state, completion count and time, canRepeat/isStartable and the step group (QuestState.java), and QuestEnv.getTargetId (QuestEnv.java:94-96).
//
// The golden vectors of the packing were computed by emulating Java's int arithmetic (32-bit two's complement, `>>` arithmetic, `<<` dropping
// the bits shifted out) over QuestVars.setVar and getQuestVars (QuestVars.java:55-60, 40-46); the script is the lane's scratch
// `m5d-engine/javavars.py`, and every vector is stated with its derivation beside it.
//
// Fixture rows are copied from the shipped data: quests 1101 "Sleeping on the Job" (quest_data.xml:895-897, max_repeat_count 1), 1221
// "Brainwashed Tursin Krall" (:1869-1877, 5), 1853 "[Weekly/Group] Officer Ousting" (:7783-7790, 10, repeat_cycle WED) and 9601 "[Test]
// Limited Quest Mondays Only" (:32819-32821, 255, repeat_cycle MON WED FRI); the npc 210668 "pinkbeak airon" (npc_templates.xml:57842-57847).
// Time is the ManualClock of the in-world fixture (tests/cm_ak/InWorldPacketRunSupport.h), which QuestState reads through the installed
// scheduler backend (docs/deviations/P5-06a.md).

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::model::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
using PersistentState = gameserver::model::gameobjects::Persistable::PersistentState;
using commons::database::Timestamp;
using runtime::Ref;

constexpr const char* QUESTS_XML =
	R"(<quests>)"
	// quest_data.xml:895-897
	R"(<quest id="1101" name="Sleeping on the Job" nameId="1102201" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" can_report="true")"
	R"( race_permitted="ELYOS" category="IMPORTANT"><rewards gold="120" exp="130"/></quest>)"
	// :1869-1877
	R"(<quest id="1221" name="Brainwashed Tursin Krall" nameId="1102391" quest_zone="Verteron" minlevel_permitted="13" max_repeat_count="5")"
	R"( cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="2720" exp="69750"><reward_item item_id="182200020" count="10"/>)"
	R"(</rewards><quest_kill step="0" var="0" count="3" npc_ids="216913" seq="0"/><start_conditions><acquired>1018</acquired></start_conditions>)"
	R"(</quest>)"
	// :7783-7790
	R"(<quest id="1853" name="[Weekly/Group] Officer Ousting" nameId="1104653" quest_zone="Reshanta" minlevel_permitted="40" rank="9")"
	R"( max_repeat_count="10" cannot_share="true" race_permitted="ELYOS" category="QUEST" repeat_cycle="WED">)"
	R"(<rewards exp="1768350" ap="1000"><reward_item item_id="188051888" count="1"/></rewards>)"
	R"(<start_conditions><finished quest_id="1719"/></start_conditions></quest>)"
	// :32819-32821
	R"(<quest id="9601" name="[Test] Limited Quest Mondays Only" nameId="1100512" quest_zone="Test zone" minlevel_permitted="10")"
	R"( max_repeat_count="255" race_permitted="ELYOS" category="QUEST" repeat_cycle="MON WED FRI"><rewards gold="9601" exp="9601"/></quest>)"
	R"(</quests>)";

/** npc_templates.xml:57842-57847 */
constexpr const char* PINKBEAK_AIRON_XML =
	R"(<npc_template npc_id="210668" level="4" name="pinkbeak airon" name_id="301027" height="2.02" group_drop="HIIV" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="BEAST" tribe="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2142" hpgauge="3")"
	R"( floatcorpse="true"><stats maxHp="383"><speeds walk="0.955" group_walk="0.955" run="6.806" run_fight="5.3" group_run_fight="6.806" />)"
	R"(</stats><bound_radius front="0.35" side="0.78" upper="2.02" /></npc_template>)";

constexpr int32_t INT_MAX_ = std::numeric_limits<int32_t>::max();
constexpr int32_t INT_MIN_ = std::numeric_limits<int32_t>::min();

class QuestStateSpawnTemplate final : public gameserver::model::templates::spawns::SpawnTemplate {
public:
	explicit QuestStateSpawnTemplate(gameserver::model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class QuestStateTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // the npc names the "aggressive" AI, which this executable does not link
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests([](gameserver::model::gameobjects::player::Player&) {
			return std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>>();
		});
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), QUESTS_XML));
	}

	void TearDown() override {
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		npcs.clear();
		spawnGroups.clear();
		InWorldPacketTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	gameserver::model::gameobjects::Npc& spawnNpc(const char* templateXml) {
		const gameserver::model::templates::npc::NpcTemplate* template_ =
			xml::bindString<gameserver::model::templates::npc::NpcTemplate>(contexts.emplace_back(), templateXml).release(); // static data
		Ref<gameserver::model::templates::spawns::SpawnGroup> group =
			gameserver::model::templates::spawns::SpawnGroup::create(210010000, template_->getTemplateId(), 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<QuestStateSpawnTemplate>(*group));
		Ref<gameserver::model::gameobjects::Npc> npc = gameserver::model::gameobjects::VisibleObject::create<gameserver::model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, template_);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	/** A state as PlayerQuestListDAO.load leaves it: stored, nothing to write */
	static Ref<QuestState> storedState(int32_t questId, QuestStatus status, int32_t completeCount) {
		Ref<QuestState> qs = QuestState::create(questId, status, 0, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(PersistentState::UPDATED);
		return qs;
	}

	Timestamp at(int64_t millis) const { return Timestamp(std::chrono::milliseconds(millis)); }

	std::deque<xml::LoadContext> contexts;
	std::vector<Ref<gameserver::model::gameobjects::Npc>> npcs;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnGroup>> spawnGroups;
};

std::vector<int32_t> varsOf(QuestVars& vars) {
	std::vector<int32_t> result;
	for (int32_t i = 0; i < 6; i++)
		result.push_back(vars.getVarById(i));
	return result;
}

// QuestVars(int) -> setVar (QuestVars.java:55-60): variable i is bits 6i..6i+5, and `var >>= 6` is Java's arithmetic shift, so the sign
// bit of a negative value fills variable 5; getQuestVars (:40-46) packs the six back into the same int. Java's values, per vector:
//   0x12345678 = 305419896:        [56, 25, 5, 13, 18, 0]    (0x78 & 0x3F = 56, 0x48D159 & 0x3F = 25, ...; bits 30-31 are 0)
//   1234567890 = 0x499602D2:       [18, 11, 32, 37, 9, 1]    (1234567890 >> 30 = 1)
//   -1 = 0xFFFFFFFF:               [63, 63, 63, 63, 63, 63]  (the shift keeps -1)
//   Integer.MIN_VALUE = 0x80000000: [0, 0, 0, 0, 0, 62]       (0x80000000 >> 30 = -2, -2 & 0x3F = 62; a logical shift would give 2)
//   -1234567890 = 0xB669FD2E:      [46, 52, 31, 26, 54, 62]
TEST_F(QuestStateTest, SetVarSplitsTheIntIntoSixBitVariablesWithJavasArithmeticShift) {
	struct Vector {
		int32_t packed;
		std::vector<int32_t> vars;
	};
	const std::vector<Vector> vectors = {
		{0x12345678, {56, 25, 5, 13, 18, 0}},
		{1234567890, {18, 11, 32, 37, 9, 1}},
		{-1, {63, 63, 63, 63, 63, 63}},
		{INT_MIN_, {0, 0, 0, 0, 0, 62}},
		{-1234567890, {46, 52, 31, 26, 54, 62}},
		{0, {0, 0, 0, 0, 0, 0}},
	};
	for (const Vector& v : vectors) {
		Ref<QuestVars> vars = QuestVars::create(v.packed);
		EXPECT_EQ(varsOf(*vars), v.vars) << v.packed;
		EXPECT_EQ(vars->getQuestVars(), v.packed) << "the six variables pack back into the int " << v.packed;

		Ref<QuestState> qs = QuestState::create(1101, QuestStatus::START, v.packed, 0, 0, std::nullopt, std::nullopt, std::nullopt);
		for (int32_t i = 0; i < 6; i++)
			EXPECT_EQ(qs->getQuestVarById(i), v.vars[static_cast<size_t>(i)]) << v.packed << ", variable " << i;
		EXPECT_EQ(qs->getQuestVars()->getQuestVars(), v.packed);
	}
}

// getQuestVars (QuestVars.java:40-46) over variables set one by one: `var <<= 6` drops what leaves the int and `var |= questVars[i]` ors the
// whole int. Java's values:
//   [1, 2, 3, 4, 5, 6]:   -2062536575  (6 << 30 leaves the int: 0x85... - the high bits of variable 5 are lost)
//   [0, 0, 0, 0, 0, 63]:  -1073741824  (63 << 30 = 0xC0000000)
//   [64, 0, 0, 0, 0, 0]:  64           (a value above 0x3F spills into variable 1's bits: the int reads back as variable 1 = 1)
//   [0, 0, -1, 0, 0, 0]:  -4096        (-1 << 6 << 6)
// setVarById (:31-35) warns for a value above 0x3F and stores it anyway; 0x3F itself and a negative value are stored without a warning.
TEST_F(QuestStateTest, GetQuestVarsPacksTheVariablesWithJavasIntOverflowAndSetVarByIdWarnsAbove0x3F) {
	network::test::LogCapture log({"com.aionemu.gameserver.questEngine.model.QuestVars"});
	Ref<QuestVars> vars = QuestVars::create();
	for (int32_t i = 0; i < 6; i++)
		vars->setVarById(i, i + 1);
	EXPECT_EQ(vars->getQuestVars(), -2062536575);

	Ref<QuestVars> high = QuestVars::create();
	high->setVarById(5, 63);
	EXPECT_EQ(high->getQuestVars(), -1073741824);
	EXPECT_FALSE(log.contains("Out of range")) << "0x3F is in range\n" << log.dump();

	Ref<QuestVars> spill = QuestVars::create();
	spill->setVarById(0, 64);
	EXPECT_EQ(spill->getVarById(0), 64) << "stored as given";
	EXPECT_EQ(spill->getQuestVars(), 64);
	EXPECT_EQ(varsOf(*QuestVars::create(spill->getQuestVars())), (std::vector<int32_t>{0, 1, 0, 0, 0, 0}));
	EXPECT_EQ(log.count("warning|com.aionemu.gameserver.questEngine.model.QuestVars|Out of range value was passed for quest var on index 0"), 1)
		<< log.dump();

	Ref<QuestVars> negative = QuestVars::create();
	negative->setVarById(2, -1);
	EXPECT_EQ(negative->getQuestVars(), -4096);
	EXPECT_EQ(log.count("Out of range"), 1) << "a negative value is not above 0x3F\n" << log.dump();
}

// questVars is a Java int[6]: an index outside [0, 6) throws ArrayIndexOutOfBoundsException - after setVarById's warning, which comes
// first (QuestVars.java:32-34)
TEST_F(QuestStateTest, AVariableIndexOutsideTheSixThrowsJavasArrayIndexExceptionAfterTheWarning) {
	network::test::LogCapture log({"com.aionemu.gameserver.questEngine.model.QuestVars"});
	Ref<QuestState> qs = QuestState::create(1101, QuestStatus::START);
	EXPECT_THROW(static_cast<void>(qs->getQuestVarById(6)), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_THROW(static_cast<void>(qs->getQuestVarById(-1)), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_THROW(qs->setQuestVarById(6, 1), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_FALSE(log.contains("Out of range"));
	EXPECT_THROW(qs->setQuestVarById(6, 64), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_EQ(log.count("Out of range value was passed for quest var on index 6"), 1) << log.dump();
	EXPECT_EQ(qs->getPersistentState(), PersistentState::NEW);
}

// setPersistentState (QuestState.java:138-154): DELETED on a NEW state (never stored) is NOACTION, on any other DELETED; UPDATE_REQUIRED on
// a NEW state keeps it NEW (the insert writes everything), on any other falls through to the default; every other value is taken as given
TEST_F(QuestStateTest, SetPersistentStateFollowsJavasSwitchWithItsFallthrough) {
	struct Row {
		PersistentState from;
		PersistentState requested;
		PersistentState to;
	};
	const std::vector<Row> rows = {
		{PersistentState::NEW, PersistentState::DELETED, PersistentState::NOACTION},
		{PersistentState::UPDATED, PersistentState::DELETED, PersistentState::DELETED},
		{PersistentState::UPDATE_REQUIRED, PersistentState::DELETED, PersistentState::DELETED},
		{PersistentState::NOACTION, PersistentState::DELETED, PersistentState::DELETED},
		{PersistentState::NEW, PersistentState::UPDATE_REQUIRED, PersistentState::NEW},
		{PersistentState::UPDATED, PersistentState::UPDATE_REQUIRED, PersistentState::UPDATE_REQUIRED},
		{PersistentState::NOACTION, PersistentState::UPDATE_REQUIRED, PersistentState::UPDATE_REQUIRED},
		{PersistentState::DELETED, PersistentState::UPDATE_REQUIRED, PersistentState::UPDATE_REQUIRED},
		{PersistentState::NEW, PersistentState::UPDATED, PersistentState::UPDATED},
		{PersistentState::NEW, PersistentState::NOACTION, PersistentState::NOACTION},
		{PersistentState::DELETED, PersistentState::NEW, PersistentState::NEW},
		{PersistentState::UPDATE_REQUIRED, PersistentState::UPDATED, PersistentState::UPDATED},
	};
	for (const Row& row : rows) {
		Ref<QuestState> qs = QuestState::create(1101, QuestStatus::START);
		ASSERT_EQ(qs->getPersistentState(), PersistentState::NEW) << "the constructor's state";
		// the way there: a stored state first (UPDATED is taken as given), since NEW itself turns DELETED into NOACTION and keeps
		// UPDATE_REQUIRED from happening
		if (row.from != PersistentState::NEW)
			qs->setPersistentState(PersistentState::UPDATED);
		if (row.from != PersistentState::NEW && row.from != PersistentState::UPDATED)
			qs->setPersistentState(row.from);
		ASSERT_EQ(qs->getPersistentState(), row.from);
		qs->setPersistentState(row.requested);
		EXPECT_EQ(qs->getPersistentState(), row.to) << static_cast<int>(row.from) << " + " << static_cast<int>(row.requested);
	}
}

// Every setter of QuestState.java asks for UPDATE_REQUIRED (:46-58, 64-75, 89-92, 102-105, 171-182), which a stored state takes and a NEW
// state ignores; setNextRepeatTime (:94-96) asks for nothing
TEST_F(QuestStateTest, EverySetterButSetNextRepeatTimeMarksAStoredStateForUpdate) {
	using Setter = void (*)(QuestState&);
	const std::vector<std::pair<const char*, Setter>> setters = {
		{"setQuestVarById", [](QuestState& qs) { qs.setQuestVarById(1, 5); }},
		{"setQuestVar", [](QuestState& qs) { qs.setQuestVar(3); }},
		{"setStatus", [](QuestState& qs) { qs.setStatus(QuestStatus::REWARD); }},
		{"setStatus(status, false)", [](QuestState& qs) { qs.setStatus(QuestStatus::COMPLETE, false); }},
		{"setCompleteCount", [](QuestState& qs) { qs.setCompleteCount(2); }},
		{"setRewardGroup", [](QuestState& qs) { qs.setRewardGroup(1); }},
		{"setRewardGroup(null)", [](QuestState& qs) { qs.setRewardGroup(std::nullopt); }},
		{"setFlags", [](QuestState& qs) { qs.setFlags(4); }},
		{"setStepGroup", [](QuestState& qs) { qs.setStepGroup(1); }},
	};
	for (const auto& [name, set] : setters) {
		Ref<QuestState> stored = storedState(1101, QuestStatus::START, 0);
		set(*stored);
		EXPECT_EQ(stored->getPersistentState(), PersistentState::UPDATE_REQUIRED) << name;
		Ref<QuestState> fresh = QuestState::create(1101, QuestStatus::START);
		set(*fresh);
		EXPECT_EQ(fresh->getPersistentState(), PersistentState::NEW) << name << " on a state never stored";
	}
	Ref<QuestState> stored = storedState(1101, QuestStatus::START, 0);
	stored->setNextRepeatTime(at(5000));
	EXPECT_EQ(stored->getPersistentState(), PersistentState::UPDATED) << "setNextRepeatTime";
	EXPECT_EQ(stored->getNextRepeatTime(), std::optional<Timestamp>(at(5000)));

	Ref<QuestState> vars = storedState(1101, QuestStatus::START, 0);
	vars->setQuestVarById(1, 5);
	vars->setQuestVarById(0, 2);
	EXPECT_EQ(vars->getQuestVars()->getQuestVars(), 2 + (5 << 6));
	vars->setQuestVar(7 << 12);
	EXPECT_EQ(varsOf(*vars->getQuestVars()), (std::vector<int32_t>{0, 0, 7, 0, 0, 0})) << "setQuestVar replaces all six";
	vars->setRewardGroup(3);
	EXPECT_EQ(vars->getRewardGroup(), std::optional<int32_t>(3));
	vars->setRewardGroup(std::nullopt);
	EXPECT_EQ(vars->getRewardGroup(), std::nullopt);
}

// setStatus (QuestState.java:68-75): the change to COMPLETE from another status counts one completion and takes the current time, unless the
// caller says not to (QuestService.abandonQuest's reset, :862); COMPLETE again counts nothing. The count is a Java int: it wraps. The
// constructor of a COMPLETE state (:37-40) has the count 1 and the current time.
TEST_F(QuestStateTest, SetStatusCountsACompletionOnceAndTakesTheClocksTime) {
	clock.setCurrentTimeMillis(1'700'000'000'000);
	Ref<QuestState> qs = QuestState::create(1221, QuestStatus::START);
	EXPECT_EQ(qs->getCompleteCount(), 0);
	EXPECT_EQ(qs->getLastCompleteTime(), std::nullopt);

	qs->setStatus(QuestStatus::REWARD);
	EXPECT_EQ(qs->getCompleteCount(), 0);
	qs->setStatus(QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getCompleteCount(), 1);
	EXPECT_EQ(qs->getLastCompleteTime(), std::optional<Timestamp>(at(1'700'000'000'000)));

	clock.setCurrentTimeMillis(1'700'000'100'000);
	qs->setStatus(QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getCompleteCount(), 1) << "COMPLETE to COMPLETE counts nothing";
	EXPECT_EQ(qs->getLastCompleteTime(), std::optional<Timestamp>(at(1'700'000'000'000)));

	qs->setStatus(QuestStatus::START);
	qs->setStatus(QuestStatus::COMPLETE, false);
	EXPECT_EQ(qs->getCompleteCount(), 1) << "updateCompleteCountAndTime false";
	EXPECT_EQ(qs->getLastCompleteTime(), std::optional<Timestamp>(at(1'700'000'000'000)));

	qs->setStatus(QuestStatus::START, true);
	qs->setStatus(QuestStatus::COMPLETE, true);
	EXPECT_EQ(qs->getCompleteCount(), 2);
	EXPECT_EQ(qs->getLastCompleteTime(), std::optional<Timestamp>(at(1'700'000'100'000)));

	qs->setCompleteCount(INT_MAX_);
	qs->setStatus(QuestStatus::START);
	qs->setStatus(QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getCompleteCount(), INT_MIN_) << "completeCount++ on a Java int";

	clock.setCurrentTimeMillis(1'700'000'200'000);
	Ref<QuestState> completed = QuestState::create(1221, QuestStatus::COMPLETE);
	EXPECT_EQ(completed->getCompleteCount(), 1);
	EXPECT_EQ(completed->getLastCompleteTime(), std::optional<Timestamp>(at(1'700'000'200'000)));
}

// canRepeat (QuestState.java:121-131): not while the count has reached max_repeat_count, unless that is 255 (unlimited); for a time-based
// quest (repeat_cycle) not before nextRepeatTime (Timestamp.before: strictly earlier). isStartable (:117-119) is COMPLETE && canRepeat, in
// that order, so a state that is not COMPLETE never reads the template.
TEST_F(QuestStateTest, CanRepeatComparesTheCountWithTheTemplateAndTheClockWithTheNextRepeatTime) {
	struct Row {
		int32_t questId;
		int32_t completeCount;
		bool canRepeat;
	};
	const std::vector<Row> counts = {
		{1101, 0, true}, {1101, 1, false}, {1221, 4, true}, {1221, 5, false}, {1221, 6, false},
		{9601, 254, true}, {9601, 255, true}, {9601, 1000, true}, // 255: no limit
		{1853, 9, true}, {1853, 10, false},                       // time-based, no next repeat time yet
	};
	for (const Row& row : counts) {
		Ref<QuestState> qs = storedState(row.questId, QuestStatus::COMPLETE, row.completeCount);
		EXPECT_EQ(qs->canRepeat(), row.canRepeat) << row.questId << " completed " << row.completeCount << " times";
		EXPECT_EQ(qs->isStartable(), row.canRepeat) << row.questId << " COMPLETE: isStartable is canRepeat";
	}

	clock.setCurrentTimeMillis(1'000'000);
	Ref<QuestState> timed = storedState(9601, QuestStatus::COMPLETE, 3);
	timed->setNextRepeatTime(at(1'000'001));
	EXPECT_FALSE(timed->canRepeat()) << "one millisecond before the next repeat time";
	EXPECT_FALSE(timed->isStartable());
	clock.setCurrentTimeMillis(1'000'001);
	EXPECT_TRUE(timed->canRepeat()) << "at the next repeat time: Timestamp.before is strict";
	clock.setCurrentTimeMillis(9'000'000);
	EXPECT_TRUE(timed->canRepeat());

	Ref<QuestState> weekly = storedState(1853, QuestStatus::COMPLETE, 10);
	weekly->setNextRepeatTime(at(0));
	EXPECT_FALSE(weekly->canRepeat()) << "the count is checked first: 10 of 10 even after the repeat time";

	Ref<QuestState> untimed = storedState(1221, QuestStatus::COMPLETE, 1);
	untimed->setNextRepeatTime(at(99'000'000));
	EXPECT_TRUE(untimed->canRepeat()) << "a quest without repeat_cycle ignores a next repeat time";

	for (QuestStatus status : {QuestStatus::START, QuestStatus::REWARD, QuestStatus::LOCKED}) {
		Ref<QuestState> active = storedState(1221, status, 0);
		EXPECT_TRUE(active->canRepeat());
		EXPECT_FALSE(active->isStartable()) << "not COMPLETE: " << static_cast<int>(status);
	}
}

// canRepeat reads DataManager.QUEST_DATA.getQuestById(questId) and dereferences it: NullPointerException for a quest without a template.
// isStartable asks the status first (&&), so only a COMPLETE state gets there.
TEST_F(QuestStateTest, CanRepeatOfAQuestWithoutATemplateThrowsJavasNullPointerExceptionButIsStartableAsksTheStatusFirst) {
	Ref<QuestState> started = storedState(4242, QuestStatus::START, 0);
	EXPECT_FALSE(started->isStartable());
	EXPECT_THROW(static_cast<void>(started->canRepeat()), runtime::NullPointerException);
	Ref<QuestState> completed = storedState(4242, QuestStatus::COMPLETE, 1);
	EXPECT_THROW(static_cast<void>(completed->isStartable()), runtime::NullPointerException);
}

// getStepGroup / setStepGroup (QuestState.java:176-182): the step group is the flags above their first six bits, read with Java's arithmetic
// `>>` and written with `<<`, which drops what leaves the int (0x04000001 << 6 = 64)
TEST_F(QuestStateTest, TheStepGroupIsTheFlagsAboveTheirFirstSixBits) {
	Ref<QuestState> qs = storedState(1101, QuestStatus::START, 0);
	qs->setStepGroup(3);
	EXPECT_EQ(qs->getFlags(), 192);
	EXPECT_EQ(qs->getStepGroup(), 3);
	qs->setFlags(0x3F);
	EXPECT_EQ(qs->getStepGroup(), 0);
	qs->setFlags(0x7F);
	EXPECT_EQ(qs->getStepGroup(), 1);
	qs->setFlags(-64);
	EXPECT_EQ(qs->getStepGroup(), -1) << "arithmetic shift";
	qs->setFlags(INT_MIN_);
	EXPECT_EQ(qs->getStepGroup(), -33554432);
	qs->setStepGroup(0x04000001);
	EXPECT_EQ(qs->getFlags(), 64) << "0x04000001 << 6 on a Java int";
	EXPECT_EQ(qs->getStepGroup(), 1);
}

// QuestEnv.getTargetId (QuestEnv.java:94-96): the template id of the object the player interacts with, 0 without one - what
// TalkEventHandler's USE_OBJECT (the npc), a kill (the npc) and CM_DIALOG_SELECT's journal branch (null) hand the handlers
TEST_F(QuestStateTest, GetTargetIdIsTheTemplateIdOfTheTargetOrZeroWithoutOne) {
	cp::PlayerFixture f = cp::makePlayer(800101, 9811, "Targeter");
	Ref<QuestEnv> journal = QuestEnv::create(nullptr, *f.player, 1101);
	EXPECT_EQ(journal->getTargetId(), 0);

	gameserver::model::gameobjects::Npc& npc = spawnNpc(PINKBEAK_AIRON_XML);
	Ref<QuestEnv> talk = QuestEnv::create(runtime::Ptr<gameserver::model::gameobjects::VisibleObject>(npc), *f.player, 0, 10);
	EXPECT_EQ(talk->getTargetId(), 210668);
	talk->setVisibleObject(nullptr);
	EXPECT_EQ(talk->getTargetId(), 0);
	journal->setVisibleObject(runtime::Ptr<gameserver::model::gameobjects::VisibleObject>(npc));
	EXPECT_EQ(journal->getTargetId(), 210668);
	f = {};
}

} // namespace
} // namespace aion::gameserver::questEngine::model::test
