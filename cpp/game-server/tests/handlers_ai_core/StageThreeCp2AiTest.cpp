// M5j stage 3 CP2 (m5j-plan.md §18.4, item N-01): ChestAI, HiddenTeleportNpcAI, SummonerAI, SkillCooltimeResetAI and the four
// ConquestOffering AIs (P5-05). As StageThreeAiTest: npcs of npc_templates.xml's 798100 row (copied under the npc ids the AIs switch on) beside
// the item fixture's player in a Poeta map instance. SkillCooltimeResetAI's GeoService.canSee is checked against a generated geo fixture in the
// format of tests/geo/GeoWorldLoaderFilesTest.cpp (the writers of tests/handlers_ai_core/AbyssGuardGeoTest.cpp).
//
// Java: data/handlers/ai/{ChestAI,HiddenTeleportNpcAI,SummonerAI,SkillCooltimeResetAI,ConquestOffering*AI}.java.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/AIData.bind.h"
#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/ChestData.bind.h"
#include "aion/gameserver/dataholders/ChestData.h"
#include "aion/gameserver/dataholders/FlyPathData.bind.h"
#include "aion/gameserver/dataholders/FlyPathData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/handlers/ai/ChestAI.h"
#include "aion/gameserver/handlers/ai/ConquestOfferingAggressiveAI.h"
#include "aion/gameserver/handlers/ai/ConquestOfferingBuffNpcAI.h"
#include "aion/gameserver/handlers/ai/ConquestOfferingSpawnerAI.h"
#include "aion/gameserver/handlers/ai/HiddenTeleportNpcAI.h"
#include "aion/gameserver/handlers/ai/SkillCooltimeResetAI.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"
#include "aion/gameserver/model/templates/ai/SummonGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;

using gameserver::ai::AIState;
using model::gameobjects::Npc;
using model::gameobjects::state::CreatureState;

constexpr int32_t NPC = 798100;
constexpr int32_t HIDDEN_TELEPORTER = 804811;      // getTeleportId 279
constexpr int32_t LAST_HIDDEN_TELEPORTER = 804825; // getTeleportId 285
constexpr int32_t KEYLESS_CHEST = 804812;
constexpr int32_t INGGISON_PORTAL = 833018;

/** npc_templates.xml:462252-462258, the row of 798100; the cases copy it under the npc ids the AIs switch on */
constexpr std::string_view NPC_ROW =
	R"(<npc_template npc_id="798100" level="15" name="zephyr deliveryman" name_id="350579" height="1.16875" group_drop="NONE" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="deliveryman" srange="20" sangle="240" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" )"
	R"(group_run_fight="4.23" /></stats><bound_radius front="0.595" side="0.3774" upper="1.16875" /><talk_info distance="5" )"
	R"(can_talk_invisible="false" /></npc_template>)";

std::string npcTemplatesXml() {
	std::string xml = "<npc_templates>";
	for (int32_t npcId : {NPC, HIDDEN_TELEPORTER, LAST_HIDDEN_TELEPORTER, KEYLESS_CHEST, INGGISON_PORTAL}) {
		std::string row(NPC_ROW);
		row.replace(row.find("798100"), 6, std::to_string(npcId));
		xml += row;
	}
	return xml + "</npc_templates>";
}

/** fly_path.xml shape (TravelTestSupport's rows): the two paths of the hidden teleporters the cases use */
constexpr const char* FLYPATH_XML = R"xml(<flypath_template>
	<flypath_location id="279" sx="100" sy="100" sz="50" sworld="210010000" ex="400" ey="400" ez="60" eworld="210010000" time="20"/>
	<flypath_location id="285" sx="100" sy="100" sz="50" sworld="210010000" ex="300" ey="300" ez="60" eworld="210010000" time="20"/>
</flypath_template>)xml";

/** chest_templates.xml shape: 798100 takes three keys of two kinds (the fixture's juice and potion rows), 804812 none (item id 0) */
constexpr const char* CHEST_XML = R"xml(<chest_templates>
	<chest npc_id="798100"><key_item item_ids="160000001 162000002" count="3"/></chest>
	<chest npc_id="804812"><key_item item_ids="0" count="1"/></chest>
</chest_templates>)xml";

/** the fixture's skill rows plus the ones SkillCooltimeResetAI reads (skill_templates.xml's attributes, test values) */
constexpr std::string_view EXTRA_SKILL_ROWS = R"xml(
	<skill_template skill_id="37" name="Resettable" nameId="1" stack="TEST_RESETTABLE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="3060" duration="0"/>
	<skill_template skill_id="38" name="Avatar" nameId="2" stack="TEST_AVATAR" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="10" duration="0" avatar="true"/>
	<skill_template skill_id="39" name="Long" nameId="3" stack="TEST_LONG" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="3061" duration="0"/>
	<skill_template skill_id="40" name="Shared" nameId="4" stack="TEST_SHARED" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="10" duration="0" cooldownId="400"/>
	<skill_template skill_id="9889" name="Potion" nameId="5" stack="ITEM_POTION_HP_10" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE" cooldown="0" duration="0"/>
	<skill_template skill_id="9890" name="Buff" nameId="6" stack="TEST_SCROLL" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"/>
	<skill_template skill_id="9891" name="Arena Potion" nameId="7" stack="ITEM_ARENA_POTION_HP" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE" cooldown="0" duration="0"/>
