#pragma once

// Shared fixture of the travel-core tests (m5f-plan.md §5 T-08 and P-07, restricted to the early travel slice of §16): tests/playersvc (P5-08,
// TravelTeleportTest.cpp) and - by relative path, like tests/cm_lz includes tests/instance/AscensionTestSupport.h - tests/cm_lz (P5-16,
// TeleportSelectPacketTest.cpp).
//
// It is InWorldPacketTest (a DeterministicExecutor on a ManualClock, real Players and a real AionConnection) on the ascension lane's World of
// real map rows (tests/instance/AscensionTestSupport.h: Poeta, Verteron, Ishalgen, Altgard and five instance maps, no zones, geo data off),
// WITHOUT that fixture's database: an instance a case needs (newInstance) is created bare, without spawns, so nothing reads the database.
// Published per test, verbatim with file:line beside each row: the npc templates of the Poeta and Ishalgen teleporters and flight masters and
// of one npc without teleporter data, their npc_teleporter.xml and teleport_location.xml rows, four flypath_template.xml rows, the Kinah item
// row, skills 10350 (the HiPass buff) and 10373 (a buff without hipass) and the tribe relations the ascension fixture copies. The prices are the shipped prices.properties (100/100/100) with sieges off (influence 0, so PricesService's global
// prices 125 and taxes 113, m5c-plan.md §2.10).
//
// The actor is a connected level-1 Elyos WARRIOR spawned in Poeta beside Daines (203194, spawns/Npcs/210010000_Poeta.xml:134-136), with a quest
// state list and a Kinah stack; `watcher` is a second connected player spawned beside him, so the despawn's SM_DELETE to the players who knew
// him can be read. Npcs are created the way DialogServiceTest.cpp does it (VisibleObjectSpawner.spawnNpc without the World): positioned in
// the map instance and marked spawned, but in no region - a case that needs one in the actor's known list adds it with TestKnownList.

#include "../instance/AscensionTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/FlyPathData.bind.h"
#include "aion/gameserver/dataholders/FlyPathData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/TeleLocationData.bind.h"
#include "aion/gameserver/dataholders/TeleLocationData.h"
#include "aion/gameserver/dataholders/TeleporterData.bind.h"
#include "aion/gameserver/dataholders/TeleporterData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {

namespace cptest = network::aion::clientpackets::testing;
using ::aion::gameserver::instance::test::ALTGARD;
using ::aion::gameserver::instance::test::ISHALGEN;
using ::aion::gameserver::instance::test::KARAMATIS_B;
using ::aion::gameserver::instance::test::POETA;
using ::aion::gameserver::instance::test::VERTERON;

inline constexpr int32_t DAINES = 203194;   // Poeta's teleporter (teleportId 2): Sanctum (quest 1006), Verteron
inline constexpr int32_t KUSTANON = 203070; // Akarios' flight master (teleportId 103): Melponeh's Campsite, FLIGHT 5001
inline constexpr int32_t AERO = 203083;     // Melponeh's flight master (teleportId 104): Akarios Village, FLIGHT 6001
inline constexpr int32_t OSMAR = 203679;    // Ishalgen's teleporter (teleportId 51): Pandaemonium (quest 2008), Altgard
inline constexpr int32_t FULLA = 203064;    // Akarios' soul healer: no teleporter data
inline constexpr int32_t KINAH = 182400001;
inline constexpr int32_t HIPASS_SKILL = 10350; // Administrator's Boon: noresurrectpenalty, nodeathpenalty, hipass
inline constexpr int32_t NO_HIPASS_SKILL = 10373; // Medical Miracle Effect: noresurrectpenalty, nodeathpenalty - an abnormal effect, no hipass

