// M5e C-01 (m5e-plan.md §5 stage 1, P5-08): ClassChangeService, all seven bodies. It is the whole of Java's simple class change
// (`gameserver.simple.secondclass.enable`, owner decision M5e D1 (a); m5e-plan.md §2.3 route S, steps S2-S8), and its setClass is the call the
// retail ascension handlers 1006 / 2008 make (`_1006Ascension.java:278-285`, route R).
//
// Java: ClassChangeService.java:23-164. The cases drive the service directly against the item packet fixture (tests/cm_ak/ItemPacketTestSupport.h:
// a spawned warrior in Poeta and a real AionConnection whose send queue the cases read), as DialogServiceTest does, with the player online and in
// the World as PlayerEnterWorldService leaves him: PlayerCommonData.updateDaeva reads his quest list only through getPlayer(), and loads it from the
// database otherwise (PlayerCommonData.java:596-600).
// - getSelectedPlayerClass: both races' eleven SELECT* actions (S6), the other race's actions, an action no page carries and a race that is
//   neither (null).
// - getClassSelectionDialogPageId: the six starting classes of both races and three advanced classes (0) (S3).
// - showClassChangeDialog: level 9 opens the page with the race's quest (S2); level 8 and an advanced class open nothing.
// - setClass: the validate table (the starting class id + 1 and + 2 accepted for every family; + 0, + 3, another family and a lower id refused
//   with "Invalid class chosen"; an advanced class refused with "You already switched class"); no validation; a null class; what an accepted
//   change does (the stats template, upgradePlayer, the animation to the player and his watcher, SM_PLAYER_INFO to the watcher only, the skills
//   of level 9 for both classes and none of level 10); updateDaevaStatus both ways.
// - setClass above level 9, as the console's Changeclass / Classup call it (validate false): the skills of every level from 9 to the player's,
//   and learnNewSkills' essence-tapping upgrade for a player who is a Daeva already.
// - completeAscensionQuest: a new state (ADD + UPDATE) and a held one (UPDATE only), for each race's quest.
// - changeClassToSelection: a selection of each race, an unknown and a refused one (the window closes whatever the result), and m5e-plan.md
//   D11, Java's missing level check (a level-1 Warrior becomes a level-1 Gladiator and a Daeva).
// Expected packets are Java's bytes (writeOP + the writeImpl fields; ServerPacketsOpcodes.java); SM_PLAYER_INFO is decoded field by field up to
// the class id. Every data row is the shipped data's, verbatim (file:line beside each).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../world/WorldTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/ClassChangeService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::PlayerClass;
using model::Race;
using model::gameobjects::Persistable;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using services::ClassChangeService;
namespace DialogAction = model::DialogAction;

// ServerPacketsOpcodes.java:42, :50, :62, :63, :78, :88, :142 (SM_STATS_INFO_OPCODE, :19, is the item fixture's)
constexpr int32_t SM_MESSAGE_OPCODE = 24;
constexpr int32_t SM_PLAYER_INFO_OPCODE = 32;
constexpr int32_t SM_SKILL_LIST_OPCODE = 44;
constexpr int32_t SM_SKILL_REMOVE_OPCODE = 45;
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
constexpr int32_t SM_ACTION_ANIMATION_OPCODE = 70;
constexpr int32_t SM_QUEST_ACTION_OPCODE = 124;

constexpr int32_t ELYOS_ASCENSION = 1006;
constexpr int32_t ASMODIAN_ASCENSION = 2008;

// SkillLearnService.java:70-73: human gathering, and the Daeva's essence tapping that replaces it from level 10
constexpr int32_t HUMAN_GATHERING = 30001;
constexpr int32_t ESSENCE_TAPPING = 30002;

/** quest_data.xml, verbatim rows: SM_QUEST_ACTION.writeImpl asks each quest's extra category (none here, so the packet is written) */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<!-- :65 -->
	<quest id="1006" name="Ascension" nameId="1102006" quest_zone="Ascension Quests" minlevel_permitted="9" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION">
		<rewards exp="73200"/>
	</quest>
	<!-- :9299 -->
	<quest id="2008" name="Ascension" nameId="1103108" quest_zone="Ascension Quests" minlevel_permitted="9" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ASMODIANS" category="MISSION">
		<rewards exp="73200"/>
	</quest>
</quests>)xml";

/**
 * skill_tree.xml, verbatim rows: two of the Gladiator's ten level-9 skills (44 learns from the fixture's sword skill 37, so it is no new
 * skill; 51 is), the Warrior's level-9 skill (a Gladiator below level 10 also learns his starting class's skills, SkillLearnService.java:
 * learnNewSkills) and two Warrior skills below level 9 (levels 8 and 5), which setClass's learnNewSkills(player, 9, level) must not reach,
 * and one of the Gladiator's level-10 skills, which it reaches only at level 10 or more
 */
constexpr std::string_view SKILL_TREE_XML = R"xml(<skill_tree>
	<!-- :56 -->
	<skill skillId="44" minLevel="9" autolearn="true" skillLearn="37" classId="GLADIATOR" />
	<!-- :82 -->
	<skill skillId="51" minLevel="9" autolearn="true" classId="GLADIATOR" />
	<!-- :143 -->
	<skill skillId="138" minLevel="9" autolearn="true" classId="WARRIOR" />
	<!-- :145 -->
	<skill skillId="139" minLevel="5" autolearn="true" classId="WARRIOR" />
	<!-- :309 -->
	<skill skillId="246" minLevel="10" autolearn="true" classId="GLADIATOR" />
	<!-- :2895 -->
	<skill skillId="2878" minLevel="8" autolearn="true" skillLearn="2877" classId="WARRIOR" />
</skill_tree>)xml";

/**
 * skill_templates.xml, verbatim rows of the skill tree's skills (SkillLearnService.onLearnSkill reads each learned skill's template), and of
 * human gathering and essence tapping (learnNewSkills' upgrade adds the one and removes the other, SkillLearnService.java:69-74)
 */