</skill_data>)xml";

/** item_templates.xml shape: a buff scroll, an arena potion and a usable without a delay id beside the fixture's juice and potion */
constexpr std::string_view EXTRA_ITEM_ROWS = R"xml(
	<item_template id="164000001" name="Test Scroll" level="1" cName="scroll_test" mask="12414" max_stack_count="1000" quality="COMMON" price="5" desc="1" activate_target="STANDALONE" activate_count="1">
		<actions><skilluse level="1" skillid="9890"/></actions>
		<uselimits usedelay="60000" usedelayid="31"/>
	</item_template>
	<item_template id="164000002" name="Test Arena Potion" level="1" cName="arena_test" mask="12414" max_stack_count="1000" quality="COMMON" price="5" desc="1" activate_target="STANDALONE" activate_count="1">
		<actions><skilluse level="1" skillid="9891"/></actions>
		<uselimits usedelay="60000" usedelayid="41"/>
	</item_template>
	<item_template id="164000003" name="Test No Delay Id" level="1" cName="nodelay_test" mask="12414" max_stack_count="1000" quality="COMMON" price="5" desc="1" activate_target="STANDALONE" activate_count="1">
		<actions><skilluse level="1" skillid="9890"/></actions>
		<uselimits usedelay="60000"/>
	</item_template>
</item_templates>)xml";

constexpr int32_t TEST_SCROLL = 164000001;
constexpr int32_t TEST_ARENA_POTION = 164000002;
constexpr int32_t TEST_NO_DELAY_ID = 164000003;
constexpr int32_t MINOR_LIFE_POTION = 162000002;

class NpcSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	NpcSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z, int32_t creatorId)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, creatorId, std::nullopt) {}
};

// ---- geo files (GeoWorldLoader.java's formats; the writers of AbyssGuardGeoTest.cpp) ---------------------------------------------------------

class BigEndianWriter {
public:
	std::vector<uint8_t> bytes;

	BigEndianWriter& u8(uint8_t value) {
		bytes.push_back(value);
		return *this;
	}
	BigEndianWriter& i16(int16_t value) {
		bytes.push_back(static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8));
		bytes.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	BigEndianWriter& u32(uint32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8)
			bytes.push_back(static_cast<uint8_t>(value >> shift));
		return *this;
	}
	BigEndianWriter& f32(float value) { return u32(std::bit_cast<uint32_t>(value)); }
	BigEndianWriter& name(std::string_view text) {
		i16(static_cast<int16_t>(text.size()));
		bytes.insert(bytes.end(), text.begin(), text.end());
		return *this;
	}
};

/** A models.mesh entry: a vertical PHYSICAL quad in the y-z plane (x 0), y -3..3, z -5..15 (CollisionIntention.PHYSICAL blocks the sight) */
void writeWall(BigEndianWriter& out, std::string_view name) {
	const float vertices[] = {0, -3, -5, 0, 3, -5, 0, -3, 15, 0, 3, 15};
	const int8_t indices[] = {0, 1, 2, 1, 3, 2};
	out.name(name).u8(1);
	out.i16(4);
	for (float value : vertices)
		out.f32(value);
	out.i16(2).u8(1);
	for (int8_t index : indices)
		out.u8(static_cast<uint8_t>(index));
	out.u8(0).u8(1); // material 0, PHYSICAL
}

/** A .geo placement: the model's name, its location, the identity rotation, scale 1, no despawnable type */
void writePlacement(BigEndianWriter& out, std::string_view name, float x, float y, float z) {
	out.name(name).f32(x).f32(y).f32(z);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			out.f32(i == j ? 1.0f : 0.0f);
	out.f32(1).f32(1).f32(1).u8(0).i16(0).u8(0);
}

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// ---- the probes: each AI with its protected hooks callable -------------------------------------------------------------------------------

struct HiddenTeleportProbe final : roots::HiddenTeleportNpcAI {
	using HiddenTeleportNpcAI::HiddenTeleportNpcAI;
	using HiddenTeleportNpcAI::handleDialogStart;
};
struct ChestProbe final : roots::ChestAI {
	using ChestAI::ChestAI;
	using ChestAI::handleDialogStart;
	using ChestAI::handleUseItemFinish;
};
struct SummonerProbe final : roots::SummonerAI {
	using SummonerAI::SummonerAI;
	using SummonerAI::addHelpersSpawn;
	using SummonerAI::handleAttack;
	using SummonerAI::handleBackHome;
	using SummonerAI::handleDespawned;
	using SummonerAI::handleDied;
	using SummonerAI::handleNotAtHome;
	using SummonerAI::handleSpawned;

	bool recordSpawns = true;
	bool allowSpawn = true;
	std::vector<int32_t> spawned;     // spawnHelpers' summon groups (npc ids)
	std::vector<int32_t> befores;     // handleBeforeSpawn's percentages
	std::vector<int32_t> individuals; // handleIndividualSpawnedSummons' percentages
	int32_t checks = 0;
	int32_t finished = 0;