/** spawns/Npcs/210010000_Poeta.xml:135, 764, 39, 532 and spawns/Npcs/220010000_Ishalgen.xml:1322 */
struct Spot {
	float x, y, z;
	int8_t h;
};
inline constexpr Spot DAINES_SPOT{804.924f, 1244.6f, 118.986f, int8_t{105}};
inline constexpr Spot KUSTANON_SPOT{803.915f, 1242.15f, 118.986f, int8_t{0}};
inline constexpr Spot AERO_SPOT{425.428f, 1739.61f, 119.926f, int8_t{15}};
inline constexpr Spot FULLA_SPOT{851.51f, 1208.55f, 117.815f, int8_t{15}};
inline constexpr Spot OSMAR_SPOT{525.529f, 2450.73f, 281.593f, int8_t{116}};
/** where the actor stands: 1.2 m from Daines, 2.4 m from Kustanon, inside both talk ranges (talk_info distance 3, + 1, PositionUtil.isInTalkRange) */
inline constexpr Spot ACTOR_SPOT{805.5f, 1243.6f, 118.986f, int8_t{0}};
/** teleport_location.xml:5 (loc 4, no heading attribute: 0) */
inline constexpr float VERTERON_X = 1640.76f, VERTERON_Y = 1500.32f, VERTERON_Z = 119.70999f;

/** npcs/npc_templates.xml:3864-3870 (203194), :2146-2152 (203070), :2338-2344 (203083), :8974-8989 (203679), and 203064's row */
inline constexpr std::string_view TRAVEL_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="203194" level="40" name="daines" name_id="351402" height="1.8" title_id="350423" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="3" is_dialog="true" func_dialogs="44" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="203070" level="40" name="kustanon" name_id="351062" height="1.8" title_id="350420" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="3" is_dialog="true" func_dialogs="44" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="203083" level="40" name="aero" name_id="351137" height="1.8" title_id="350420" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="3" is_dialog="true" func_dialogs="44" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="203679" level="40" name="osmar" name_id="352256" height="1.8" title_id="350423" group_drop="DARK" rank="DISCIPLINED" rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100772</item>
			<item>110100858</item>
			<item>112100723</item>
			<item>111100762</item>
			<item>114100793</item>
			<item>125001756</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="3" is_dialog="true" func_dialogs="44" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="203064" level="45" name="fulla" name_id="351007" height="1.8" title_id="350412" group_drop="NONE" rank="VETERAN" rating="ELITE" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="10" attack_speed="2000" hpgauge="14" cancel_level="20">
		<stats maxHp="108866">
			<speeds walk="2.1" group_walk="2.1" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>100000592</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="5" is_dialog="true" func_dialogs="35" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** npc_teleporter.xml:15-20 (teleportId 2), :80-85 (51), :145-149 (103), :150-154 (104) */
inline constexpr std::string_view TRAVEL_NPC_TELEPORTER_XML = R"xml(<npc_teleporter>
	<teleporter_template npc_ids="203194" teleportId="2">
		<locations>
			<telelocation loc_id="2" price="100" pricePvp="100" required_quest="1006" type="REGULAR"/>
			<telelocation loc_id="4" price="800" pricePvp="800" type="REGULAR"/>
		</locations>
	</teleporter_template>
	<teleporter_template npc_ids="203679" teleportId="51">
		<locations>
			<telelocation loc_id="7" price="100" pricePvp="100" required_quest="2008" type="REGULAR"/>
			<telelocation loc_id="9" price="800" pricePvp="800" type="REGULAR"/>
		</locations>
	</teleporter_template>
	<teleporter_template npc_ids="830030 203070 203233" teleportId="103">
		<locations>
			<telelocation loc_id="13" teleportid="5001" price="160" pricePvp="160" type="FLIGHT"/>
		</locations>
	</teleporter_template>
	<teleporter_template npc_ids="203083" teleportId="104">
		<locations>
			<telelocation loc_id="12" teleportid="6001" price="160" pricePvp="160" type="FLIGHT"/>
		</locations>
	</teleporter_template>
</npc_teleporter>)xml";

