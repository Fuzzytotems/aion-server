// D12-CP2b (owner, 2026-10-13: D12 = port): RoahCustomInstanceHandler (Java custom/instance) on a real instance of AscensionWorldTest's
// World and database. The handler is put on a Karamatis B instance through InstanceService's supplier, as CustomInstanceService.onEnter puts
// it on the Roah chamber (300070000 is not in this World; the handler reads its map only for the fly ring). The npc rows are the shipped ones
// (npc_templates.xml, verbatim, line beside each). The database is the fixture's (aion_gs_test_ascension): the rank rows are real.

#include "AscensionTestSupport.h"

#include "aion/commons/utils/TimeUtils.h"

#include <string>
#include <vector>

#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/instance/RoahCustomInstanceHandler.h"
#include "aion/gameserver/dao/CustomInstanceDAO.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::instance::test {
namespace {

using custom::instance::RoahCustomInstanceHandler;
using network::aion::serverpackets::SM_MESSAGE;
using network::aion::serverpackets::SM_QUEST_ACTION;
using services::instance::InstanceService;

/** npc_templates.xml:455037-455041, :174709-174712, :371721-371730, :152105-152121, :149463-149478 */
constexpr std::string_view ROAH_NPC_ROWS = R"x(
	<npc_template npc_id="730588" level="65" name="secret storeroom exit" name_id="372284" height="10" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="portal" srange="20" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="172" />
		<bound_radius front="5" side="1" upper="10" />
		<talk_info distance="6" delay="3" is_dialog="true" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="234029" level="65" name="floating pillar" name_id="466204" height="3.36" group_drop="SAM" rank="DISCIPLINED" rating="NORMAL" race="CONSTRUCT" tribe="AGGRESSIVESINGLEMONSTER" type="MONSTER" ai="noaction" srange="10" sangle="140" arange="37" attack_speed="2645" cast_speed="850" hpgauge="3">
		<stats maxHp="1200000" />
		<bound_radius front="1.2" side="1.2" upper="3.36" />
	</npc_template>
	<npc_template npc_id="282757" level="65" name="pet 110" name_id="355381" height="4.81" title_id="314413" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="LDF4A_CALYDON" ai="custom_instance_bulky" srange="70" sangle="240" arange="2" attack_speed="2100" cast_speed="1500" hpgauge="3" cancel_level="20">
		<stats maxHp="269056">
			<speeds walk="1.76" group_walk="1.76" run="7" run_fight="7" group_run_fight="7" />
		</stats>
		<equipment>
			<item>100900997</item>
		</equipment>
		<bound_radius front="2.05" side="4" upper="4.81" />
	</npc_template>
	<npc_template npc_id="231918" level="65" name="vigils garrison shieldcaptain" name_id="320098" height="2.4" title_id="334085" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="UNDEAD" tribe="MONSTER" ai="custom_instance_boss" srange="6" sangle="140" arange="2" attack_speed="2100" hpgauge="3" cancel_level="40">
		<stats maxHp="128881">
			<speeds walk="1.457" group_walk="1.457" run="6" run_fight="7" group_run_fight="6" />
		</stats>
		<equipment>
			<item>113601399</item>
			<item>110601454</item>
			<item>100001559</item>
			<item>115001600</item>
			<item>112601397</item>
			<item>111601416</item>
			<item>114601404</item>
			<item>125003525</item>
		</equipment>
		<bound_radius front="0.3" side="0.42" upper="2.4" />
	</npc_template>
	<npc_template npc_id="231717" level="65" name="support unit invader" name_id="320098" height="2" title_id="334085" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="UNDEAD" tribe="MONSTER" ai="custom_instance_boss" srange="70" sangle="140" arange="2" attack_speed="2100" hpgauge="3" cancel_level="40">
		<stats maxHp="123278">
			<speeds walk="1.457" group_walk="1.457" run="6" run_fight="7" group_run_fight="6" />
		</stats>
		<equipment>
			<item>113601272</item>
			<item>110601318</item>
			<item>100900976</item>
			<item>112601263</item>
			<item>111601283</item>
			<item>114601267</item>
			<item>125100146</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
	</npc_template>
</npc_templates>)x";