	void spawnHelpers(const model::templates::ai::SummonGroup& summonGroup) override {
		if (recordSpawns)
			spawned.push_back(summonGroup.getNpcId());
		else
			SummonerAI::spawnHelpers(summonGroup);
	}
	bool checkBeforeSpawn() override {
		checks++;
		return allowSpawn;
	}
	void handleBeforeSpawn(const model::templates::ai::Percentage& percent) override { befores.push_back(percent.getPercent()); }
	void handleSpawnFinished(const model::templates::ai::SummonGroup& /*summonGroup*/) override { finished++; }
	void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) override { individuals.push_back(percent.getPercent()); }
};
struct CooltimeResetProbe final : roots::SkillCooltimeResetAI {
	using SkillCooltimeResetAI::SkillCooltimeResetAI;
	using SkillCooltimeResetAI::handleDialogStart;
	using SkillCooltimeResetAI::handleSpawned;
};
struct SpawnerProbe final : roots::ConquestOfferingSpawnerAI {
	using ConquestOfferingSpawnerAI::ConquestOfferingSpawnerAI;
	using ConquestOfferingSpawnerAI::handleCustomEvent;
	using ConquestOfferingSpawnerAI::handleDespawned;
};
struct ConquestAggressiveProbe final : roots::ConquestOfferingAggressiveAI {
	using ConquestOfferingAggressiveAI::ConquestOfferingAggressiveAI;
	using ConquestOfferingAggressiveAI::handleDied;
	using ConquestOfferingAggressiveAI::handleSpawned;
};
struct BuffNpcProbe final : roots::ConquestOfferingBuffNpcAI {
	using ConquestOfferingBuffNpcAI::ConquestOfferingBuffNpcAI;
	using ConquestOfferingBuffNpcAI::handleDespawned;
	using ConquestOfferingBuffNpcAI::handleSpawned;
	using ConquestOfferingBuffNpcAI::handleUseItemFinish;
};

class StageThreeCp2AiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // the empty AI registry of this executable: the test installs the AI
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, npcTemplatesXml()));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, gameserver::ai::testing::AI_TRIBE_RELATIONS_XML));
		std::string itemsXml(ITEM_TEMPLATES_XML);
		itemsXml.replace(itemsXml.rfind("</item_templates>"), std::string_view("</item_templates>").size(), EXTRA_ITEM_ROWS);
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext, itemsXml));
		std::string skillsXml(SKILL_TEMPLATES_XML);
		skillsXml.replace(skillsXml.rfind("</skill_data>"), std::string_view("</skill_data>").size(), EXTRA_SKILL_ROWS);
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skillsXml));
		player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		world::World::getInstance().storeObject(player());
		clearSent();
	}

	void TearDown() override {
		for (const runtime::Ref<Npc>& npc : npcs)
			npc->getController().cancelAllTasks();
		player().setTarget(nullptr);
		mapInstance->removeObject(player());
		world::World::getInstance().removeObject(player());
		for (const runtime::Ref<Npc>& npc : npcs) {
			mapInstance->removeObject(*npc);
			world::World::getInstance().removeObject(*npc);
		}
		npcs.clear();
		groups.clear();
		dataholders::DataManager::AI_DATA.resetForTests();
		dataholders::DataManager::CHEST_DATA.resetForTests();
		dataholders::DataManager::FLY_PATH.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** an npc of the template at (x, y, 50) with the AI `A` installed and idle */
	template <class A>
	Npc& npc(float x = 102.0f, int32_t npcId = NPC, float y = 100.0f, int32_t creatorId = 0) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn =
			group->addSpawnTemplate(std::make_unique<NpcSpawnTemplate>(*group, x, y, 50.0f, creatorId));
		runtime::Ref<Npc> created = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		created->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*created));
		created->setEffectController(std::make_unique<controllers::effect::EffectController>(*created));
		created->setPosition(world::WorldPosition::create(210010000, x, y, 50.0f, int8_t{0}, mapInstance->getRegion(x, y, 50.0f)));
		created->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<A>(*created);
		ai->setStateIfNot(AIState::IDLE);
		created->replaceAi(std::move(ai));
		groups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	template <class A>
	static A& aiOf(Npc& npc) {
		return dynamic_cast<A&>(npc.getAi());
	}

	size_t count(const std::vector<uint8_t>& packet) {
		size_t n = 0;
		for (const std::vector<uint8_t>& bytes : sent())
			n += bytes == packet ? 1 : 0;
		return n;
	}

	std::vector<uint8_t> npcSays(Npc& npc, std::string_view text) {
		return serializedFor(serverpackets::SM_MESSAGE(npc, text, model::ChatType::NPC));
	}

	xml::LoadContext context;
	xml::LoadContext itemContext;
	xml::LoadContext skillContext;
	xml::LoadContext dataContext;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups;
	std::vector<runtime::Ref<Npc>> npcs;
};

// ---- HiddenTeleportNpcAI -----------------------------------------------------------------------------------------------------------------