/** teleport_location.xml:3, 5, 8, 10, 13, 14 and 48 (Divine Fortress, a siege route) - the trailing comments left out */
inline constexpr std::string_view TRAVEL_TELEPORT_LOCATION_XML = R"xml(<teleport_location>
	<teleloc_template loc_id="2" mapid="110010000" name="Sanctum" name_id="400489" posX="1313.25" posY="1512.011" posZ="568.107"/>
	<teleloc_template loc_id="4" mapid="210030000" name="Verteron" name_id="400491" posX="1640.76" posY="1500.32" posZ="119.70999"/>
	<teleloc_template loc_id="7" mapid="120010000" name="Pandaemonium" name_id="400494" posX="1685.7" posY="1400.5" posZ="195.48618" heading="60"/>
	<teleloc_template loc_id="9" mapid="220030000" name="Altgard" name_id="400496" posX="1752.5322" posY="1806.6096" posZ="254.66133" heading="60"/>
	<teleloc_template loc_id="12" mapid="210010000" name="Akarios Village" name_id="400499"/>
	<teleloc_template loc_id="13" mapid="210010000" name="Melponeh's Campsite" name_id="400500"/>
	<teleloc_template loc_id="49" mapid="400010000" name="Divine Fortress" name_id="400690" posX="2125.776" posY="1913.0077" posZ="2322.0728" heading="72"/>
</teleport_location>)xml";

/**
 * flypath_template.xml:7-8 (the Poeta flights 5 and 6) and :14-15 (fly paths 12 and 13, which start in Ishalgen and Altgard: the ids of the
 * validator's lookup by loc id, m5f-plan.md D7)
 */
inline constexpr std::string_view TRAVEL_FLYPATH_XML = R"xml(<flypath_template>
	<flypath_location id="5" sx="806.63" sy="1242.11" sz="119" sworld="210010000" ex="426.17" ey="1742.29" ez="119.85" eworld="210010000" time="38"/>
	<flypath_location id="6" sx="427" sy="1741.83" sz="119.82" sworld="210010000" ex="806.78" ey="1242.13" ez="118.69" eworld="210010000" time="38"/>
	<flypath_location id="12" sx="938.1" sy="1710.94" sz="258.81" sworld="220010000" ex="526.25" ey="2449.65" ez="281.82" eworld="220010000" time="35"/>
	<flypath_location id="13" sx="1752.66" sy="1806.09" sz="254.75" sworld="220030000" ex="2681.8" ey="1025.44" ez="311.94" eworld="220030000" time="49.5"/>
</flypath_template>)xml";

/** items/item_templates.xml:894666 */
inline constexpr std::string_view TRAVEL_ITEM_TEMPLATES_XML = R"xml(<item_templates>
	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>
</item_templates>)xml";

/** skills/skill_templates.xml:99663-99676 (skill 10350) and :100044-100057 (skill 10373: 10350's first two effects, without hipass) */
inline constexpr std::string_view TRAVEL_SKILL_TEMPLATES_XML = R"xml(<skill_data>
	<skill_template skill_id="10350" name="Administrator's Boon" nameId="770429" stack="CASH_ITEM_START_KIT" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BOOST" dispel_category="NEVER" activation="ACTIVE" cooldown="0" duration="0" noremoveatdie="true">
		<properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<noresurrectpenalty duration2="3600000" effectid="2103501" e="1" basiclvl="1" noresist="true" />
			<nodeathpenalty duration2="3600000" effectid="2103502" e="2" basiclvl="1" noresist="true" preeffect="1" />
			<hipass duration2="3600000" effectid="2103503" e="3" basiclvl="1" noresist="true" preeffect="2" />
		</effects>
	</skill_template>
	<skill_template skill_id="10373" name="Medical Miracle Effect" nameId="298143" stack="CASH_ITEM_START_KIT_02" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BOOST" dispel_category="NEVER" activation="ACTIVE" cooldown="0" duration="0" noremoveatdie="true">
		<properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<noresurrectpenalty duration2="3600000" effectid="2103501" e="1" basiclvl="1" noresist="true" />
			<nodeathpenalty duration2="3600000" effectid="2103502" e="2" basiclvl="1" noresist="true" preeffect="1" />
		</effects>
	</skill_template>