/** skill_templates.xml:27185-27208 (Word of Wind), :76092-76119 (Symphony of Destruction): the prebuffs the first entry looks for */
constexpr std::string_view ROAH_SKILL_ROWS = R"x(<skill_data>
	<skill_template skill_id="1810" name="Word of Wind" nameId="2286467" cooldownId="1547" group="CH_PROVIDENCE" stack="CH_PROVIDENCE" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="30" activation="ACTIVE" cooldown="18000" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true">
		<properties first_target="ME" first_target_range="1" target_relation="MYPARTY" target_type="PARTY" target_maxcount="6" effective_altitude="4" effective_range="20" />
		<startconditions>
			<dp value="2000" />
		</startconditions>
		<endconditions>
			<chargeweapon value="263" />
			<chargearmor value="281" />
			<polishchargeweapon value="2104" />
		</endconditions>
		<effects>
			<statup duration2="300000" effectid="113281" e="1" noresist="true" hoptype="SKILLLV" hopb="512">
				<change stat="SPEED" func="PERCENT" value="20" />
			</statup>
			<statup duration2="300000" effectid="113282" e="2" noresist="true" preeffect="1">
				<change stat="ATTACK_SPEED" func="PERCENT" value="-20" />
			</statup>
		</effects>
		<actions>
			<dpuse value="2000" />
		</actions>
		<motion name="chchant3" />
	</skill_template>
	<skill_template skill_id="4469" name="Symphony of Destruction" nameId="2286078" cooldownId="1292" group="BA_SONGOFDESTROY" stack="BA_SONGOFDESTROY" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="18000" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true">
		<properties first_target="ME" first_target_range="25" target_relation="MYPARTY" target_type="PARTY" target_maxcount="6" revision_distance="6" effective_altitude="25" effective_range="25" />
		<startconditions>
			<dp value="2000" />
			<weapon weapon="HARP" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<endconditions>
			<chargeweapon value="30" />
			<chargearmor value="20" />
			<polishchargeweapon value="238" />
		</endconditions>
		<effects>
			<statup duration2="300000" effectid="113281" e="1" noresist="true" hoptype="SKILLLV" hopb="212">
				<change stat="SPEED" func="PERCENT" value="10" />
				<change stat="ATTACK_SPEED" func="PERCENT" value="-10" />
			</statup>
			<boostskillcastingtime duration2="300000" effectid="944042" e="2" noresist="true" accmod2="500" preeffect="1">
				<change stat="BOOST_CASTING_TIME_SKILL" func="PERCENT" value="10" />
			</boostskillcastingtime>
		</effects>
		<actions>
			<dpuse value="2000" />
		</actions>
		<motion name="buff02" />
	</skill_template>
</skill_data>)x";

constexpr float INSIDE_X = 127.0f; // InstanceLifecycleTest's place inside Karamatis B
constexpr float INSIDE_Y = 203.0f;
constexpr float INSIDE_Z = 225.0f;