constexpr std::string_view CLASS_CHANGE_SKILLS_XML = R"xml(
	<!-- :812 -->
	<skill_template skill_id="44" name="Advanced Sword Training I" nameId="281829" group="P_EQUIP_ENHANCEDSWORD" stack="P_EQUIP_SWORD" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="ME" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<wpnmastery weapon="SWORD" effectid="101" e="1" basiclvl="2">
				<change stat="PHYSICAL_ATTACK" func="PERCENT" value="24" />
			</wpnmastery>
		</effects>
	</skill_template>
	<!-- :889 -->
	<skill_template skill_id="51" name="Advanced Greatsword Training I" nameId="281843" group="P_EQUIP_2HSWORD" stack="P_EQUIP_2HSWORD" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="ME" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<wpnmastery weapon="GREATSWORD" effectid="104" e="1" basiclvl="1">
				<change stat="PHYSICAL_ATTACK" func="PERCENT" value="4" />
			</wpnmastery>
		</effects>
	</skill_template>
	<!-- :1847 -->
	<skill_template skill_id="138" name="Boost Parry I" nameId="283274" group="P_STATBOOSTPARRY" stack="P_STATBOOSTPARRY" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="ME" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<statboost effectid="102011" e="1">
				<change stat="PARRY" func="ADD" value="80" />
			</statboost>
		</effects>
	</skill_template>
	<!-- :1858 -->
	<skill_template skill_id="139" name="Boost HP I" nameId="281959" group="P_STATBOOSTPHYSICALDEFENSE" stack="P_STATBOOSTPHYSICALDEFENSE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="ME" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<statboost effectid="101011" e="1">
				<change stat="MAXHP" func="PERCENT" value="3" />
			</statboost>
			<statboost effectid="101212" e="2" noresist="true" preeffect="1">
				<change stat="REGEN_HP" func="ADD" value="4" />
			</statboost>
		</effects>
	</skill_template>
	<!-- :3085 -->
	<skill_template skill_id="246" name="Herb Treatment" nameId="2288151" cooldownId="1153" group="MENDING" stack="MENDING" lvl="1" skilltype="PHYSICAL" skill_category="HEAL" skillsubtype="HEAL" tslot="NONE" activation="ACTIVE" cooldown="160" duration="4000" cancel_rate="100000" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="ME" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<healinstant value="281" e="1" noresist="true" element="WATER" hoptype="SKILLLV" hopb="744" />
		</effects>
		<actions>
			<itemuse itemid="169300003" count="1" />
		</actions>
		<motion name="mending2" />
	</skill_template>
	<!-- :48467 -->
	<skill_template skill_id="2878" name="Robust Blow" nameId="2287727" cooldownId="124" group="WA_ROBUSTBLOW" stack="WA_ROBUSTBLOW" lvl="2" skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="80" duration="0" cancel_rate="10" chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="TARGET" first_target_range="1" target_relation="ENEMY" target_type="ONLYONE" awr="true" />
		<startconditions>
			<weapon weapon="GREATSWORD DAGGER MACE POLEARM STAFF SWORD" />
			<chain category="W_CHAINA_2TH_1" precategory="W_CHAINA_1TH_1" time="3000" />
		</startconditions>
		<endconditions>
			<chargeweapon value="7" />
			<chargearmor value="7" />
			<polishchargeweapon value="50" />
		</endconditions>
		<effects>
			<skillatk value="59" e="1" accmod2="0" />
		</effects>
		<motion name="chainatk03" />
	</skill_template>
	<!-- :204322 -->
	<skill_template skill_id="30001" name="Collection" nameId="282931" stack="GATHERING_A" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<motion name="herb" />
	</skill_template>
	<!-- :204328 -->
	<skill_template skill_id="30002" name="Essencetapping" nameId="282933" stack="GATHERING_B" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<motion name="axe" />
	</skill_template>
)xml";

runtime::Ptr<model::house::House> noHouse(model::gameobjects::player::Player&) {
	return nullptr;
}

runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

/** SM_DIALOG_WINDOW.writeImpl (SM_DIALOG_WINDOW.java:29-41) of a page that is neither MAIL nor TOWN_CHALLENGE_TASK: D target, H page, D quest, H 0, H 0 */
std::vector<uint8_t> dialogWindow(int32_t targetObjectId, int32_t page, int32_t questId = 0) {
	return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(targetObjectId).H(page).D(questId).H(0).H(0));
}

/**
 * PacketSendUtility.sendMessage(player, msg) (PacketSendUtility.java:27-29) = SM_MESSAGE(0, null, msg, GOLDEN_YELLOW).writeImpl: C chat type 25
 * (ChatType.java:42), C sender race 0 (no sender), D sender 0, S null (the terminator only, BaseServerPacket.writeS), S msg
 */
std::vector<uint8_t> message(std::string_view text) {
	return javaPacket(SM_MESSAGE_OPCODE, PacketWriter().C(25).C(0).D(0).S("").S(text));
}

/** SM_ACTION_ANIMATION(objectId, CLASS_CHANGE, level).writeImpl: D target, H ActionAnimation.CLASS_CHANGE.getId() = 4 (ActionAnimation.java:17), D level */
std::vector<uint8_t> classChangeAnimation(int32_t objectId, int32_t level) {
	return javaPacket(SM_ACTION_ANIMATION_OPCODE, PacketWriter().D(objectId).H(4).D(level));
}

/**
 * SM_QUEST_ACTION(ADD, qs).writeImpl of a COMPLETE state with variables 0 and no flags: C ActionType.ADD.getId() 1, D quest, C QuestStatus.COMPLETE
 * 5 (QuestStatus.java), C 0, D step | flags << 24, H 0, C 0
 */
std::vector<uint8_t> questAdded(int32_t questId) {
	return javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(1).D(questId).C(5).C(0).D(0).H(0).C(0));
}

