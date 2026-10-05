// PortalService (P5-08, m5f-plan.md §2.5 steps 3-6, §5 T-03 and T-08): port with its checks, port(private) and transfer - the way into and
// out of an instance through a portal npc.
//
// Java: PortalService.java:45-367.
// The fixture is tests/instance/AscensionTestSupport.h (P5-13's, by relative path as TravelTestSupport.h includes it): the World of real map
// rows (Verteron, Haramel, Karamatis, Taloc's Hollow, Kamar, ...), the real cooltime and exit rows, the ascension test database (an instance
// created by InstanceService.getNextAvailableInstance spawns through SpawnEngine.spawnInstance, whose HousingService reads the database). Added
// per case: instance_cooltimes.xml:428-437 (Haramel, verbatim: DAILY, 16 entries, one member, level 16), portal_loc.xml:203-204 and :42
// (Haramel's two locs and its Verteron exit), npc_templates.xml:453720-453723 and :453730-453733 (the Haramel entrance, a dialog npc, and
// the Haramel exit, which is none), the Kinah item row, and synthetic cooltime rows where a case needs another member count, a level cap or
// a mentor ban. The portal paths are built in place (PortalPath is a data-only template): portal_template2.xml:166-168's Haramel entrance
// path (loc 3002000, ELYOS, err_level 27) and :157-160's exit path, and variations of them for the checks.
//
// Not covered, said in place: the group and alliance arms of the second switch (maxPlayers 3/6 and above) need a PlayerGroup or a
// PlayerAlliance, which only P5-10's team services create (M5g; the same limit as InstanceLifecycleTest's team arm) - their refusals for a
// player without a team (checkPlayerSize) are covered; the INSTANCE_ENTER_ALL access level and the MembershipConfig permissions (an
// account's access level and membership, which the fixture's accounts do not have: every check runs).

#include "TravelTestSupport.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/PortalLocData.bind.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/portal/PortalPath.h"
#include "aion/gameserver/model/templates/portal/QuestReq.h"
#include "aion/gameserver/model/templates/portal/ItemReq.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {
namespace {

namespace inst = ::aion::gameserver::instance::test;
namespace cptest = network::aion::clientpackets::testing;
using inst::HARAMEL;
using inst::KAMAR_BATTLEFIELD;
using inst::TALOCS_HOLLOW;
using inst::VERTERON;
using model::gameobjects::Npc;
using model::templates::portal::PortalPath;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;

/** ServerPacketsOpcodes.java:38 (SM_TELEPORT_LOC), :141 (SM_INSTANCE_INFO) */
constexpr int32_t SM_TELEPORT_LOC_OPCODE = 20;
constexpr int32_t SM_INSTANCE_INFO_OPCODE = 141;
constexpr int32_t KINAH = 182400001;
constexpr int32_t ANCIENT_KINAH = 182006985;
constexpr int32_t HARAMEL_ENTRANCE = 730318; // a dialog npc (talk_info is_dialog="true")
constexpr int32_t HARAMEL_EXIT = 730320;     // no dialog npc
constexpr int32_t HARAMEL_LOC = 3002000;
constexpr int32_t HARAMEL_INNER_LOC = 3002001;
constexpr int32_t HARAMEL_EXIT_LOC = 2100301;
constexpr const char* PORTAL_LOGGER = "com.aionemu.gameserver.services.teleport.PortalService";

/** Where the actor stands in Verteron (inside the map, away from every spawn) */
constexpr float VERTERON_X = 2500.0f, VERTERON_Y = 800.0f, VERTERON_Z = 100.0f;

/** npc_templates.xml:453720-453723, :453730-453733 */
constexpr std::string_view PORTAL_NPC_TEMPLATES = R"xml(
	<npc_template npc_id="730318" level="1" name="haramel secret entrance" name_id="371605" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="portal" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="172" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" delay="3" is_dialog="true" can_talk_invisible="false" />
	</npc_template>
	<npc_template npc_id="730320" level="1" name="haramel exit" name_id="371715" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="portal" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="172" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" delay="3" can_talk_invisible="false" />
	</npc_template>
)xml";