/** HiddenTeleportNpcAI.java:27-52: page 1011; SETPRO1 starts the npc's fly teleport (path id * 1000 + 1); every choice closes the dialog */
TEST_F(StageThreeCp2AiTest, HiddenTeleporterStartsItsFlyTeleport) {
	dataholders::DataManager::FLY_PATH.publish(xml::bindString<dataholders::FlyPathData>(dataContext, FLYPATH_XML));
	Npc& teleporter = npc<HiddenTeleportProbe>(102.0f, HIDDEN_TELEPORTER);
	aiOf<HiddenTeleportProbe>(teleporter).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_DIALOG_WINDOW(teleporter.getObjectId(), 1011))}));

	clearSent();
	EXPECT_TRUE(teleporter.getAi().onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_FALSE(player().isInState(CreatureState::FLYING)) << "only SETPRO1 teleports";
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_DIALOG_WINDOW(teleporter.getObjectId(), 0))}));

	clearSent();
	player().setState(CreatureState::ACTIVE);
	EXPECT_TRUE(teleporter.getAi().onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(CreatureState::ACTIVE));
	EXPECT_EQ(count(serializedFor(serverpackets::SM_EMOTION(player(), model::EmotionType::START_FLYTELEPORT, 279001, 0))), 1u)
		<< "804811 flies path 279";
	EXPECT_EQ(sent().back(), serializedFor(serverpackets::SM_DIALOG_WINDOW(teleporter.getObjectId(), 0))) << "after the teleport";

	clearSent();
	Npc& last = npc<HiddenTeleportProbe>(103.0f, LAST_HIDDEN_TELEPORTER);
	EXPECT_TRUE(last.getAi().onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(count(serializedFor(serverpackets::SM_EMOTION(player(), model::EmotionType::START_FLYTELEPORT, 285001, 0))), 1u)
		<< "804825 flies path 285";

	clearSent();
	player().unsetState(CreatureState::FLYING);
	Npc& other = npc<HiddenTeleportProbe>(104.0f);
	EXPECT_TRUE(other.getAi().onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_FALSE(player().isInState(CreatureState::FLYING)) << "another npc id: teleport id 0, no teleport";
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_DIALOG_WINDOW(other.getObjectId(), 0))}));
}

// ---- ChestAI -----------------------------------------------------------------------------------------------------------------------------

/** ChestAI.java:39-48, :78-79: no chest template, no dialog; no keys, the monologue 1111301 and the chest stays */
TEST_F(StageThreeCp2AiTest, ChestWithoutTemplateOrKeys) {
	dataholders::DataManager::CHEST_DATA.publish(xml::bindString<dataholders::ChestData>(dataContext, CHEST_XML));
	Npc& stranger = npc<ChestProbe>(102.0f, HIDDEN_TELEPORTER);
	{
		network::test::LogCapture capture({"ai.ChestAI"});
		aiOf<ChestProbe>(stranger).handleDialogStart(player());
		EXPECT_EQ(capture.count("Missing chest template or incorrect AI for npc 804811"), 1) << capture.dump();
	}
	EXPECT_TRUE(sent().empty()) << "no use bar";
	EXPECT_THROW(aiOf<ChestProbe>(stranger).handleUseItemFinish(player()), runtime::NullPointerException) << "Java: chestTemplate.getKeyItems()";

	Npc& chest = npc<ChestProbe>(103.0f);
	aiOf<ChestProbe>(chest).handleDialogStart(player());
	EXPECT_FALSE(sent().empty()) << "the template is there: ActionItemNpcAI's dialog";
	stored(9911, MERCENARYS_FRUIT_JUICE, 1);
	stored(9912, MINOR_LIFE_POTION, 1);
	clearSent();
	aiOf<ChestProbe>(chest).handleUseItemFinish(player());
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_SYSTEM_MESSAGE(model::ChatType::NORMAL,
		runtime::Ptr<model::gameobjects::VisibleObject>(player()), 1111301, {}))}))
		<< "2 of 3 keys";
	EXPECT_FALSE(chest.isDead());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MERCENARYS_FRUIT_JUICE), 1) << "nothing taken";
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 1);
}

/**
 * ChestAI.java:50-56, :81-104: the keys are taken across the ids in order before the DEAD check; a dead chest only audits. (The opening of a live
 * chest goes on to DropRegistrationService.registerDrop, whose calculateBoostDropRate loads the opener's houses from the database
 * (DropTestSupport.h): it is left to the in-game retest. The chest is marked DEAD here without dying, as a looted chest is.)
 */
TEST_F(StageThreeCp2AiTest, ChestTakesKeysAndAuditsALootedChest) {
	dataholders::DataManager::CHEST_DATA.publish(xml::bindString<dataholders::ChestData>(dataContext, CHEST_XML));
	const bool savedLogAudit = configs::main::LoggingConfig::LOG_AUDIT.load();
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	Npc& chest = npc<ChestProbe>();
	aiOf<ChestProbe>(chest).handleDialogStart(player());
	chest.setState(CreatureState::DEAD, true); // NpcController.onDie: setState(DEAD, true)
	stored(9911, MERCENARYS_FRUIT_JUICE, 2);
	stored(9912, MINOR_LIFE_POTION, 4);
	clearSent();
	{
		network::test::LogCapture capture({"AUDIT_LOG"});
		aiOf<ChestProbe>(chest).handleUseItemFinish(player());
		EXPECT_EQ(capture.count("attempted multiple chest looting!"), 1) << capture.dump();
	}
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MERCENARYS_FRUIT_JUICE), 0) << "both juices first";
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 3) << "then the one missing potion";
	EXPECT_EQ(count(serializedFor(serverpackets::SM_SYSTEM_MESSAGE(model::ChatType::NORMAL,
				  runtime::Ptr<model::gameobjects::VisibleObject>(player()), 1111301, {}))),
		0u)
		<< "opened";
	{
		network::test::LogCapture capture({"AUDIT_LOG"});
		aiOf<ChestProbe>(chest).handleUseItemFinish(player());
		EXPECT_EQ(capture.count("attempted multiple chest looting!"), 1) << capture.dump();
	}
	EXPECT_EQ(player().getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 0) << "exactly three: all three from the potions";

	Npc& keyless = npc<ChestProbe>(104.0f, KEYLESS_CHEST);
	keyless.setState(CreatureState::DEAD, true);
	{
		network::test::LogCapture capture({"AUDIT_LOG"});
		// the template has no talk delay: ActionItemNpcAI's dialog finishes the use at once
		aiOf<ChestProbe>(keyless).handleDialogStart(player());
		EXPECT_EQ(capture.count("attempted multiple chest looting!"), 1) << "item id 0: no key needed" << capture.dump();
	}
	configs::main::LoggingConfig::LOG_AUDIT.store(savedLogAudit);
}