class RoahCustomInstanceHandlerTest : public AscensionWorldTest {
protected:
	void SetUp() override {
		AscensionWorldTest::SetUp();
		if (IsSkipped())
			return;
		std::string npcs = npcTemplatesXml();
		npcs.replace(npcs.rfind("</npc_templates>"), std::string_view("</npc_templates>").size(), ROAH_NPC_ROWS);
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(npcContext, ROAH_SKILL_ROWS));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, npcs));
		if (!dataholders::DataManager::QUEST_DATA) { // the npcs' deaths and deletions ask the quest engine: one row (quest_data.xml:65-67)
			dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(npcContext, R"x(<quests>
	<quest id="1006" name="Ascension" nameId="1102006" quest_zone="Ascension Quests" minlevel_permitted="9" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION">
		<rewards exp="73200"/>
	</quest>
</quests>)x"));
			questDataPublished = true;
		}
		spawnActor(POETA, 1, 657.0f, 1071.0f, 99.375f, int8_t{72});
		executeSql("DELETE FROM custom_instance");
		executeSql("DELETE FROM players WHERE id = " + std::to_string(actor.player->getObjectId()));
		// the rank row's foreign key (aion_gs.sql: custom_instance_ibfk_1 references players)
		executeSql("INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, exp) VALUES ("
			+ std::to_string(actor.player->getObjectId()) + ", 'Ascender', 9411, 'account9411', 1, 2, 3, 4, 210010000, 'MALE', 'ELYOS', 'WARRIOR', 0)");
	}

	void TearDown() override {
		AscensionWorldTest::TearDown();
		if (questDataPublished)
			dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	static void executeSql(const std::string& sql) {
		auto con = commons::database::DatabaseFactory::getConnection();
		con->executeSimple(sql);
	}

	/** CustomInstanceService.onEnter's creation (InstanceService.getNextAvailableInstance with RoahCustomInstanceHandler::new), then the
	 * player inside it; the handler's onEnterInstance runs as InstanceService.onEnterInstance runs it at the player's arrival (CM_LEVEL_READY's
	 * path, which this unit test does not drive) */
	runtime::Ptr<world::WorldMapInstance> enter() {
		runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{1},
			[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return RoahCustomInstanceHandler::create(created); },
			1, false);
		instance->register_(actor.player->getObjectId());
		services::teleport::TeleportService::teleportTo(*actor.player, *instance, INSIDE_X, INSIDE_Y, INSIDE_Z);
		world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		handlerOf(*instance).onEnterInstance(*actor.player);
		return instance;
	}

	static RoahCustomInstanceHandler& handlerOf(world::WorldMapInstance& instance) {
		return dynamic_cast<RoahCustomInstanceHandler&>(*instance.getInstanceHandler());
	}

	std::vector<uint8_t> centerMessage(const std::string& text) {
		return cptest::serialized(SM_MESSAGE(0, "", text, model::ChatType::BRIGHT_YELLOW_CENTER), client->con());
	}

	bool sentText(const std::string& text) { return wasSent(centerMessage(text)); }

	static std::vector<runtime::Ptr<model::gameobjects::Npc>> npcs(world::WorldMapInstance& instance, int32_t npcId) {
		return instance.getNpcs({npcId});
	}

	xml::LoadContext npcContext;
	bool questDataPublished = false;
};

/** RoahCustomInstanceHandler.java:77-92 and 223-237: the exit and the entry ring at creation; the welcome at the first entry with the rank of
 * the database (none: IRON), no unlockable npcs at rank 0, an empty skill set and no model without recorded entries */
TEST_F(RoahCustomInstanceHandlerTest, CreationAndTheFirstEntry) {
	runtime::Ptr<world::WorldMapInstance> instance = enter();
	ASSERT_TRUE(instance);
	EXPECT_EQ(npcs(*instance, 730588).size(), 1u) << "the exit";
	bool ring = false;
	instance->forEachObject([&ring](model::gameobjects::VisibleObject& object) { ring = ring || object.getName() == "ROAH_WING_1"; });
	EXPECT_TRUE(ring) << "the entry gate's fly ring";
	RoahCustomInstanceHandler& handler = handlerOf(*instance);
	EXPECT_TRUE(sentText("Welcome to the 'Eternal Challenge', " + custom::instance::getRankDescription(0) + " challenger!"));
	ASSERT_TRUE(handler.getSkillSet());
	EXPECT_EQ(handler.getSkillSet()->size(), 0);
	EXPECT_FALSE(handler.getPlayerModel()) << "no recorded entries: trainModelForPlayer answers null";
	EXPECT_FALSE(handler.allowInstanceRevive());
	EXPECT_FALSE(handler.allowSelfReviveByItem());
	EXPECT_FALSE(handler.allowSelfReviveBySkill());
	(*client)->clearSent();
	handler.onEnterInstance(*actor.player);
	EXPECT_FALSE(sentText("Welcome to the 'Eternal Challenge', " + custom::instance::getRankDescription(0) + " challenger!")) << "only once";
}

/** RoahCustomInstanceHandler.java:94-175, 273-299: the ring starts the run once - the artifact with the class's HP, the timer, the bulky
 * protector after 80 s; a death before the end lowers the rank (not below 0) and stores it */