/** instance_cooltimes.xml:428-437 */
constexpr std::string_view HARAMEL_COOLTIME = R"xml(
	<instance_cooltime race="PC_ALL" worldId="300200000" id="46" sync_id="46">
		<type>DAILY</type>
		<ent_cool_time>900</ent_cool_time>
		<maxcount>16</maxcount>
		<max_member_light>1</max_member_light>
		<max_member_dark>1</max_member_dark>
		<enter_min_level_light>16</enter_min_level_light>
		<enter_min_level_dark>16</enter_min_level_dark>
		<can_enter_mentor>true</can_enter_mentor>
	</instance_cooltime>
)xml";

/** portal_loc.xml:203-204 (Haramel) and :42 (Haramel's exit in Verteron), and a synthetic loc of Taloc's Hollow */
constexpr std::string_view PORTAL_LOCS_XML = R"xml(<portal_locs>
	<portal_loc world_id="300200000" loc_id="3002000" x="172" y="20" z="144.22548" h="60"/>
	<portal_loc world_id="300200000" loc_id="3002001" x="220" y="213" z="126.68472" h="60"/>
	<portal_loc world_id="210030000" loc_id="2100301" x="2533.8564" y="835.055" z="103.967476" h="59"/>
	<portal_loc world_id="300190000" loc_id="3001900" x="200" y="200" z="100" h="10"/>
	<portal_loc world_id="301120000" loc_id="3011200" x="200" y="200" z="100" h="10"/>
</portal_locs>)xml";

/** items/item_templates.xml:894666 (Kinah) and the row of 182006985 (Lesser Ancient Kinah, a stackable junk item; tests/cm_ak/ItemPacketTestSupport.h:179) */
constexpr std::string_view KINAH_ITEM_XML = R"xml(<item_templates>
	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>
	<item_template id="182006985" name="Lesser Ancient Kinah" level="1" cName="junk_OwnerTree_housing_gold_01" mask="28684" max_stack_count="1000" quality="JUNK" price="42857" desc="801258"/>
</item_templates>)xml";

/** A synthetic cooltime row of a map (one member, the levels and the mentor flag given) */
std::string cooltimeRow(int32_t worldId, int32_t members, int32_t minLevel, int32_t maxLevel, bool mentor) {
	return "<instance_cooltime race=\"PC_ALL\" worldId=\"" + std::to_string(worldId) + "\" id=\"900\" sync_id=\"900\"><type>RELATIVE</type>"
		   "<ent_cool_time>60</ent_cool_time><maxcount>1</maxcount><max_member_light>" + std::to_string(members) + "</max_member_light>"
		   "<max_member_dark>" + std::to_string(members) + "</max_member_dark><enter_min_level_light>" + std::to_string(minLevel)
		   + "</enter_min_level_light><enter_min_level_dark>" + std::to_string(minLevel) + "</enter_min_level_dark>"
		   + (maxLevel > 0 ? "<enter_max_level_light>" + std::to_string(maxLevel) + "</enter_max_level_light><enter_max_level_dark>"
								 + std::to_string(maxLevel) + "</enter_max_level_dark>"
						   : std::string())
		   + "<can_enter_mentor>" + (mentor ? "true" : "false") + "</can_enter_mentor></instance_cooltime>";
}