// ---- SummonerAI --------------------------------------------------------------------------------------------------------------------------

/** spawn_helpers.xml shape: at 75 % skill 10034 and two summon groups after 1 s and 2 s, at 50 % the individual summons */
constexpr const char* SUMMONS_XML = R"xml(<ai_templates><ai npcId="798100"><summons>
	<percentage percent="75" skillId="10034">
		<summonGroup npcId="700001" minCount="2" schedule="1000"/>
		<summonGroup npcId="700002" minCount="1" schedule="2000"/>
	</percentage>
	<percentage percent="50" isIndividual="true"/>
	<percentage percent="25"/>
</summons></ai></ai_templates>)xml";

/** SummonerAI.java:31-35, :89-114: each percentage fires once as the hp falls; the summon groups after their schedule */
TEST_F(StageThreeCp2AiTest, SummonerCallsHelpersByHpPercentage) {
	dataholders::DataManager::AI_DATA.publish(xml::bindString<dataholders::AIData>(dataContext, SUMMONS_XML));
	Npc& summoner = npc<SummonerProbe>();
	SummonerProbe& ai = aiOf<SummonerProbe>(summoner);
	EXPECT_THROW(ai.handleDied(), commons::utils::UnsupportedOperationException) << "Java: Collections.emptyList().clear() before the spawn";
	ai.handleAttack(nullptr);
	EXPECT_TRUE(ai.befores.empty()) << "no percentages before the spawn";

	ai.handleSpawned();
	summoner.getLifeStats()->setCurrentHpPercent(80);
	ai.handleAttack(nullptr);
	EXPECT_TRUE(ai.befores.empty()) << "80 %: above 75";
	ASSERT_EQ(summoner.getGameStats()->getLastSkillTime(), 0);

	summoner.getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(nullptr);
	EXPECT_EQ(ai.befores, std::vector<int32_t>{75});
	EXPECT_NE(summoner.getGameStats()->getLastSkillTime(), 0) << "AIActions.useSkill(this, 10034)";
	executor->advance(std::chrono::milliseconds(999));
	EXPECT_TRUE(ai.spawned.empty());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(ai.spawned, std::vector<int32_t>{700001});
	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(ai.spawned, (std::vector<int32_t>{700001, 700002}));

	summoner.getLifeStats()->setCurrentHpPercent(60);
	ai.handleAttack(nullptr);
	EXPECT_EQ(ai.befores, std::vector<int32_t>{75}) << "75 % fired already";
	summoner.getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(nullptr);
	EXPECT_EQ(ai.individuals, std::vector<int32_t>{50});
	EXPECT_EQ(ai.befores, std::vector<int32_t>{75}) << "an individual percentage has no summon groups";
	summoner.getLifeStats()->setCurrentHpPercent(20);
	ai.handleAttack(nullptr);
	EXPECT_EQ(ai.befores, std::vector<int32_t>{75}) << "25 %: no summons and not individual, nothing";
	EXPECT_EQ(ai.individuals, std::vector<int32_t>{50});

	ai.handleBackHome(); // resets spawnedPercent
	ai.handleAttack(nullptr);
	EXPECT_EQ(ai.befores, (std::vector<int32_t>{75, 75})) << "after the reset every percentage above the hp fires again";
	EXPECT_EQ(ai.individuals, (std::vector<int32_t>{50, 50}));
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(ai.spawned.size(), 4u);

	ai.handleDespawned();
	ai.handleAttack(nullptr);
	EXPECT_EQ(ai.befores.size(), 2u) << "despawned: the list is cleared";
}