/** SM_QUEST_ACTION(UPDATE, qs).writeImpl of the same state: C 2, D quest, C 5, C 0, D 0, H 0 */
std::vector<uint8_t> questUpdated(int32_t questId) {
	return javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(2).D(questId).C(5).C(0).D(0).H(0));
}

/** The index of the first packet of `opcode` in `packets`, or packets.size() */
size_t firstOf(const std::vector<std::vector<uint8_t>>& packets, int32_t opcode) {
	for (size_t i = 0; i < packets.size(); ++i) {
		if (javaOpcodeOf(packets[i]) == opcode)
			return i;
	}
	return packets.size();
}

/** The last of `packets`, or no bytes when there is none */
std::vector<uint8_t> lastOf(const std::vector<std::vector<uint8_t>>& packets) {
	return packets.empty() ? std::vector<uint8_t>{} : packets.back();
}

/** The index of `packet` in `packets`, or packets.size() */
size_t indexOf(const std::vector<std::vector<uint8_t>>& packets, const std::vector<uint8_t>& packet) {
	return static_cast<size_t>(std::distance(packets.begin(), std::find(packets.begin(), packets.end(), packet)));
}

class ClassChangeServiceTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// The world holders are published once per process, P4-10's test set first (see DialogServiceTest.cpp's fixture)
		ASSERT_TRUE(world::test::publishTestStaticData()) << "this process published the real static data";
		ItemPacketTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, SKILL_TREE_XML));
		// the item fixture's skill rows plus the skill tree's (the base TearDown resets SKILL_DATA)
		std::string skillRows(SKILL_TEMPLATES_XML);
		skillRows.insert(skillRows.rfind("</skill_data>"), CLASS_CHANGE_SKILLS_XML);
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, skillRows));
		// the two reads of SM_PLAYER_INFO that need services this test has not got (HousingService loads from the database)
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
		serverpackets::detail::setPacketLookupsForTests(&lookups);
		// Java PlayerService.loadPlayer: the quest states (PlayerQuestListDAO.load: none for this character)
		questStates = model::gameobjects::player::QuestStateList::create();
		player().setQuestStateList(questStates);
	}

	void TearDown() override {
		if (watcher.player)
			watcher.player->setClientConnection(nullptr);
		watcherClient.reset();
		watcher = {};
		if (online) {
			world::World::getInstance().removeObject(*f.player);
			f.commonData->setOnline(false);
		}
		if (f.player)
			f.player->setQuestStateList(nullptr);
		questStates = nullptr;
		ItemPacketTest::TearDown();
		serverpackets::detail::setPacketLookupsForTests(nullptr);
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	/**
	 * The player as the DAO loads him and PlayerEnterWorldService leaves him: race, Daeva flag and level set offline while he still has the
	 * fixture's WARRIOR class (setExp's own updateDaeva then ends at the starting class, PlayerCommonData.java:592-593, instead of loading the
	 * quest list from the database), then his class, then online and in the World (PlayerCommonData.getPlayer finds him). Clears the queue.
	 */
	void prepare(Race race, PlayerClass playerClass, int32_t level, bool daeva = false) {
		f.commonData->setRace(race);
		f.commonData->setDaeva(daeva);
		f.commonData->setLevel(level);
		f.commonData->setPlayerClass(playerClass);
		f.commonData->setOnline(true);
		world::World::getInstance().storeObject(*f.player);
		online = true;
		ASSERT_EQ(player().getLevel(), level);
		ASSERT_EQ(player().getPlayerClass(), playerClass);
		clearSent();
	}

	/**
	 * A second character 3 m from the player, in his known list, with a connection of his own: broadcastPacket walks the player's known list
	 * (PacketSendUtility.java:98-100). Both queues are cleared afterwards (the player's see notification sends him the watcher's SM_PLAYER_INFO).
	 */
	void watch() {
		watcher = makePlayer(710102, 9902, "Watcher");
		// PlayerController.see sends the player the watcher's SM_MOTION too, which reads his motions (PlayerMotionDAO.loadMotionList: none)
		watcher.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*watcher.player));
		watcher.player->setPosition(
			world::WorldPosition::create(210010000, 103.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(103.0f, 100.0f, 50.0f)));
		watcher.player->getPosition()->setIsSpawned(true);
		watcherClient = std::make_unique<TestClient>();
		watcherClient->enterWorld(watcher);
		ASSERT_TRUE(f.knownList().addForTest(*watcher.player));
		ASSERT_EQ(knownSeeNotifiesFailed(), 0u) << "the player's see of the watcher ran to its end";
		clearSent();
		(*watcherClient)->clearSent();
	}

	/** A quest state as PlayerQuestListDAO loads it (stored), in the player's quest list */
	runtime::Ref<QuestState> hold(int32_t questId, QuestStatus status, int32_t questVars, std::optional<int32_t> reward) {
		runtime::Ref<QuestState> qs = QuestState::create(questId, status, questVars, 0, 0, std::nullopt, reward, std::nullopt);
		qs->setPersistentState(Persistable::PersistentState::UPDATED);
		questStates->addQuest(questId, *qs);
		return qs;
	}

	/** The skills as PlayerSkillListDAO.load gives them (PlayerSkillListDAO.java:44): the fixture's sword skill and human gathering at `points` */
	void holdHumanGathering(int32_t points) {
		player().setSkillList(model::skill::PlayerSkillList::create(
			{model::skill::PlayerSkillEntry::create(SWORD_SKILL, 1, 0, Persistable::PersistentState::UPDATED),
				model::skill::PlayerSkillEntry::create(HUMAN_GATHERING, points, 0, Persistable::PersistentState::UPDATED)}));
	}

	model::gameobjects::player::PlayerCommonData& commonData() { return *f.commonData; }

	bool knows(int32_t skillId) { return player().getSkillList()->isSkillPresent(skillId); }

	std::vector<std::vector<uint8_t>> watcherSent() { return (*watcherClient)->sentBytes(); }

	serverpackets::detail::PacketLookupsForTests lookups{};
	runtime::Ref<model::gameobjects::player::QuestStateList> questStates;
	PlayerFixture watcher;
	std::unique_ptr<TestClient> watcherClient;
	bool online = false;
};