/** A spawn part with a heading (Java `new SpawnTemplate(group, x, y, z, h, 0, null, 0, 0, null)`) */
class PortalSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	PortalSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, 0, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class PortalServiceTest : public inst::AscensionWorldTest {
protected:
	void SetUp() override {
		inst::AscensionWorldTest::SetUp();
		if (!prepared)
			return;
		std::string npcRows = inst::npcTemplatesXml();
		npcRows.insert(npcRows.rfind("</npc_templates>"), std::string(PORTAL_NPC_TEMPLATES));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcRows));
		// the DAILY entrance cooltime is computed in server time (InstanceCooltimeData -> ServerTime, gameserver.timezone)
		savedZone = configs::main::GSConfig::TIME_ZONE_ID.exchange(std::chrono::locate_zone("Europe/Berlin"));
		// the shipped instance-entry defaults (TravelTestSupport.h): an ordinary account (access level 0, membership 0) is checked
		shippedInstanceConfig.emplace();
		publishCooltimes("");
		// player_experience_table.xml:3-23, levels 0-20 (the base fixture's table stops at 15; Haramel needs 16)
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(),
			"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
			"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
			"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp></player_experience_table>"));
		dataholders::DataManager::PORTAL_LOC_DATA.publish(xml::bindString<dataholders::PortalLocData>(contexts.emplace_back(), PORTAL_LOCS_XML));
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), KINAH_ITEM_XML));
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
	}

	void TearDown() override {
		if (prepared) {
			for (const runtime::Ref<Npc>& created : npcs)
				created->clearKnownlist(model::animations::ObjectDeleteAnimation::FADE_OUT);
			npcs.clear();
			spawnGroups.clear();
			if (actor.player) {
				actor.player->setQuestStateList(nullptr);
			}
			items.clear();
			dataholders::DataManager::PORTAL_LOC_DATA.resetForTests();
			dataholders::DataManager::ITEM_DATA.resetForTests();
			dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
			configs::main::GSConfig::TIME_ZONE_ID.store(savedZone);
			shippedInstanceConfig.reset();
		}
		inst::AscensionWorldTest::TearDown();
	}

	/** The fixture's cooltime rows, Haramel's and `extraRows` */
	void publishCooltimes(const std::string& extraRows) {
		std::string rows = inst::instanceCooltimesXml();
		rows.insert(rows.rfind("</instance_cooltimes>"), std::string(HARAMEL_COOLTIME) + extraRows);
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.resetForTests();
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(xml::bindString<dataholders::InstanceCooltimeData>(contexts.emplace_back(), rows));
	}

	/** The actor in Verteron (an Elyos Daeva of `level` unless said otherwise) with a quest state list and `kinah` Kinah */
	void spawnInVerteron(int32_t level, model::Race race = model::Race::ELYOS, int64_t kinah = 0) {
		spawnActor(VERTERON, 1, VERTERON_X, VERTERON_Y, VERTERON_Z, int8_t{0}, race);
		actor.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
		actor.commonData->setDaeva(level >= 10);
		actor.commonData->setLevel(level);
		ASSERT_EQ(actor.player->getLevel(), level);
		if (kinah > 0) {
			runtime::Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(420501, KINAH, kinah, std::nullopt, 0, "", 0, 0, false, false, 0,
				model::items::storage::getId(model::items::storage::StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
			actor.player->getInventory().onLoadHandler(*item);
			items.push_back(item);
		}
		(*client)->clearSent();
	}

	model::gameobjects::player::Player& player() { return *actor.player; }

	/** A stack of the item in the cube, loaded as the DAO loads it (no packet) */
	void carry(int32_t itemId, int64_t count) {
		runtime::Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(420502, itemId, count, std::nullopt, 0, "", 0, 0, false, false, 0,
			model::items::storage::getId(model::items::storage::StorageType::CUBE), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
		player().getInventory().onLoadHandler(*item);
		items.push_back(item);
	}

	/** The npc of the template beside the actor, created as VisibleObjectSpawner.spawnNpc does, outside every region */
	Npc& npc(int32_t npcId) {
		const model::templates::npc::NpcTemplate* objectTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
		EXPECT_NE(objectTemplate, nullptr) << npcId;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(VERTERON, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn =
			group->addSpawnTemplate(std::make_unique<PortalSpawnTemplate>(*group, VERTERON_X + 1, VERTERON_Y, VERTERON_Z));
		runtime::Ref<Npc> created =
			model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
		created->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*created));
		created->setEffectController(std::make_unique<controllers::effect::EffectController>(*created));
		runtime::Ptr<world::WorldMapInstance> mapInstance = world::World::getInstance().getWorldMap(VERTERON)->getWorldMapInstance(1);
		created->setPosition(world::WorldPosition::create(VERTERON, VERTERON_X + 1, VERTERON_Y, VERTERON_Z, 0,
			mapInstance->getRegion(VERTERON_X + 1, VERTERON_Y, VERTERON_Z)));
		created->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	/** portal_template2.xml:166-168: the Elyos path of the Haramel entrance */
	static PortalPath haramelPath() {
		PortalPath path;
		path.locId = HARAMEL_LOC;
		path.race = model::Race::ELYOS;
		path.errLevel = 27;
		return path;
	}

	/** portal_template2.xml:157-158: the Elyos path of the Haramel exit */
	static PortalPath exitPath() {
		PortalPath path;
		path.locId = HARAMEL_EXIT_LOC;
		path.race = model::Race::ELYOS;
		return path;
	}

	std::vector<uint8_t> forActor(network::aion::AionServerPacket&& packet) { return cptest::serialized(std::move(packet), client->con()); }

	std::vector<uint8_t> noRightPage(Npc& portal) {
		return forActor(SM_DIALOG_WINDOW(portal.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
	}

	/** The header Java writes for a server packet id, followed by the body */
	static std::vector<uint8_t> javaPacket(int32_t opcode, const PacketWriter& body) {
		int32_t op = (opcode + 207) ^ 0xDF;
		PacketWriter packet;
		packet.H(op).C(0x44).H(~op);
		packet.data.insert(packet.data.end(), body.data.begin(), body.data.end());
		return packet.data;
	}

	std::vector<std::vector<uint8_t>> ofOpcode(int32_t opcode) {
		std::vector<uint8_t> header = javaPacket(opcode, PacketWriter());
		std::vector<std::vector<uint8_t>> result;
		for (const std::vector<uint8_t>& packet : sent())
			if (headerOf(packet) == header)
				result.push_back(packet);
		return result;
	}

	void animationDone() {
		runtime::Ptr<runtime::Future> task = player().getController().getAndRemoveTask(model::TaskId::TELEPORT);
		ASSERT_TRUE(task) << "no TELEPORT task";
		task->run();
		task->get();
		world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player())); // CM_LEVEL_READY
	}

	/** A Haramel cooldown of `entries` entries that ends in an hour (the way PortalCooldownsDAO loads one) */
	void haramelCooldown(int32_t entries) {
		auto cooldowns = runtime::RcHashMap<int32_t, runtime::Ref<model::gameobjects::player::PortalCooldown>>::create();
		cooldowns->put(HARAMEL, model::gameobjects::player::PortalCooldown::create(HARAMEL, commons::utils::currentTimeMillis() + 3600000, entries));
		player().getPortalCooldownList().setPortalCoolDowns(cooldowns);
	}

	int32_t enterCount(int32_t worldId) {
		runtime::Ptr<model::gameobjects::player::PortalCooldown> cooldown = player().getPortalCooldownList().getPortalCooldown(worldId);
		return cooldown ? cooldown->getEnterCount() : 0;
	}

	/** Enters Haramel through the entrance, to the end of the animation: the player stands in his new instance */
	runtime::Ptr<world::WorldMapInstance> enterHaramel(Npc& entrance) {
		PortalPath path = haramelPath();
		PortalService::port(&path, player(), entrance);
		animationDone();
		EXPECT_EQ(player().getWorldId(), HARAMEL);
		return player().getPosition()->getWorldMapInstance();
	}

	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::Item>> items;
	const std::chrono::time_zone* savedZone = nullptr;
	std::optional<ShippedInstanceConfig> shippedInstanceConfig;
	std::deque<xml::LoadContext> contexts; // one per bound document (the fixture's context holds its npc ids already)
};

// ---- the private instance: port(private) and transfer (PortalService.java:345-367) ------------------------------------------------------------

TEST_F(PortalServiceTest, TheHaramelEntranceCreatesARegisteredInstanceAndCountsTheEntry) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), entrance);

	runtime::Ptr<world::WorldMapInstance> haramel = services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId());
	ASSERT_TRUE(haramel) << "port(private): getNextAvailableInstance + register";
	EXPECT_GT(haramel->getInstanceId(), 1);
	ASSERT_TRUE(haramel->getStartPos()) << "transfer: startPos = the portal loc";
	EXPECT_EQ(haramel->getStartPos()->getMapId(), HARAMEL);
	EXPECT_FLOAT_EQ(haramel->getStartPos()->getX(), 172.0f);
	EXPECT_FLOAT_EQ(haramel->getStartPos()->getZ(), 144.22548f);
	EXPECT_EQ(haramel->getStartPos()->getHeading(), 60);
	// teleportTo(..., FADE_OUT_BEAM = 1): an instance map's SM_TELEPORT_LOC carries the instance id
	EXPECT_EQ(ofOpcode(SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(1).D(HARAMEL).D(haramel->getInstanceId()).F(172.0f).F(20.0f).F(144.22548f).C(60))}));
	EXPECT_EQ(enterCount(HARAMEL), 1) << "a DAILY cooltime: addPortalCooldown";
	EXPECT_EQ(ofOpcode(SM_INSTANCE_INFO_OPCODE).size(), 1u) << "PortalCooldownList.sendEntryInfo";
	animationDone();
	EXPECT_EQ(player().getWorldId(), HARAMEL);
	EXPECT_EQ(player().getInstanceId(), haramel->getInstanceId());
}