/** SummonerAI.java:44-55, :70-87, :116-131: the helpers leave when it returns home (walking home only); the spawn asks checkBeforeSpawn */
TEST_F(StageThreeCp2AiTest, SummonerHelpersLeave) {
	dataholders::DataManager::AI_DATA.publish(xml::bindString<dataholders::AIData>(dataContext, SUMMONS_XML));
	Npc& summoner = npc<SummonerProbe>();
	SummonerProbe& ai = aiOf<SummonerProbe>(summoner);
	ai.handleSpawned();
	Npc& helper = npc<roots::AggressiveNpcAI>(103.0f);
	Npc& unspawned = npc<roots::AggressiveNpcAI>(104.0f);
	world::World::getInstance().storeObject(helper); // delete() is World.removeObject
	world::World::getInstance().storeObject(unspawned);
	unspawned.getPosition()->setIsSpawned(false);
	ai.addHelpersSpawn(helper.getObjectId());
	ai.addHelpersSpawn(unspawned.getObjectId());

	ai.handleNotAtHome();
	EXPECT_EQ(ai.getState(), AIState::RETURNING) << "ReturningEventHandler.onNotAtHome: only a path walker walks home (WALKING)";
	EXPECT_TRUE(world::World::getInstance().isInWorld(helper.getObjectId())) << "returning, not walking: the helpers stay";
	ai.handleBackHome();
	EXPECT_FALSE(world::World::getInstance().isInWorld(helper.getObjectId())) << "home: removeAndResetHelperSpawns";
	EXPECT_TRUE(world::World::getInstance().isInWorld(unspawned.getObjectId())) << "an unspawned helper is not deleted";

	ai.setStateIfNot(AIState::IDLE);
	ai.recordSpawns = false;
	ai.allowSpawn = false;
	summoner.getLifeStats()->setCurrentHpPercent(70);
	ai.handleAttack(nullptr);
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(ai.checks, 2) << "both summon groups asked checkBeforeSpawn";
	EXPECT_EQ(ai.finished, 0) << "refused: no spawn, no handleSpawnFinished";
}

// ---- SkillCooltimeResetAI ----------------------------------------------------------------------------------------------------------------

/** SkillCooltimeResetAI.java:75-92: a player within 8 m is greeted once with the price */
TEST_F(StageThreeCp2AiTest, CooltimeResetterGreetsOnce) {
	Npc& resetter = npc<CooltimeResetProbe>(105.0f);
	const std::vector<uint8_t> greeting = npcSays(resetter, "I can heal you and reset your skill cooldowns for 50,000 Kinah, yang yang.");
	aiOf<CooltimeResetProbe>(resetter).handleCreatureMoved(player());
	EXPECT_EQ(count(greeting), 1u);
	aiOf<CooltimeResetProbe>(resetter).handleCreatureMoved(player());
	EXPECT_EQ(count(greeting), 1u) << "already in sight";

	Npc& far = npc<CooltimeResetProbe>(108.5f);
	aiOf<CooltimeResetProbe>(far).handleCreatureMoved(player());
	EXPECT_EQ(count(npcSays(far, "I can heal you and reset your skill cooldowns for 50,000 Kinah, yang yang.")), 0u) << "8.5 m: out of range 8";
	Npc& other = npc<CooltimeResetProbe>(107.5f);
	aiOf<CooltimeResetProbe>(other).handleCreatureMoved(resetter);
	EXPECT_EQ(count(npcSays(other, "I can heal you and reset your skill cooldowns for 50,000 Kinah, yang yang.")), 0u) << "an npc moved";
	aiOf<CooltimeResetProbe>(other).handleCreatureMoved(player());
	EXPECT_EQ(count(npcSays(other, "I can heal you and reset your skill cooldowns for 50,000 Kinah, yang yang.")), 1u) << "7.5 m";
}

/**
 * SkillCooltimeResetAI.java:59-73, :94-180: the dialog offers the reset while a skill (cooldown <= 3060, no avatar skill) or a buff or potion
 * item (use delay <= 300 s) is on cooldown; the accepted request takes 50,000 kinah, heals and resets those cooldowns only
 */