</skill_data>)xml";

/** A spawn part with a heading and a static id (Java `new SpawnTemplate(group, x, y, z, h, 0, null, staticId, 0, null)`) */
class TravelSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	TravelSpawnTemplate(model::templates::spawns::SpawnGroup& group, const Spot& spot, int32_t staticId)
		: SpawnTemplate(group, spot.x, spot.y, spot.z, spot.h, 0, std::nullopt, staticId, 0, std::nullopt) {}
};

class TravelTest : public cptest::InWorldPacketTest {
protected:
	void SetUp() override {
		cptest::InWorldPacketTest::SetUp();
		if (!::aion::gameserver::instance::test::publishAscensionWorldData())
			GTEST_SKIP() << "this process published another test world (run the test on its own)";
		prepared = true;
		savedMissingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // no AI is registered in the test executables: DummyNpcAI instead
		// the shipped prices.properties (PricesConfig.java:28-32 defaults 100, 100, 100)
		savedPrices = configs::main::PricesConfig::DEFAULT_PRICES.exchange(100);
		savedModifier = configs::main::PricesConfig::DEFAULT_MODIFIER.exchange(100);
		savedTaxes = configs::main::PricesConfig::DEFAULT_TAXES.exchange(100);
		savedFlypathValidator = configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.exchange(false);
		savedLogAudit = configs::main::LoggingConfig::LOG_AUDIT.exchange(true); // AuditLogger writes the AUDIT_LOG line only with it
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, TRAVEL_NPC_TEMPLATES_XML));
		dataholders::DataManager::TELEPORTER_DATA.publish(xml::bindString<dataholders::TeleporterData>(context, TRAVEL_NPC_TELEPORTER_XML));
		dataholders::DataManager::TELELOCATION_DATA.publish(
			xml::bindString<dataholders::TeleLocationData>(context, TRAVEL_TELEPORT_LOCATION_XML));
		dataholders::DataManager::FLY_PATH.publish(xml::bindString<dataholders::FlyPathData>(context, TRAVEL_FLYPATH_XML));
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, TRAVEL_ITEM_TEMPLATES_XML));
		// the item info blob's GENERAL_INFO entry of SM_INVENTORY_UPDATE_ITEM asks it; no cleanup entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, TRAVEL_SKILL_TEMPLATES_XML));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, ::aion::gameserver::instance::test::tribeRelationsXml()));
		// the two reads of SM_PLAYER_INFO that need services these tests have not got (HousingService loads from the database)
		lookups.activeHouseOfPlayer = &::aion::gameserver::instance::test::noHouse;
		lookups.cpInfoForCurrentMap = &::aion::gameserver::instance::test::noCpInfo;
		network::aion::serverpackets::detail::setPacketLookupsForTests(&lookups);
	}

	void TearDown() override {
		if (prepared) {
			for (const runtime::Ref<model::gameobjects::Npc>& npc : npcs)
				npc->clearKnownlist(model::animations::ObjectDeleteAnimation::FADE_OUT);
			for (cptest::PlayerFixture* fixture : {&actor, &watcher}) {
				if (!fixture->player)
					continue;
				fixture->player->getController().cancelAllTasks();
				if (fixture->player->isSpawned())
					world::World::getInstance().despawn(*fixture->player);
				fixture->player->clearKnownlist(model::animations::ObjectDeleteAnimation::FADE_OUT);
				world::World::getInstance().removeObject(*fixture->player);
				fixture->player->setTarget(nullptr);
				fixture->player->setQuestStateList(nullptr);
				fixture->player->setClientConnection(nullptr);
			}
			actorClient.reset();
			watcherClient.reset();
			for (const auto& [mapId, instanceId] : createdInstances)
				world::World::getInstance().getWorldMap(mapId)->removeWorldMapInstance(instanceId);
			createdInstances.clear();
			npcs.clear();
			spawnGroups.clear();
			items.clear();
			actor = {};
			watcher = {};
			network::aion::serverpackets::detail::setPacketLookupsForTests(nullptr);
			dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
			dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
			dataholders::DataManager::ITEM_DATA.resetForTests();
			dataholders::DataManager::FLY_PATH.resetForTests();
			dataholders::DataManager::TELELOCATION_DATA.resetForTests();
			dataholders::DataManager::TELEPORTER_DATA.resetForTests();
			dataholders::DataManager::NPC_DATA.resetForTests();
			configs::main::LoggingConfig::LOG_AUDIT.store(savedLogAudit);
			configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.store(savedFlypathValidator);
			configs::main::PricesConfig::DEFAULT_TAXES.store(savedTaxes);
			configs::main::PricesConfig::DEFAULT_MODIFIER.store(savedModifier);
			configs::main::PricesConfig::DEFAULT_PRICES.store(savedPrices);
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(savedMissingAiHandlers);
		}
		cptest::InWorldPacketTest::TearDown();
	}

	/**
	 * A connected level-1 WARRIOR of the race, stored in the World and spawned at the spot of the map instance, with a quest state list and
	 * `kinah` Kinah
	 */
	cptest::PlayerFixture spawnPlayer(int32_t objectId, int32_t accountId, std::string_view name, model::Race race, int32_t mapId, const Spot& spot,
		int64_t kinah, std::unique_ptr<cptest::TestClient>& client, int32_t instanceId = 1) {
		cptest::PlayerFixture f = cptest::makePlayer(objectId, accountId, name, race);
		f.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*f.player));
		f.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
		if (kinah > 0) {
			runtime::Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(objectId + 1, KINAH, kinah, std::nullopt, 0, "", 0, 0, false, false, 0,
				model::items::storage::getId(model::items::storage::StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
			f.player->getInventory().onLoadHandler(*item); // the DAO's load: no packet
			items.push_back(item);
		}
		world::World& world = world::World::getInstance();
		world.storeObject(*f.player);
		EXPECT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*f.player), mapId, instanceId, spot.x, spot.y, spot.z, spot.h));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*f.player));
		EXPECT_TRUE(f.player->isSpawned());
		client = std::make_unique<cptest::TestClient>();
		client->enterWorld(f);
		(*client)->clearSent();
		return f;
	}

	/** The actor (Elyos, beside Daines in Poeta) and, unless `withWatcher` is false, the watcher 1 m from him in the same map instance */
	void spawnActor(int64_t kinah, model::Race race = model::Race::ELYOS, int32_t mapId = POETA, const Spot& spot = ACTOR_SPOT, bool withWatcher = true,
		int32_t instanceId = 1) {
		actor = spawnPlayer(420101, 9501, "Traveller", race, mapId, spot, kinah, actorClient, instanceId);
		if (withWatcher) {
			watcher =
				spawnPlayer(420201, 9502, "Watcher", race, mapId, Spot{spot.x + 1.0f, spot.y, spot.z, spot.h}, 0, watcherClient, instanceId);
			clearSent();
		}
	}

	/** The npc of the template at the spot of `mapId`'s instance 1, created as VisibleObjectSpawner.spawnNpc does, outside every region */
	model::gameobjects::Npc& npc(int32_t npcId, int32_t mapId, const Spot& spot, int32_t staticId = 0) {
		const model::templates::npc::NpcTemplate* objectTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
		EXPECT_NE(objectTemplate, nullptr) << npcId;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(mapId, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<TravelSpawnTemplate>(*group, spot, staticId));
		runtime::Ref<model::gameobjects::Npc> created =
			model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
		created->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*created));
		created->setEffectController(std::make_unique<controllers::effect::EffectController>(*created));
		runtime::Ptr<world::WorldMapInstance> mapInstance = world::World::getInstance().getWorldMap(mapId)->getWorldMapInstance(1);
		created->setPosition(world::WorldPosition::create(mapId, spot.x, spot.y, spot.z, spot.h, mapInstance->getRegion(spot.x, spot.y, spot.z)));
		created->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	/**
	 * A new instance of the instance map with GeneralInstanceHandler (WorldMapInstanceFactory, as InstanceService.getNextAvailableInstance
	 * creates one, without the spawns and the database); TearDown removes it from its map after the players left
	 */
	int32_t newInstance(int32_t mapId) {
		runtime::Ref<world::WorldMapInstance> created = world::WorldMapInstanceFactory::createWorldMapInstance(*world::World::getInstance().getWorldMap(mapId), 0,
			[](world::WorldMapInstance& mapInstance) { return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(mapInstance)); }, 0);
		createdInstances.emplace_back(mapId, created->getInstanceId());
		return created->getInstanceId();
	}

	model::gameobjects::player::Player& player() { return *actor.player; }

	int64_t kinah() { return player().getInventory().getKinah(); }

	std::vector<std::vector<uint8_t>> sent() { return (*actorClient)->sentBytes(); }
	std::vector<std::vector<uint8_t>> watcherSent() { return (*watcherClient)->sentBytes(); }

	void clearSent() {
		if (actorClient)
			(*actorClient)->clearSent();
		if (watcherClient)
			(*watcherClient)->clearSent();
	}

	/** The server's own serialization of an expected packet for the actor's connection */
	std::vector<uint8_t> forActor(network::aion::AionServerPacket&& packet) { return cptest::serialized(std::move(packet), actorClient->con()); }

	/** The 5 byte opcode header of a serialized packet: it identifies the packet class */
	static std::vector<uint8_t> headerOf(const std::vector<uint8_t>& packet) {
		return std::vector<uint8_t>(packet.begin(), packet.begin() + std::min<size_t>(5, packet.size()));
	}

	/** The header Java writes for a server packet id (AionServerPacket.writeOP: H((opcode + 207) ^ 0xDF), C(0x44), H(~that)) */
	static std::vector<uint8_t> javaHeader(int32_t opcode) {
		int32_t op = (opcode + 207) ^ 0xDF;
		network::test::PacketWriter header;
		header.H(op).C(0x44).H(~op);
		return header.data;
	}

	/** A Java server packet: the header of `opcode` followed by the body */
	static std::vector<uint8_t> javaPacket(int32_t opcode, const network::test::PacketWriter& body) {
		std::vector<uint8_t> packet = javaHeader(opcode);
		packet.insert(packet.end(), body.data.begin(), body.data.end());
		return packet;
	}

	/** The packets of `packets` whose class (header) is `opcode`'s (Java ServerPacketsOpcodes id) */
	static std::vector<std::vector<uint8_t>> ofOpcode(const std::vector<std::vector<uint8_t>>& packets, int32_t opcode) {
		const std::vector<uint8_t> header = javaHeader(opcode);
		std::vector<std::vector<uint8_t>> result;
		for (const std::vector<uint8_t>& packet : packets)
			if (headerOf(packet) == header)
				result.push_back(packet);
		return result;
	}

	bool prepared = false;
	std::string savedMissingAiHandlers;
	int32_t savedPrices = 0, savedModifier = 0, savedTaxes = 0;
	bool savedFlypathValidator = false;
	bool savedLogAudit = false;
	xml::LoadContext context;
	cptest::PlayerFixture actor, watcher;
	std::unique_ptr<cptest::TestClient> actorClient, watcherClient;
	std::vector<runtime::Ref<model::gameobjects::Item>> items;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::Npc>> npcs;
	std::vector<std::pair<int32_t, int32_t>> createdInstances; // map id, instance id
	network::aion::serverpackets::detail::PacketLookupsForTests lookups{};
};

} // namespace aion::gameserver::services::teleport::test