TEST_F(PortalServiceTest, AnUsedUpEntranceCooltimeRefusesAnUnregisteredPlayer) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	haramelCooldown(16); // an hour left
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME())}));
	EXPECT_FALSE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, FifteenEntriesStillLetThePlayerIn) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	haramelCooldown(15); // an hour left
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
	EXPECT_EQ(enterCount(HARAMEL), 16);
}

TEST_F(PortalServiceTest, ReenteringTheRegisteredInstanceKeepsTheCountAndIgnoresTheCooltime) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	runtime::Ptr<world::WorldMapInstance> haramel = enterHaramel(entrance);
	ASSERT_TRUE(haramel);
	ASSERT_EQ(enterCount(HARAMEL), 1);
	TeleportService::teleportTo(player(), VERTERON, VERTERON_X, VERTERON_Y, VERTERON_Z); // out again (NONE: SpawnTask at once)
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	ASSERT_EQ(player().getWorldId(), VERTERON);
	for (int32_t i = 1; i < 16; i++) // the cooltime used up: an unregistered player would be refused
		player().getPortalCooldownList().getPortalCooldown(HARAMEL)->increaseEnterCount();
	(*client)->clearSent();
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), entrance);

	// reenter: no cooltime check, no level check, transfer without addPortalCooldown (PortalService.java:101-122, 364)
	EXPECT_EQ(ofOpcode(SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(1).D(HARAMEL).D(haramel->getInstanceId()).F(172.0f).F(20.0f).F(144.22548f).C(60))}));
	EXPECT_EQ(enterCount(HARAMEL), 16) << "the count did not grow";
	EXPECT_TRUE(ofOpcode(SM_INSTANCE_INFO_OPCODE).empty());
}