TEST_F(RoahCustomInstanceHandlerTest, TheRingStartsTheRunAndADeathEndsIt) {
	runtime::Ptr<world::WorldMapInstance> instance = enter();
	RoahCustomInstanceHandler& handler = handlerOf(*instance);
	(*client)->clearSent();
	EXPECT_FALSE(handler.onPassFlyingRing(*actor.player, "ROAH_WING_2"));
	EXPECT_TRUE(npcs(*instance, 234029).empty());
	EXPECT_FALSE(handler.onPassFlyingRing(*actor.player, "ROAH_WING_1"));
	std::vector<runtime::Ptr<model::gameobjects::Npc>> artifacts = npcs(*instance, 234029);
	ASSERT_EQ(artifacts.size(), 1u);
	EXPECT_EQ(artifacts[0]->getLifeStats()->getMaxHp(), 300 * 4000) << "a warrior's 4000 average dps at rank 0";
	EXPECT_EQ(artifacts[0]->getLifeStats()->getCurrentHp(), 300 * 4000);
	EXPECT_TRUE(wasSent(cptest::serialized(SM_QUEST_ACTION(0, 900), client->con())));
	EXPECT_TRUE(sentText("You have 15 minutes to demolish the central Aether Stone. Beware for its protectors!"));
	EXPECT_FALSE(handler.onPassFlyingRing(*actor.player, "ROAH_WING_1"));
	EXPECT_EQ(npcs(*instance, 234029).size(), 1u) << "the start time is set once";

	executor().advance(std::chrono::milliseconds(80000));
	std::vector<runtime::Ptr<model::gameobjects::Npc>> bulkies = npcs(*instance, 282757);
	ASSERT_EQ(bulkies.size(), 1u) << "the bulky protector after 1:20";
	EXPECT_EQ(bulkies[0]->getLifeStats()->getMaxHp(), 10 * 4000);
	EXPECT_TRUE(sentText("A tough protector has appeared!"));

	(*client)->clearSent();
	EXPECT_TRUE(handler.onDie(*actor.player, *actor.player));
	EXPECT_TRUE(sentText("You shall not pass!"));
	EXPECT_TRUE(sentText("Your rank has been decreased to " + custom::instance::getRankDescription(0) + "."));
	EXPECT_TRUE(npcs(*instance, 234029).empty()) << "deleteAliveNpcs";
	std::optional<custom::instance::CustomInstanceRank> stored = dao::CustomInstanceDAO::loadPlayerRankObject(actor.player->getObjectId());
	ASSERT_TRUE(stored);
	EXPECT_EQ(stored->getRank(), 0);
	(*client)->clearSent();
	EXPECT_TRUE(handler.onDie(*actor.player, *actor.player));
	EXPECT_TRUE((*client)->sentBytes().empty()) << "completed: nothing more";
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(npcs(*instance, 282757).size(), 0u) << "the tasks are cancelled";
}

/** RoahCustomInstanceHandler.java:177-221, 301-331: the artifact's death brings the boss of the player's race and gender; the boss's death
 * raises the rank and stores it with the dps */
TEST_F(RoahCustomInstanceHandlerTest, TheArtifactThenTheBoss) {
	runtime::Ptr<world::WorldMapInstance> instance = enter();
	RoahCustomInstanceHandler& handler = handlerOf(*instance);
	const int64_t started = commons::utils::currentTimeMillis();
	handler.onPassFlyingRing(*actor.player, "ROAH_WING_1");
	runtime::Ptr<model::gameobjects::Npc> artifact = npcs(*instance, 234029).at(0);
	executor().advance(std::chrono::milliseconds(10000));
	(*client)->clearSent();
	handler.onDie(*artifact);
	EXPECT_TRUE(sentText("You feel a shadowy presence from the throne room."));
	const bool male = actor.player->getCommonData()->getGender() == model::Gender::MALE;
	std::vector<runtime::Ptr<model::gameobjects::Npc>> bosses = npcs(*instance, male ? 231918 : 231717);
	ASSERT_EQ(bosses.size(), 1u) << "an Elyos warrior's boss";

	(*client)->clearSent();
	handler.onDie(*bosses[0]);
	EXPECT_TRUE(sentText("Your rank has been increased to " + custom::instance::getRankDescription(1)
		+ ". Stay steadfast for tougher challenges and higher rewards!"));
	std::optional<custom::instance::CustomInstanceRank> stored = dao::CustomInstanceDAO::loadPlayerRankObject(actor.player->getObjectId());
	ASSERT_TRUE(stored);
	EXPECT_EQ(stored->getRank(), 1);
	EXPECT_EQ(stored->getMaxRank(), 1);
	EXPECT_GT(stored->getDps(), 0);
	EXPECT_GE(stored->getDps(), static_cast<int32_t>(300 * 4000 / ((commons::utils::currentTimeMillis() - started) / 1000.0f + 1.0f)))
		<< "the artifact's HP over the wall-clock time of the run (Java: System.currentTimeMillis)";
}

} // namespace
} // namespace aion::gameserver::instance::test