// ---- getSelectedPlayerClass (ClassChangeService.java:109-164, m5e-plan.md §2.3 S6) ------------------------------------------------------------

struct Selection {
	int32_t dialogActionId;
	PlayerClass playerClass;
};

TEST_F(ClassChangeServiceTest, TheElyosPagesSelectTheElevenAdvancedClasses) {
	const Selection rows[] = {{DialogAction::SELECT5_1, PlayerClass::GLADIATOR}, {DialogAction::SELECT5_2, PlayerClass::TEMPLAR},
		{DialogAction::SELECT6_1, PlayerClass::ASSASSIN}, {DialogAction::SELECT6_2, PlayerClass::RANGER},
		{DialogAction::SELECT7_1, PlayerClass::SORCERER}, {DialogAction::SELECT7_2, PlayerClass::SPIRIT_MASTER},
		{DialogAction::SELECT8_1, PlayerClass::CLERIC}, {DialogAction::SELECT8_2, PlayerClass::CHANTER},
		{DialogAction::SELECT9_1, PlayerClass::GUNNER}, {DialogAction::SELECT9_2, PlayerClass::RIDER},
		{DialogAction::SELECT10_1, PlayerClass::BARD}};
	for (const Selection& row : rows)
		EXPECT_EQ(ClassChangeService::getSelectedPlayerClass(Race::ELYOS, row.dialogActionId), row.playerClass) << row.dialogActionId;

	// the actions only the Asmodian pages carry, and actions no class page carries, select nothing for an Elyos: the inner switch breaks out of
	// the ELYOS arm (no fall-through into the ASMODIANS arm) and the method returns null
	for (int32_t action : {DialogAction::SELECT10_2, DialogAction::SELECT8_3_1, DialogAction::SELECT8_3_2, DialogAction::SELECT9_3_1,
			 DialogAction::SELECT5_3, DialogAction::SELECT1, DialogAction::SETPRO4, 0})
		EXPECT_EQ(ClassChangeService::getSelectedPlayerClass(Race::ELYOS, action), std::nullopt) << action;
}

TEST_F(ClassChangeServiceTest, TheAsmodianPagesAreShiftedByOneAndCarryTheEngineersAndArtistsOnTheirOwnActions) {
	const Selection rows[] = {{DialogAction::SELECT7_1, PlayerClass::GLADIATOR}, {DialogAction::SELECT7_2, PlayerClass::TEMPLAR},
		{DialogAction::SELECT8_1, PlayerClass::ASSASSIN}, {DialogAction::SELECT8_2, PlayerClass::RANGER},
		{DialogAction::SELECT9_1, PlayerClass::SORCERER}, {DialogAction::SELECT9_2, PlayerClass::SPIRIT_MASTER},
		{DialogAction::SELECT10_1, PlayerClass::CLERIC}, {DialogAction::SELECT10_2, PlayerClass::CHANTER},
		{DialogAction::SELECT8_3_1, PlayerClass::GUNNER}, {DialogAction::SELECT8_3_2, PlayerClass::RIDER},
		{DialogAction::SELECT9_3_1, PlayerClass::BARD}};
	for (const Selection& row : rows)
		EXPECT_EQ(ClassChangeService::getSelectedPlayerClass(Race::ASMODIANS, row.dialogActionId), row.playerClass) << row.dialogActionId;

	for (int32_t action : {DialogAction::SELECT5_1, DialogAction::SELECT5_2, DialogAction::SELECT6_1, DialogAction::SELECT6_2,
			 DialogAction::SELECT7_3, DialogAction::SELECT1, DialogAction::SETPRO4, 0})
		EXPECT_EQ(ClassChangeService::getSelectedPlayerClass(Race::ASMODIANS, action), std::nullopt) << action;
}

TEST_F(ClassChangeServiceTest, ARaceThatIsNeitherSelectsNoClass) {
	// Java's outer switch has only the ELYOS and ASMODIANS arms
	for (int32_t action : {DialogAction::SELECT5_1, DialogAction::SELECT7_1, DialogAction::SELECT10_1})
		EXPECT_EQ(ClassChangeService::getSelectedPlayerClass(Race::LYCAN, action), std::nullopt) << action;
}

// ---- getClassSelectionDialogPageId (ClassChangeService.java:90-107, S3) ------------------------------------------------------------------------

TEST_F(ClassChangeServiceTest, TheSelectionPageFollowsTheRaceAndTheStartingClass) {
	struct Page {
		PlayerClass playerClass;
		int32_t elyos;
		int32_t asmodian;
	};
	const Page rows[] = {{PlayerClass::WARRIOR, 2375, 3057}, {PlayerClass::SCOUT, 2716, 3398}, {PlayerClass::MAGE, 3057, 3739},
		{PlayerClass::PRIEST, 3398, 4080}, {PlayerClass::ENGINEER, 3739, 3569}, {PlayerClass::ARTIST, 4080, 3910}};
	for (const Page& row : rows) {
		EXPECT_EQ(ClassChangeService::getClassSelectionDialogPageId(Race::ELYOS, row.playerClass), row.elyos);
		EXPECT_EQ(ClassChangeService::getClassSelectionDialogPageId(Race::ASMODIANS, row.playerClass), row.asmodian);
	}
	// an advanced class has no page (the default arm)
	for (PlayerClass advanced : {PlayerClass::GLADIATOR, PlayerClass::CHANTER, PlayerClass::BARD}) {
		EXPECT_EQ(ClassChangeService::getClassSelectionDialogPageId(Race::ELYOS, advanced), 0);
		EXPECT_EQ(ClassChangeService::getClassSelectionDialogPageId(Race::ASMODIANS, advanced), 0);
	}
}