TEST_F(PortalServiceTest, APortalOfTheSameInstanceMovesAtOnceWithoutAnimation) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	runtime::Ptr<world::WorldMapInstance> haramel = enterHaramel(entrance);
	ASSERT_TRUE(haramel);
	(*client)->clearSent();
	PortalPath inner = haramelPath();
	inner.locId = HARAMEL_INNER_LOC;

	PortalService::port(&inner, player(), entrance);

	// PortalService.java:118-121: teleportTo(player, mapId, his instance, x, y, z, h) - NONE, the same map: spawned at once
	EXPECT_TRUE(ofOpcode(SM_TELEPORT_LOC_OPCODE).empty());
	EXPECT_EQ(player().getInstanceId(), haramel->getInstanceId());
	EXPECT_FLOAT_EQ(player().getX(), 220.0f);
	EXPECT_FLOAT_EQ(player().getY(), 213.0f);
	EXPECT_EQ(player().getHeading(), 60);
	EXPECT_TRUE(player().isSpawned());
	EXPECT_EQ(enterCount(HARAMEL), 1);
}

TEST_F(PortalServiceTest, TheExitToAnOpenMapTeleportsWithTheBeam) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	ASSERT_TRUE(enterHaramel(entrance));
	(*client)->clearSent();
	Npc& exit = npc(HARAMEL_EXIT);
	PortalPath path = exitPath();

	PortalService::port(&path, player(), exit);

	// Verteron has no cooltime row: maxPlayers 0, no registration, port(private) -> not an instance map -> teleportTo(FADE_OUT_BEAM)
	EXPECT_EQ(ofOpcode(SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(1).D(VERTERON).D(VERTERON).F(2533.8564f).F(835.055f).F(103.967476f).C(59))}));
	animationDone();
	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_EQ(player().getInstanceId(), 1);
}