TEST_F(StageThreeCp2AiTest, CooltimeResetterResetsForKinah) {
	Npc& resetter = npc<CooltimeResetProbe>();
	world::World::getInstance().storeObject(resetter); // delete() is World.removeObject
	CooltimeResetProbe& ai = aiOf<CooltimeResetProbe>(resetter);
	player().setSkillList(model::skill::PlayerSkillList::create({
		model::skill::PlayerSkillEntry::create(37, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED),
		model::skill::PlayerSkillEntry::create(38, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED),
		model::skill::PlayerSkillEntry::create(39, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED),
		model::skill::PlayerSkillEntry::create(40, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED),
	}));
	stored(9921, MERCENARYS_FRUIT_JUICE, 1); // delay id 21, skill 10034: SPEC, SHOP_FOOD_HPREGEN
	stored(9922, MINOR_LIFE_POTION, 1);      // delay id 11, skill 9889: ITEM_POTION_
	stored(9923, TEST_SCROLL, 1);            // delay id 31, skill 9890: BUFF
	stored(9924, TEST_ARENA_POTION, 1);      // delay id 41, skill 9891: ITEM_ARENA_POTION_
	stored(9925, TEST_NO_DELAY_ID, 1);
	const std::vector<uint8_t> nothing = npcSays(resetter, "Daeva has no skill cooldowns to reset, yang.");

	const int64_t now = commons::utils::currentTimeMillis();
	player().setSkillCoolDown(38, now + 60000); // avatar
	player().setSkillCoolDown(39, now + 60000); // cooldown 3061
	player().setSkillCoolDown(37, now - 1000);  // expired
	player().addItemCoolDown(21, now + 60000, 5);    // food: neither buff nor potion
	player().addItemCoolDown(11, now + 60500, 301);  // a potion, but over 300 s
	player().addItemCoolDown(31, now - 1000, 60);    // a buff, expired
	player().addItemCoolDown(51, now + 60000, 60);   // no item of the delay id
	ai.handleDialogStart(player());
	EXPECT_EQ(count(nothing), 1u);
	EXPECT_FALSE(player().getResponseRequester().respond(1300765, 1)) << "no request";

	player().setSkillCoolDown(37, now + 60000); // cooldown 3060: resettable
	clearSent();
	ai.handleDialogStart(player());
	EXPECT_EQ(count(nothing), 0u);
	stored(9930, KINAH, 49999);
	clearSent();
	player().removeSkillCoolDown(37); // the kinah is checked before the cooldowns are collected again
	ASSERT_TRUE(player().getResponseRequester().respond(1300765, 1)) << "the request of the skill cooldown";
	EXPECT_EQ(count(serializedFor(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(50000))), 1u);
	player().setSkillCoolDown(37, now + 60000);

	player().getInventory().increaseKinah(10001); // 60,000
	player().setSkillCoolDown(400, now + 60000);   // skill 40's cooldown id
	player().addItemCoolDown(11, now + 60500, 300); // the potion at 300 s
	player().addItemCoolDown(31, now + 60500, 60);
	player().addItemCoolDown(41, now + 60500, 60);
	player().getLifeStats()->setCurrentHp(10);
	player().getLifeStats()->setCurrentMp(10);
	ASSERT_LT(player().getLifeStats()->getCurrentMp(), player().getLifeStats()->getMaxMp());
	ai.handleDialogStart(player());
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(1300765, 1));
	EXPECT_EQ(player().getInventory().getKinah(), 10000);
	EXPECT_EQ(player().getLifeStats()->getCurrentHp(), player().getLifeStats()->getMaxHp());
	EXPECT_EQ(player().getLifeStats()->getCurrentMp(), player().getLifeStats()->getMaxMp());
	EXPECT_EQ(player().getSkillCoolDown(37), 0);
	EXPECT_EQ(player().getSkillCoolDown(400), 0) << "the cooldown id, not the skill id";
	EXPECT_EQ(player().getSkillCoolDown(38), now + 60000) << "avatar";
	EXPECT_EQ(player().getSkillCoolDown(39), now + 60000) << "over 3060";
		EXPECT_EQ(count(serializedFor(serverpackets::SM_SKILL_COOLDOWN(player(), std::vector<int32_t>{37, 400}))), 1u);
	for (int32_t reset : {11, 31, 41})
		EXPECT_EQ(player().getItemReuseTime(reset), 0) << reset;
	for (int32_t kept : {21, 51})
		EXPECT_EQ(player().getItemReuseTime(kept), now + 60000) << kept;
	std::unordered_map<int32_t, runtime::Ptr<model::items::ItemCooldown>> dummies;
	std::vector<runtime::Ref<model::items::ItemCooldown>> keep;
	for (int32_t reset : {11, 31, 41}) {
		keep.push_back(model::items::ItemCooldown::create(now + 60500, 0)); // 60 s left for 500 ms
		dummies[reset] = runtime::Ptr<model::items::ItemCooldown>(keep.back());
	}
	EXPECT_EQ(count(serializedFor(serverpackets::SM_ITEM_COOLDOWN(dummies))), 1u) << "the old reuse times with use delay 0";
	EXPECT_TRUE(world::World::getInstance().isInWorld(resetter.getObjectId())) << "not on a PvP map: the npc stays";

	player().setSkillCoolDown(37, now + 60000);
	player().getController().enterCombat(true);
	clearSent();
	ai.handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_IN_COMBAT_STATE())}));
}

/** SkillCooltimeResetAI.java:88: GeoService.canSee - a wall between the npc and the player keeps the greeting back */
TEST_F(StageThreeCp2AiTest, CooltimeResetterGreetsOnlyInGeoSight) {
	const bool canSeeEnabled = configs::main::GeoDataConfig::CANSEE_ENABLE.load();
	const bool geoEnabled = configs::main::GeoDataConfig::GEO_ENABLE.load();
	const std::filesystem::path root =
		std::filesystem::temp_directory_path() / ("aion_gs_handlers_ai_core_cdreset_" + std::to_string(std::random_device()()));
	std::filesystem::create_directories(root / "data" / "geo");
	BigEndianWriter meshes;
	writeWall(meshes, "levels/test/wall.cgf");
	writeFile(root / "data" / "geo" / "models.mesh", meshes.bytes);
	BigEndianWriter geo;
	writePlacement(geo, "levels/test/wall.cgf", 103.5f, 100, 50); // between the player at x 100 and the npc at x 107
	writeFile(root / "data" / "geo" / "210010000.geo", geo.bytes);
	const std::filesystem::path workingDirectory = std::filesystem::current_path();
	std::filesystem::current_path(root);
	configs::main::GeoDataConfig::GEO_ENABLE.store(true);
	try {
		world::geo::GeoService::getInstance().init();
	} catch (...) {
		std::filesystem::current_path(workingDirectory);
		throw;
	}
	std::filesystem::current_path(workingDirectory);
	geoEngine::GeoCallbacks::setMaterialZoneSink(nullptr);
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(true);

	Npc& walled = npc<CooltimeResetProbe>(107.0f);
	Npc& free = npc<CooltimeResetProbe>(100.0f, NPC, 107.0f);
	ASSERT_FALSE(world::geo::GeoService::getInstance().canSee(walled, player())) << "the fixture's wall blocks the sight";
	ASSERT_TRUE(world::geo::GeoService::getInstance().canSee(free, player()));
	aiOf<CooltimeResetProbe>(walled).handleCreatureMoved(player());
	aiOf<CooltimeResetProbe>(free).handleCreatureMoved(player());
	EXPECT_EQ(count(npcSays(walled, "I can heal you and reset your skill cooldowns for 50,000 Kinah, yang yang.")), 0u) << "behind the wall";
	EXPECT_EQ(count(npcSays(free, "I can heal you and reset your skill cooldowns for 50,000 Kinah, yang yang.")), 1u) << "7 m, free sight";

	configs::main::GeoDataConfig::GEO_ENABLE.store(false);
	world::geo::GeoService::getInstance().init(); // empty GeoMaps: what a server with geo data off has
	configs::main::GeoDataConfig::GEO_ENABLE.store(geoEnabled);
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(canSeeEnabled);
	std::error_code ignored;
	std::filesystem::remove_all(root, ignored);
}