// ---- showClassChangeDialog (ClassChangeService.java:23-29, S1-S2) ----------------------------------------------------------------------------

TEST_F(ClassChangeServiceTest, AnElyosWarriorOfLevelNineIsShownHisPageForQuest1006) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	ClassChangeService::showClassChangeDialog(player());

	EXPECT_EQ(sent(), exactly({dialogWindow(0, 2375, ELYOS_ASCENSION)}));
}

TEST_F(ClassChangeServiceTest, AnAsmodianScoutOfLevelNineIsShownHisPageForQuest2008) {
	prepare(Race::ASMODIANS, PlayerClass::SCOUT, 9);

	ClassChangeService::showClassChangeDialog(player());

	EXPECT_EQ(sent(), exactly({dialogWindow(0, 3398, ASMODIAN_ASCENSION)}));
}

TEST_F(ClassChangeServiceTest, BelowLevelNineNoPageOpens) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 8);

	ClassChangeService::showClassChangeDialog(player());

	EXPECT_TRUE(sent().empty());
}

TEST_F(ClassChangeServiceTest, AnAdvancedClassIsShownNoPage) {
	prepare(Race::ELYOS, PlayerClass::GLADIATOR, 9);

	ClassChangeService::showClassChangeDialog(player());

	EXPECT_TRUE(sent().empty());
}

// ---- setClass: the validation (ClassChangeService.java:55-66, S7) -----------------------------------------------------------------------------

struct Change {
	PlayerClass from;
	PlayerClass to;
};

TEST_F(ClassChangeServiceTest, ValidationAcceptsTheTwoClassesAboveEveryStartingClass) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);
	// the starting class id + 1 and + 2 (PlayerClass.java: ids 0-16); the Artist has only the Bard above him
	const Change rows[] = {{PlayerClass::WARRIOR, PlayerClass::GLADIATOR}, {PlayerClass::WARRIOR, PlayerClass::TEMPLAR},
		{PlayerClass::SCOUT, PlayerClass::ASSASSIN}, {PlayerClass::SCOUT, PlayerClass::RANGER}, {PlayerClass::MAGE, PlayerClass::SORCERER},
		{PlayerClass::MAGE, PlayerClass::SPIRIT_MASTER}, {PlayerClass::PRIEST, PlayerClass::CLERIC}, {PlayerClass::PRIEST, PlayerClass::CHANTER},
		{PlayerClass::ENGINEER, PlayerClass::RIDER}, {PlayerClass::ENGINEER, PlayerClass::GUNNER}, {PlayerClass::ARTIST, PlayerClass::BARD}};
	for (const Change& row : rows) {
		commonData().setPlayerClass(row.from);
		EXPECT_TRUE(ClassChangeService::setClass(player(), row.to)) << static_cast<int>(row.to);
		EXPECT_EQ(player().getPlayerClass(), row.to);
	}
	EXPECT_EQ(firstOf(sent(), SM_MESSAGE_OPCODE), sent().size()) << "an accepted class says nothing";
}

TEST_F(ClassChangeServiceTest, ValidationRefusesEveryOtherClassOfAStartingClassWithInvalidClassChosen) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);
	// + 0 (the same class), + 3 (the next family's starting class), + 4 (the next family's first class), a lower id, and the far ends
	const Change rows[] = {{PlayerClass::WARRIOR, PlayerClass::WARRIOR}, {PlayerClass::WARRIOR, PlayerClass::SCOUT},
		{PlayerClass::WARRIOR, PlayerClass::ASSASSIN}, {PlayerClass::SCOUT, PlayerClass::GLADIATOR}, {PlayerClass::SCOUT, PlayerClass::WARRIOR},
		{PlayerClass::MAGE, PlayerClass::PRIEST}, {PlayerClass::PRIEST, PlayerClass::SPIRIT_MASTER}, {PlayerClass::ENGINEER, PlayerClass::ARTIST},
		{PlayerClass::ARTIST, PlayerClass::GUNNER}, {PlayerClass::WARRIOR, PlayerClass::BARD}};
	for (const Change& row : rows) {
		commonData().setPlayerClass(row.from);
		clearSent();
		EXPECT_FALSE(ClassChangeService::setClass(player(), row.to)) << static_cast<int>(row.from) << " -> " << static_cast<int>(row.to);
		EXPECT_EQ(player().getPlayerClass(), row.from);
		EXPECT_EQ(sent(), exactly({message("Invalid class chosen")})) << static_cast<int>(row.from) << " -> " << static_cast<int>(row.to);
	}
}

TEST_F(ClassChangeServiceTest, AnAdvancedClassIsRefusedWithYouAlreadySwitchedClass) {
	prepare(Race::ELYOS, PlayerClass::GLADIATOR, 9);
	// the Templar is the Gladiator's id + 1: only the starting-class check refuses it
	for (PlayerClass target : {PlayerClass::TEMPLAR, PlayerClass::WARRIOR, PlayerClass::GLADIATOR}) {
		clearSent();
		EXPECT_FALSE(ClassChangeService::setClass(player(), target)) << static_cast<int>(target);
		EXPECT_EQ(player().getPlayerClass(), PlayerClass::GLADIATOR);
		EXPECT_EQ(sent(), exactly({message("You already switched class")})) << static_cast<int>(target);
	}
}

TEST_F(ClassChangeServiceTest, WithoutValidationAnyClassIsTaken) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	EXPECT_TRUE(ClassChangeService::setClass(player(), PlayerClass::BARD, false, false));

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::BARD);
	EXPECT_EQ(firstOf(sent(), SM_MESSAGE_OPCODE), sent().size());
}

TEST_F(ClassChangeServiceTest, NoClassChangesNothingAndSendsNothing) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	// Java: `if (newClass == null) return false;` before the validation, even with the Daeva update asked for
	EXPECT_FALSE(ClassChangeService::setClass(player(), std::nullopt, true, true));

	EXPECT_TRUE(sent().empty()) << "no validation message";
	EXPECT_FALSE(questStates->getQuestState(ELYOS_ASCENSION)) << "no ascension quest";
}