TEST_F(PortalServiceTest, AnUnknownPortalLocOnlyWarns) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	network::test::LogCapture capture({PORTAL_LOGGER}, spdlog::level::info);
	PortalPath path = haramelPath();
	path.locId = 9999999;

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(capture.contains("warning|" + std::string(PORTAL_LOGGER) + "|No portal loc for locId 9999999")) << capture.dump();
	EXPECT_TRUE(sent().empty());
}

// ---- checkEnterLevel (PortalService.java:204-224) -----------------------------------------------------------------------------------------

TEST_F(PortalServiceTest, BelowTheCooltimesMinimumLevelThePortalsErrorPageIsShown) {
	spawnInVerteron(15);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_DIALOG_WINDOW(entrance.getObjectId(), 27))})) << "err_level 27";
	EXPECT_FALSE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, WithoutAnErrorPageTheLevelRefusalIsASystemMessage) {
	spawnInVerteron(15);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.errLevel = 0;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL())}));
}

TEST_F(PortalServiceTest, ThePathsMinimumLevelReplacesTheCooltimes) {
	spawnInVerteron(12);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.minLevel = 12; // not 0: the cooltime's 16 is not read (PortalService.java:210-211)

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, ThePathsMinimumLevelRefusesBelowIt) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.minLevel = 17;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_DIALOG_WINDOW(entrance.getObjectId(), 27))}));
}

TEST_F(PortalServiceTest, AboveTheCooltimesMaximumLevelIsRefused) {
	publishCooltimes(cooltimeRow(TALOCS_HOLLOW, 1, 10, 15, true));
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3001900;
	path.errLevel = 27;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_DIALOG_WINDOW(entrance.getObjectId(), 27))}));
}

TEST_F(PortalServiceTest, TheCooltimesMaximumLevelItselfIsAllowed) {
	publishCooltimes(cooltimeRow(TALOCS_HOLLOW, 1, 10, 15, true));
	spawnInVerteron(15);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3001900;

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(TALOCS_HOLLOW, player().getObjectId()));
}

// ---- checkMentor, checkRace, checkRank, checkTitle, checkQuests (PortalService.java:193-315) -------------------------------------------------

TEST_F(PortalServiceTest, AMentorIsRefusedWhereTheCooltimeBansMentors) {
	publishCooltimes(cooltimeRow(TALOCS_HOLLOW, 1, 10, 0, false));
	spawnInVerteron(16);
	player().setMentor(true);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3001900;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_MENTOR_CANT_ENTER(TALOCS_HOLLOW))}));
}

TEST_F(PortalServiceTest, ANonMentorEntersWhereMentorsAreBanned) {
	publishCooltimes(cooltimeRow(TALOCS_HOLLOW, 1, 10, 0, false));
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3001900;

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(TALOCS_HOLLOW, player().getObjectId()));
}

TEST_F(PortalServiceTest, AMentorMayEnterWhereTheCooltimeAllowsIt) {
	spawnInVerteron(16);
	player().setMentor(true);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, TheOtherRaceGetsTheNoRightPageOfADialogNpc) {
	spawnInVerteron(16, model::Race::ASMODIANS);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath(); // ELYOS

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
}