// ---- the ConquestOffering AIs --------------------------------------------------------------------------------------------------------------

/** ConquestOfferingSpawnerAI.java:126-157: no damage; a dead offering schedules one respawn (10-20 min); death or despawn cancels it */
TEST_F(StageThreeCp2AiTest, ConquestSpawnerRespawnTask) {
	Npc& spawner = npc<SpawnerProbe>();
	SpawnerProbe& ai = aiOf<SpawnerProbe>(spawner);
	EXPECT_EQ(spawner.getAi().modifyDamage(player(), 500.0f, nullptr), 0.0f);
	EXPECT_EQ(spawner.getAi().modifyOwnerDamage(500.0f, player(), nullptr), 0.0f);
	const size_t before = executor->pendingTaskCount();
	ai.handleCustomEvent(2, {});
	EXPECT_EQ(executor->pendingTaskCount(), before) << "only event 1";
	ai.handleCustomEvent(1, {});
	EXPECT_EQ(executor->pendingTaskCount(), before + 1);
	ai.handleCustomEvent(1, {});
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "one pending respawn at a time";
	executor->advance(std::chrono::milliseconds(1200000)); // spawnRandomNpc of 798100: none of the 12 spawners, no spawn
	EXPECT_EQ(executor->pendingTaskCount(), before) << "done";
	ai.handleCustomEvent(1, {});
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "the done task is replaced";
	ai.handleDespawned();
	executor->runReady();
	EXPECT_EQ(executor->pendingTaskCount(), before) << "handleDespawned cancelled it";
	ai.handleCustomEvent(1, {});
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "a cancelled task is replaced";
}

/** ConquestOfferingAggressiveAI.java:20-38: its creator (the spawner) hears of its death while alive */
TEST_F(StageThreeCp2AiTest, ConquestAggressiveNotifiesItsSpawner) {
	Npc& spawner = npc<SpawnerProbe>();
	mapInstance->addObject(spawner);
	Npc& offering = npc<ConquestAggressiveProbe>(103.0f, NPC, 100.0f, spawner.getObjectId());
	offering.setCreatorId(spawner.getObjectId()); // SpawnEngine.spawnObject sets it from the template
	aiOf<ConquestAggressiveProbe>(offering).handleSpawned();
	// a generator whose first Rnd.chance() is 55 or more: spawnRandomNpc spawns nothing
	uint64_t seed = 1;
	for (;; seed++) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		if (commons::utils::Rnd::chance() >= 55)
			break;
	}
	commons::utils::Rnd::seedCurrentThreadForTests(seed);
	const size_t before = executor->pendingTaskCount();
	aiOf<ConquestAggressiveProbe>(offering).handleDied();
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "the spawner's respawn task (onCustomEvent(1))";
}

/** ConquestOfferingBuffNpcAI.java:27-59: it leaves after 65 s unless despawned; one use buffs and removes it */
TEST_F(StageThreeCp2AiTest, ConquestBuffNpc) {
	Npc& buff = npc<BuffNpcProbe>();
	world::World::getInstance().storeObject(buff); // delete() is World.removeObject
	aiOf<BuffNpcProbe>(buff).handleSpawned();
	executor->advance(std::chrono::milliseconds(64999));
	EXPECT_TRUE(world::World::getInstance().isInWorld(buff.getObjectId()));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(world::World::getInstance().isInWorld(buff.getObjectId())) << "65 s";

	Npc& kept = npc<BuffNpcProbe>(103.0f);
	world::World::getInstance().storeObject(kept);
	aiOf<BuffNpcProbe>(kept).handleSpawned();
	aiOf<BuffNpcProbe>(kept).handleDespawned();
	executor->advance(std::chrono::milliseconds(70000));
	EXPECT_TRUE(world::World::getInstance().isInWorld(kept.getObjectId())) << "handleDespawned cancelled the task";

	EXPECT_THROW(aiOf<BuffNpcProbe>(kept).handleUseItemFinish(player()), runtime::NullPointerException) << "no template of 21924-21927";
	aiOf<BuffNpcProbe>(kept).handleUseItemFinish(player());
	EXPECT_TRUE(world::World::getInstance().isInWorld(kept.getObjectId())) << "used already: nothing";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