// ---- setClass: what an accepted class does (ClassChangeService.java:68-77) --------------------------------------------------------------------

TEST_F(ClassChangeServiceTest, TheNewClassGetsItsStatsTemplateAndItsFullHpAndMp) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);
	const model::templates::stats::StatsTemplate* gladiator = model::createStatsTemplate(PlayerClass::GLADIATOR, 9);
	runtime::Ptr<model::stats::container::PlayerLifeStats> lifeStats = player().getLifeStats();
	ASSERT_NE(player().getGameStats()->getStatsTemplate(), gladiator);
	const int32_t hpBefore = lifeStats->getCurrentHp(); // the fixture's level-1 Warrior's (the level was set offline, the template never followed)

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::GLADIATOR));

	// updateStatsTemplate: the interned template of the new class and the current level (PlayerGameStats.java: createStatsTemplate)
	EXPECT_EQ(player().getGameStats()->getStatsTemplate(), gladiator);
	// upgradePlayer: synchronizeWithMaxStats fills HP and MP to the new maximum (PlayerController.java:601-604)
	EXPECT_EQ(lifeStats->getCurrentHp(), lifeStats->getMaxHp());
	EXPECT_NE(lifeStats->getCurrentHp(), hpBefore) << "the level-9 Gladiator's maximum";
	EXPECT_EQ(lifeStats->getCurrentMp(), lifeStats->getMaxMp());
}

TEST_F(ClassChangeServiceTest, TheClassChangeIsAnimatedForAllButOnlyTheOthersAreSentThePlayerInfo) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);
	watch();

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::GLADIATOR));

	const std::vector<uint8_t> animation = classChangeAnimation(player().getObjectId(), 9);
	// the player: upgradePlayer's SM_STATS_INFO (updateStatsVisually), then the animation (broadcastPacket(player, ..., true)), and no
	// SM_PLAYER_INFO: Java's two-argument broadcastPacket(VisibleObject, packet) walks the known list only (PacketSendUtility.java:98-100)
	std::vector<std::vector<uint8_t>> packets = sent();
	size_t animationAt = indexOf(packets, animation);
	ASSERT_LT(animationAt, packets.size()) << ::testing::PrintToString(opcodesOf(packets));
	EXPECT_LT(firstOf(packets, SM_STATS_INFO_OPCODE), animationAt) << ::testing::PrintToString(opcodesOf(packets));
	EXPECT_EQ(packetsOf(packets, SM_ACTION_ANIMATION_OPCODE).size(), 1u);
	EXPECT_EQ(firstOf(packets, SM_PLAYER_INFO_OPCODE), packets.size()) << ::testing::PrintToString(opcodesOf(packets));

	// the watcher: the same animation, then the player's SM_PLAYER_INFO with the new class
	std::vector<std::vector<uint8_t>> seen = watcherSent();
	EXPECT_EQ(packetsOf(seen, SM_ACTION_ANIMATION_OPCODE), exactly({animation})) << ::testing::PrintToString(opcodesOf(seen));
	std::vector<std::vector<uint8_t>> infos = packetsOf(seen, SM_PLAYER_INFO_OPCODE);
	ASSERT_EQ(infos.size(), 1u) << ::testing::PrintToString(opcodesOf(seen));
	EXPECT_LT(indexOf(seen, animation), firstOf(seen, SM_PLAYER_INFO_OPCODE));
	// SM_PLAYER_INFO.writeImpl: F x, F y, F z, D object id, D template id, D robot id, D model id, C 0, D transform type, C 0x26, C race,
	// C class id (PlayerClass.GLADIATOR.getClassId() = 1)
	PacketReader info(bodyOf(infos[0]));
	info.B(4 + 4 + 4 + 4 + 4 + 4 + 4 + 1 + 4 + 1 + 1);
	EXPECT_EQ(info.C(), 1);
}

TEST_F(ClassChangeServiceTest, TheClassChangeLearnsTheSkillsOfLevelNineOfBothClassesAndNoneBelow) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);
	ASSERT_TRUE(knows(SWORD_SKILL));
	ASSERT_FALSE(knows(44));

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::GLADIATOR));

	// SkillLearnService.learnNewSkills(player, 9, 9): the Gladiator's level-9 rows and, below level 10, the starting class's (the Warrior's 138);
	// the Warrior's rows of levels 8 and 5 are below the call's first level, the Gladiator's level-10 row above its last
	EXPECT_TRUE(knows(44));
	EXPECT_TRUE(knows(51));
	EXPECT_TRUE(knows(138));
	EXPECT_FALSE(knows(2878));
	EXPECT_FALSE(knows(139));
	EXPECT_FALSE(knows(246)) << "the call ends at the player's level";
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(packetsOf(packets, SM_SKILL_LIST_OPCODE).size(), 3u) << ::testing::PrintToString(opcodesOf(packets));
	EXPECT_LT(indexOf(packets, classChangeAnimation(player().getObjectId(), 9)), firstOf(packets, SM_SKILL_LIST_OPCODE))
		<< "the skills are learned after the animation";
}