TEST_F(PortalServiceTest, TheOtherRaceGetsASystemMessageFromANpcWithoutDialog) {
	spawnInVerteron(16, model::Race::ASMODIANS);
	Npc& exit = npc(HARAMEL_EXIT);
	PortalPath path = haramelPath();

	PortalService::port(&path, player(), exit);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MOVE_PORTAL_ERROR_INVALID_RACE())}));
}

TEST_F(PortalServiceTest, APathForEveryRaceLetsBothRacesIn) {
	spawnInVerteron(16, model::Race::ASMODIANS);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.race = model::Race::PC_ALL;

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, AFortressPathWithSiegesOffLetsThePlayerIn) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.siegeId = 1011; // checkSiegeId: SiegeService.getFortress is null with sieges off -> true (PortalService.java:243-249)

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, ARankBelowThePathsMinimumGetsTheNoRightPage) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.minRank = 2; // a new character is GRADE9_SOLDIER, id 1

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
}

TEST_F(PortalServiceTest, TheMinimumRankItselfIsEnough) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.minRank = 1;

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, AnotherTitleGetsTheNoRightPage) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.titleId = 5;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
}

TEST_F(PortalServiceTest, TheRequiredTitleLetsThePlayerIn) {
	spawnInVerteron(16);
	player().getCommonData()->setTitleId(5);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.titleId = 5;

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

PortalPath questPath(int32_t questId, int32_t step) {
	PortalPath path;
	path.locId = HARAMEL_LOC;
	path.race = model::Race::ELYOS;
	model::templates::portal::QuestReq req;
	req.questId = questId;
	req.questStep = step;
	path.questReq.push_back(req);
	return path;
}

TEST_F(PortalServiceTest, AMissingRequiredQuestGetsTheNoRightPageOfADialogNpc) {
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = questPath(1006, 0);

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
}

TEST_F(PortalServiceTest, AMissingRequiredQuestGetsTheGroupGateMessageFromANpcWithoutDialog) {
	spawnInVerteron(16);
	Npc& exit = npc(HARAMEL_EXIT);
	PortalPath path = questPath(1006, 0);

	PortalService::port(&path, player(), exit);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_GROUPGATE_NO_RIGHT())}));
}

TEST_F(PortalServiceTest, ACompletedRequiredQuestLetsThePlayerIn) {
	spawnInVerteron(16);
	player().getQuestStateList()->addQuest(1006, *questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::COMPLETE));
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = questPath(1006, 0);

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, AStartedQuestAtTheRequiredStepLetsThePlayerIn) {
	spawnInVerteron(16);
	runtime::Ref<questEngine::model::QuestState> state = questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::START);
	state->setQuestVarById(0, 3);
	player().getQuestStateList()->addQuest(1006, *state);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = questPath(1006, 3);

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, AStartedQuestBelowTheRequiredStepIsRefused) {
	spawnInVerteron(16);
	runtime::Ref<questEngine::model::QuestState> state = questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::START);
	state->setQuestVarById(0, 2);
	player().getQuestStateList()->addQuest(1006, *state);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = questPath(1006, 3);

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
}

TEST_F(PortalServiceTest, OneOfSeveralRequiredQuestsIsEnough) {
	spawnInVerteron(16);
	player().getQuestStateList()->addQuest(1007, *questEngine::model::QuestState::create(1007, questEngine::model::QuestStatus::COMPLETE));
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = questPath(1006, 0);
	model::templates::portal::QuestReq second;
	second.questId = 1007;
	path.questReq.push_back(second);

	PortalService::port(&path, player(), entrance);

	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

// ---- checkPlayerSize (PortalService.java:260-282) -------------------------------------------------------------------------------------------

TEST_F(PortalServiceTest, AGroupInstanceRefusesAPlayerWithoutGroupWithThePathsPage) {
	publishCooltimes(cooltimeRow(KAMAR_BATTLEFIELD, 6, 10, 0, true));
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3011200;
	path.errGroup = 1352;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_DIALOG_WINDOW(entrance.getObjectId(), 1352))}));
}

TEST_F(PortalServiceTest, AThreeMemberInstanceWithoutGroupPageIsASystemMessage) {
	publishCooltimes(cooltimeRow(KAMAR_BATTLEFIELD, 3, 10, 0, true));
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3011200;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_PARTY_DON())}));
}

TEST_F(PortalServiceTest, AnAllianceInstanceRefusesAPlayerWithoutAlliance) {
	publishCooltimes(cooltimeRow(KAMAR_BATTLEFIELD, 24, 10, 0, true));
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3011200;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_FORCE_DON())}));
}

TEST_F(PortalServiceTest, ALeagueInstanceRefusesAPlayerWithoutLeague) {
	publishCooltimes(cooltimeRow(KAMAR_BATTLEFIELD, 25, 10, 0, true));
	spawnInVerteron(16);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path;
	path.locId = 3011200;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_UNION_DON())}));
}

// ---- checkAndRemoveRequiredItems (PortalService.java:317-343) -------------------------------------------------------------------------------

TEST_F(PortalServiceTest, NotEnoughKinahGetsTheNoRightPageOfADialogNpc) {
	spawnInVerteron(16, model::Race::ELYOS, 499);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.kinah = 500;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
	EXPECT_EQ(player().getInventory().getKinah(), 499);
}

TEST_F(PortalServiceTest, NotEnoughKinahIsTheKinahMessageFromANpcWithoutDialog) {
	spawnInVerteron(16, model::Race::ELYOS, 499);
	Npc& exit = npc(HARAMEL_EXIT);
	PortalPath path = haramelPath();
	path.kinah = 500;

	PortalService::port(&path, player(), exit);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(500))}));
}

TEST_F(PortalServiceTest, ThePathsKinahIsTakenOnEntry) {
	spawnInVerteron(16, model::Race::ELYOS, 500);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	path.kinah = 500;

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(player().getInventory().getKinah(), 0);
	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

TEST_F(PortalServiceTest, AMissingRequiredItemIsRefusedWithTheItemMessage) {
	spawnInVerteron(16, model::Race::ELYOS, 99);
	carry(ANCIENT_KINAH, 99);
	Npc& exit = npc(HARAMEL_EXIT);
	PortalPath path = haramelPath();
	model::templates::portal::ItemReq req;
	req.itemId = ANCIENT_KINAH;
	req.itemCount = 100;
	path.itemReq.push_back(req);

	PortalService::port(&path, player(), exit);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM())}));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(ANCIENT_KINAH), 99);
}

TEST_F(PortalServiceTest, AMissingRequiredItemGetsTheNoRightPageOfADialogNpc) {
	spawnInVerteron(16, model::Race::ELYOS, 99);
	carry(ANCIENT_KINAH, 99);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	model::templates::portal::ItemReq req;
	req.itemId = ANCIENT_KINAH;
	req.itemCount = 100;
	path.itemReq.push_back(req);

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(sent(), cptest::exactly({noRightPage(entrance)}));
}

TEST_F(PortalServiceTest, TheRequiredItemsAreTakenOnEntry) {
	spawnInVerteron(16, model::Race::ELYOS, 150);
	carry(ANCIENT_KINAH, 150);
	Npc& entrance = npc(HARAMEL_ENTRANCE);
	PortalPath path = haramelPath();
	model::templates::portal::ItemReq req;
	req.itemId = ANCIENT_KINAH;
	req.itemCount = 100;
	path.itemReq.push_back(req);

	PortalService::port(&path, player(), entrance);

	EXPECT_EQ(player().getInventory().getItemCountByItemId(ANCIENT_KINAH), 50);
	EXPECT_EQ(player().getInventory().getKinah(), 150) << "no path kinah";
	EXPECT_TRUE(services::instance::InstanceService::getRegisteredInstance(HARAMEL, player().getObjectId()));
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