TEST_F(ClassChangeServiceTest, AConsoleChangeAboveLevelNineLearnsEveryLevelFromNineToHisOwn) {
	// consolecommands/Changeclass.java:27 and Classup.java:27 call setClass(player, class, false, true) at any level. A starting class above
	// level 9 is an ex-Daeva: Changeclass to a starting class keeps the level and clears the flag (ClassChangeService.java:84). This one holds
	// none of the tree's skills, so every row the call reaches is learned, and he holds human gathering.
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 10, true);
	commonData().setDaeva(false);
	holdHumanGathering(37);
	ASSERT_EQ(player().getLevel(), 10);
	ASSERT_FALSE(commonData().isDaeva());
	ASSERT_TRUE(knows(HUMAN_GATHERING));
	ASSERT_FALSE(knows(246));

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::GLADIATOR, false, true));

	// learnNewSkills(player, 9, 10): at level 10 the Gladiator's row only (the starting class's rows stop below level 10), at level 9 both
	// classes' rows, and nothing below 9
	EXPECT_TRUE(knows(246));
	EXPECT_TRUE(knows(44));
	EXPECT_TRUE(knows(51));
	EXPECT_TRUE(knows(138)) << "the Warrior's level-9 row: level 9 is below 10 whatever the player's level";
	EXPECT_FALSE(knows(2878));
	EXPECT_FALSE(knows(139));
	// learnNewSkills runs before the Daeva update (ClassChangeService.java:77-86), so the upgrade to essence tapping waits for a later call
	EXPECT_TRUE(knows(HUMAN_GATHERING));
	EXPECT_FALSE(knows(ESSENCE_TAPPING));
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(packetsOf(packets, SM_SKILL_LIST_OPCODE).size(), 4u) << ::testing::PrintToString(opcodesOf(packets));
	EXPECT_LT(indexOf(packets, classChangeAnimation(player().getObjectId(), 10)), packets.size()) << "the animation carries level 10";
	EXPECT_TRUE(commonData().isDaeva()) << "the Daeva update: an advanced class and 1006 COMPLETE";
}

TEST_F(ClassChangeServiceTest, AConsoleChangeOfADaevaAboveLevelNineUpgradesHumanGatheringToEssenceTapping) {
	// learnNewSkills' last arm (SkillLearnService.java:69-74) needs toLevel >= 10, the Daeva flag and human gathering. setClass runs it before
	// its own Daeva update, so only a player who is a Daeva already reaches it: Changeclass (validate false) making a Daeva Gladiator of level
	// 10 a Templar.
	prepare(Race::ELYOS, PlayerClass::GLADIATOR, 10, true);
	holdHumanGathering(37);
	ASSERT_TRUE(knows(HUMAN_GATHERING));
	ASSERT_FALSE(knows(ESSENCE_TAPPING));

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::TEMPLAR, false, true));

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::TEMPLAR);
	EXPECT_TRUE(knows(138)) << "the Templar's starting class's level-9 row";
	EXPECT_FALSE(knows(HUMAN_GATHERING)) << "removeSkill(player, 30001)";
	EXPECT_EQ(packetsOf(sent(), SM_SKILL_REMOVE_OPCODE).size(), 1u) << ::testing::PrintToString(opcodesOf(sent()));
	ASSERT_TRUE(knows(ESSENCE_TAPPING));
	EXPECT_EQ(player().getSkillList()->getSkillLevel(ESSENCE_TAPPING), 37) << "essence tapping takes human gathering's points";
}

// ---- setClass: the Daeva update (ClassChangeService.java:79-86) -------------------------------------------------------------------------------

TEST_F(ClassChangeServiceTest, TheDaevaUpdateCompletesQuest1006AndMakesTheElyosADaeva) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::GLADIATOR, true, true));

	EXPECT_TRUE(commonData().isDaeva()) << "PlayerCommonData.updateDaeva: an advanced class and 1006 COMPLETE";
	runtime::Ptr<QuestState> qs = questStates->getQuestState(ELYOS_ASCENSION);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_FALSE(questStates->getQuestState(ASMODIAN_ASCENSION));
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(packetsOf(packets, SM_QUEST_ACTION_OPCODE), exactly({questAdded(ELYOS_ASCENSION), questUpdated(ELYOS_ASCENSION)}));
	EXPECT_LT(firstOf(packets, SM_SKILL_LIST_OPCODE), firstOf(packets, SM_QUEST_ACTION_OPCODE)) << "the quest is completed after the skills";
}

TEST_F(ClassChangeServiceTest, TheDaevaUpdateCompletesQuest2008ForAnAsmodian) {
	prepare(Race::ASMODIANS, PlayerClass::SCOUT, 9);

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::RANGER, true, true));

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::RANGER);
	EXPECT_TRUE(commonData().isDaeva());
	runtime::Ptr<QuestState> qs = questStates->getQuestState(ASMODIAN_ASCENSION);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_FALSE(questStates->getQuestState(ELYOS_ASCENSION));
	EXPECT_EQ(packetsOf(sent(), SM_QUEST_ACTION_OPCODE), exactly({questAdded(ASMODIAN_ASCENSION), questUpdated(ASMODIAN_ASCENSION)}));
}

TEST_F(ClassChangeServiceTest, WithoutTheDaevaUpdateTheAscensionQuestIsLeftToTheQuest) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	// setClass(player, class) = setClass(player, class, validate true, updateDaevaStatus false): the retail handler's call, whose own quest
	// completion makes the Daeva (m5e-plan.md §2.3, route R)
	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::GLADIATOR));

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::GLADIATOR);
	EXPECT_FALSE(commonData().isDaeva());
	EXPECT_FALSE(questStates->getQuestState(ELYOS_ASCENSION));
	EXPECT_EQ(firstOf(sent(), SM_QUEST_ACTION_OPCODE), sent().size());
}

TEST_F(ClassChangeServiceTest, BackToAStartingClassTheDaevaStatusIsTakenAway) {
	prepare(Race::ELYOS, PlayerClass::GLADIATOR, 9, true);
	ASSERT_TRUE(commonData().isDaeva());

	ASSERT_TRUE(ClassChangeService::setClass(player(), PlayerClass::WARRIOR, false, true));

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::WARRIOR);
	EXPECT_FALSE(commonData().isDaeva());
	EXPECT_FALSE(questStates->getQuestState(ELYOS_ASCENSION)) << "the quest is completed only for an advanced class";
	EXPECT_EQ(firstOf(sent(), SM_QUEST_ACTION_OPCODE), sent().size());
}

// ---- completeAscensionQuest (ClassChangeService.java:36-49, S8) -------------------------------------------------------------------------------

TEST_F(ClassChangeServiceTest, WithoutAStateTheAscensionQuestIsAddedCompleteThenUpdated) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	ClassChangeService::completeAscensionQuest(player());

	runtime::Ptr<QuestState> qs = questStates->getQuestState(ELYOS_ASCENSION);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(qs->getRewardGroup(), std::optional<int32_t>(0));
	EXPECT_EQ(qs->getCompleteCount(), 1) << "new QuestState(id, COMPLETE) counts one completion";
	EXPECT_EQ(qs->getPersistentState(), Persistable::PersistentState::NEW) << "a new state stays NEW through its setters";
	EXPECT_FALSE(questStates->getQuestState(ASMODIAN_ASCENSION));
	EXPECT_EQ(sent(), exactly({questAdded(ELYOS_ASCENSION), questUpdated(ELYOS_ASCENSION)}));
}

TEST_F(ClassChangeServiceTest, AHeldStateIsCompletedResetAndOnlyUpdated) {
	prepare(Race::ASMODIANS, PlayerClass::SCOUT, 9);
	runtime::Ref<QuestState> held = hold(ASMODIAN_ASCENSION, QuestStatus::START, 3, 2);

	ClassChangeService::completeAscensionQuest(player());

	EXPECT_EQ(questStates->getQuestState(ASMODIAN_ASCENSION).get(), held.get()) << "the held state is kept, not replaced";
	EXPECT_EQ(held->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(held->getRewardGroup(), std::optional<int32_t>(0));
	EXPECT_EQ(held->getCompleteCount(), 1) << "setStatus(COMPLETE) counts the completion";
	EXPECT_EQ(held->getPersistentState(), Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_FALSE(questStates->getQuestState(ELYOS_ASCENSION));
	EXPECT_EQ(sent(), exactly({questUpdated(ASMODIAN_ASCENSION)}));
}

// ---- changeClassToSelection (ClassChangeService.java:31-34, S4-S5) ---------------------------------------------------------------------------

TEST_F(ClassChangeServiceTest, AnElyosSelectionChangesTheClassMakesADaevaAndClosesTheWindow) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	ClassChangeService::changeClassToSelection(player(), DialogAction::SELECT5_2);

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::TEMPLAR);
	EXPECT_TRUE(commonData().isDaeva());
	runtime::Ptr<QuestState> qs = questStates->getQuestState(ELYOS_ASCENSION);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(lastOf(packets), dialogWindow(0, 0)) << "SM_DIALOG_WINDOW(0, 0) closes the window last, after the class change's packets";
	EXPECT_EQ(packetsOf(packets, SM_DIALOG_WINDOW_OPCODE).size(), 1u);
}

TEST_F(ClassChangeServiceTest, AnAsmodianSelectionReadsTheAsmodianPages) {
	prepare(Race::ASMODIANS, PlayerClass::PRIEST, 9);

	ClassChangeService::changeClassToSelection(player(), DialogAction::SELECT10_2);

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::CHANTER);
	EXPECT_TRUE(commonData().isDaeva());
	ASSERT_TRUE(questStates->getQuestState(ASMODIAN_ASCENSION));
	EXPECT_EQ(lastOf(sent()), dialogWindow(0, 0));
}

TEST_F(ClassChangeServiceTest, AnUnknownSelectionOnlyClosesTheWindow) {
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 9);

	// SELECT10_2 is an Asmodian page's action: null for an Elyos, so setClass returns false at once
	ClassChangeService::changeClassToSelection(player(), DialogAction::SELECT10_2);

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::WARRIOR);
	EXPECT_FALSE(commonData().isDaeva());
	EXPECT_FALSE(questStates->getQuestState(ELYOS_ASCENSION));
	EXPECT_EQ(sent(), exactly({dialogWindow(0, 0)}));
}

TEST_F(ClassChangeServiceTest, ARefusedSelectionSaysSoAndClosesTheWindow) {
	prepare(Race::ASMODIANS, PlayerClass::SCOUT, 9);

	// the Asmodian SELECT7_1 is the Gladiator, not a class of the Scout
	ClassChangeService::changeClassToSelection(player(), DialogAction::SELECT7_1);

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::SCOUT);
	EXPECT_FALSE(commonData().isDaeva());
	EXPECT_FALSE(questStates->getQuestState(ASMODIAN_ASCENSION));
	EXPECT_EQ(sent(), exactly({message("Invalid class chosen"), dialogWindow(0, 0)}));
}

TEST_F(ClassChangeServiceTest, D11TheSimpleRouteChecksNoLevelSoALevelOneWarriorBecomesAGladiatorDaeva) {
	// m5e-plan.md D11 (docs/deviations/P5-08.md): only showClassChangeDialog checks level 9 (ClassChangeService.java:26); a client that sends
	// CM_DIALOG_SELECT(0, SELECT5_1, ..., 1006) at level 1 while the key is on is changed all the same. Java's behaviour, kept.
	prepare(Race::ELYOS, PlayerClass::WARRIOR, 1);

	ClassChangeService::changeClassToSelection(player(), DialogAction::SELECT5_1);

	EXPECT_EQ(player().getPlayerClass(), PlayerClass::GLADIATOR);
	EXPECT_TRUE(commonData().isDaeva());
	ASSERT_TRUE(questStates->getQuestState(ELYOS_ASCENSION));
	EXPECT_EQ(questStates->getQuestState(ELYOS_ASCENSION)->getStatus(), QuestStatus::COMPLETE);
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_LT(indexOf(packets, classChangeAnimation(player().getObjectId(), 1)), packets.size()) << "the animation carries level 1";
	// learnNewSkills(player, 9, 1) runs its loop from level 1 down to 9: no level at all
	EXPECT_EQ(packetsOf(packets, SM_SKILL_LIST_OPCODE).size(), 0u);
	EXPECT_FALSE(knows(44));
	EXPECT_EQ(lastOf(packets), dialogWindow(0, 0));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
