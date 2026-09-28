// P4-10 MaterialZoneHandler (wave 5a stage 3, code-findings lane): the second of the two zone handlers that only a geo-enabled start builds
// (ZoneService::createMaterialZoneTemplate creates a SiegeShield for material 11 and a MaterialZoneHandler for every other material of
// material_templates.xml). The first one cost the real-client session of 2026-09-21 thirteen npc spawns (docs/design/m5a-client-session.md F-1)
// because nothing catches a throwing zone handler: VisibleObjectSpawner only logs "Error during spawn" and the npc never enters the world. This
// handler had no test at all, although 4 of the 34 material skills of material_templates.xml carry target="NPC" and therefore fire for a
// spawning npc exactly like the shield did.
//
// Expectations are derived by hand from MaterialZoneHandler.java, MaterialSkill.java and MaterialTarget.java. Notably MaterialSkill.getTarget()
// is `target == null ? MaterialTarget.ALL : target` (MaterialSkill.java:38-40), so a <skill> without a target attribute matches every creature
// instead of throwing; the port keeps that tolerance in the `getTarget()` of the hand-written shell. A <material> without any <skill> child is
// the opposite case and the one deviation of this handler: Java's getSkills() is null there and throws, the C++ vector is empty and does nothing
// (docs/deviations/P4-10.md, AMaterialWithoutSkillsIsANoOpWhereJavaThrows; the row was filed in P4-11a.md and moved to the world file, where its
// subject and this test live, in stage 3 wave B).
//
// Test doubles: the mesh is a geoEngine Node with no children (so the collision check of the actor finds nothing) that records the ray it is
// asked to collide with (RecordingMesh below; stage 3 fixer run, see TheQueuedCollisionCheckOfTheActorFindsNothing), and the material template
// is bound from XML text without publishing it, because WorldTestSupport publishes one empty MATERIAL_DATA per process.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "WorldTestSupport.h"

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/materials/MaterialTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"
#include "aion/gameserver/world/zone/handler/MaterialZoneHandler.h"

namespace aion::gameserver::world::test {
namespace {

using runtime::Ptr;
using runtime::Ref;

constexpr int32_t ELYOS_NPC_ID = 210001;
constexpr int32_t ASMO_NPC_ID = 220001;

/** two npc templates, one per race; <stats> is what NpcLifeStats reads in setupStatContainers */
const char* const NPC_TEMPLATES = R"(<npc_templates>)"
								  R"(<npc_template npc_id="210001" level="10" name_id="1" name="elyos guard" race="ELYOS" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="500" maxMp="200"/></npc_template>)"
								  R"(<npc_template npc_id="220001" level="10" name_id="2" name="asmo guard" race="ASMODIANS" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="500" maxMp="200"/></npc_template>)"
								  R"(</npc_templates>)";

/**
 * material 13 is the real Asmodian shield material of material_templates.xml with its skill retargeted to NPC, material 14 has a skill without a
 * target attribute (Java's MaterialTarget.ALL default), material 60 keeps the real target="PLAYER" and material 121 is copied verbatim from
 * material_templates.xml:84 - one of the four entries that have no <skill> child at all.
 */
const char* const MATERIALS_XML = R"(<material_templates>)"
								  R"(<material id="13"><skill id="8341" level="1" target="NPC" frequency="3"/></material>)"
								  R"(<material id="14"><skill id="12029" level="1" frequency="1"/></material>)"
								  R"(<material id="60"><skill id="8302" level="1" target="PLAYER" frequency="5"/></material>)"
								  R"(<material id="121" skill_obstacle="1"/>)"
								  R"(</material_templates>)";

/**
 * A mesh Node that records the collision query it is given. `MaterialZoneHandler::onEnterZone` ends with `actor->moved()`
 * (MaterialZoneHandler.java:59), whose queued body builds the ray and calls `geometry->collideWith(ray, results)`
 * (AbstractCollisionObserver.cpp:57-88); with a plain Node nothing of that body is observable from outside, because it only writes the
 * observer's protected oldPos and leaves ZoneCollisionMaterialActor.isTouched false. Collision detection itself stays the real Node code (no
 * children and no world bound, so nothing is found).
 */
class RecordingMesh final : public geoEngine::scene::Node {
	AION_MAKE_REF_FRIEND
public:
	static Ref<RecordingMesh> create(std::string_view name) { return runtime::makeRef<RecordingMesh>(name); }

	/** How often the queued collision check reached this geometry */
	std::atomic<int32_t> collisionChecks{0};
	/** The ray of the last check (written before the counter, which the test reads first) */
	geoEngine::math::Ray lastRay{};

	int32_t collideWith(geoEngine::math::Ray& ray, geoEngine::collision::CollisionResults& results) override {
		lastRay = ray;
		collisionChecks.fetch_add(1);
		return Node::collideWith(ray, results);
	}

protected:
	explicit RecordingMesh(std::string_view name) : Node(std::optional<std::string_view>(name)) {}
	~RecordingMesh() override = default;
};

class MaterialZoneHandlerTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = new runtime::DeterministicExecutor(clock, 4);
		utils::ThreadPoolManager::installBackend(std::unique_ptr<runtime::DeterministicExecutor>(executor));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES));
		materials = xml::bindString<dataholders::MaterialData>(context, MATERIALS_XML); // not published: one MATERIAL_DATA per process
		spawnGroup = model::templates::spawns::SpawnGroup::create(POETA, ELYOS_NPC_ID, 0, nullptr);
		spawnTemplate = model::templates::spawns::SpawnTemplate::create(*spawnGroup, 140.0f, 140.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0);
		asmoGroup = model::templates::spawns::SpawnGroup::create(POETA, ASMO_NPC_ID, 0, nullptr);
		asmoSpawn = model::templates::spawns::SpawnTemplate::create(*asmoGroup, 140.0f, 140.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0);
	}

	void TearDown() override {
		spawnTemplate = nullptr;
		asmoSpawn = nullptr;
		spawnGroup = nullptr;
		asmoGroup = nullptr;
		materials.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	/** an npc with the real constructor chain (NpcGameStats and NpcLifeStats from the template, ObserveController from Creature) */
	Ref<model::gameobjects::Npc> makeNpc(bool elyos) {
		model::templates::spawns::SpawnTemplate& spawn = elyos ? *spawnTemplate : *asmoSpawn;
		return model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(elyos ? ELYOS_NPC_ID : ASMO_NPC_ID));
	}

	/** the geometry of an Asmodian shield mesh: MaterialZoneHandler's constructor reads the owner race off the name (BU_AB_DARKSP -> ASMODIANS) */
	static Ref<RecordingMesh> makeMesh(std::string_view name, int8_t materialId) {
		Ref<RecordingMesh> mesh = RecordingMesh::create(name);
		mesh->setMaterialId(materialId);
		return mesh;
	}

	Ref<zone::handler::MaterialZoneHandler> makeHandler(std::string_view meshName, int32_t materialId) {
		mesh = makeMesh(meshName, static_cast<int8_t>(materialId));
		const model::templates::materials::MaterialTemplate* template_ = materials->getTemplate(materialId);
		EXPECT_TRUE(template_) << "the bound material data has material " << materialId;
		return zone::handler::MaterialZoneHandler::create(*mesh, template_);
	}

	/** the plain SUB sphere zone of Poeta (WorldTestSupport ZONES_XML), a ZoneInstance without an own onEnter override */
	static Ref<zone::ZoneInstance> poetaSubZone() {
		std::unordered_map<const zone::ZoneName*, Ref<zone::ZoneInstance>> zones = zone::ZoneService::getInstance().getZoneInstancesByWorldId(POETA);
		return zones.at(zone::ZoneName::get("SUB_PLAIN_210010000"));
	}

	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr; // owned by ThreadPoolManager
	xml::LoadContext context;
	std::unique_ptr<dataholders::MaterialData> materials;
	Ref<RecordingMesh> mesh;
	Ref<model::templates::spawns::SpawnGroup> spawnGroup;
	Ref<model::templates::spawns::SpawnGroup> asmoGroup;
	Ref<model::templates::spawns::SpawnTemplate> spawnTemplate;
	Ref<model::templates::spawns::SpawnTemplate> asmoSpawn;
};

TEST_F(MaterialZoneHandlerTest, ACreatureOfTheMeshOwnerRaceIsIgnored) {
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("BU_AB_DARKSP_TEST_13", 13);
	Ref<model::gameobjects::Npc> npc = makeNpc(false); // ASMODIANS, like the BU_AB_DARKSP mesh
	Ref<zone::ZoneInstance> zone = poetaSubZone();

	handler->onEnterZone(*npc, *zone);
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "MaterialZoneHandler.java:43: `if (ownerRace == creature.getRace()) return;`";
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(mesh->collisionChecks.load(), 0) << "the early return never reaches actor.moved()";
}

TEST_F(MaterialZoneHandlerTest, AnNpcOfTheOtherRaceGetsAndLosesItsMaterialActor) {
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("BU_AB_DARKSP_TEST_13", 13);
	Ref<model::gameobjects::Npc> npc = makeNpc(true); // ELYOS
	Ref<zone::ZoneInstance> zone = poetaSubZone();
	ASSERT_FALSE(npc->getObserveController()->hasObservers());

	// target="NPC" matches, so the handler creates a ZoneCollisionMaterialActor and registers it with the creature (MaterialZoneHandler.java:52-54)
	handler->onEnterZone(*npc, *zone);
	EXPECT_TRUE(npc->getObserveController()->hasObservers());

	handler->onLeaveZone(*npc, *zone);
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "onLeaveZone removes the observer and aborts the actor";
}

TEST_F(MaterialZoneHandlerTest, ASkillWithoutATargetAttributeMatchesEveryCreature) {
	// MaterialSkill.java:38-40 returns MaterialTarget.ALL for a missing target attribute instead of dereferencing null, so the handler must not
	// throw inside onEnterZone, where nothing would catch it (m5a-client-session.md F-1).
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("BU_AB_LIGHTSP_TEST_14", 14);
	Ref<model::gameobjects::Npc> npc = makeNpc(false); // ASMODIANS against an Elyos mesh
	Ref<zone::ZoneInstance> zone = poetaSubZone();

	EXPECT_NO_THROW(handler->onEnterZone(*npc, *zone));
	EXPECT_TRUE(npc->getObserveController()->hasObservers()) << "MaterialTarget.ALL matches";
	handler->onLeaveZone(*npc, *zone);
	EXPECT_FALSE(npc->getObserveController()->hasObservers());
}

TEST_F(MaterialZoneHandlerTest, ASkillTargetingPlayersLeavesAnNpcAlone) {
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("SOME_LAVA_MESH_60", 60);
	Ref<model::gameobjects::Npc> npc = makeNpc(true);
	Ref<zone::ZoneInstance> zone = poetaSubZone();

	handler->onEnterZone(*npc, *zone);
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "no matching skill: `if (matchingSkills.isEmpty()) return;`. 26 of the 34 real "
																"material skills target PLAYER, which is why only the shield handler hit F-1";
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(mesh->collisionChecks.load(), 0) << "the empty-skill return never reaches actor.moved()";
}

TEST_F(MaterialZoneHandlerTest, AMaterialWithoutSkillsIsANoOpWhereJavaThrows) {
	// material_templates.xml:84-87 has four entries with a skill_obstacle attribute and no <skill> child (ids 121-124). Java's
	// MaterialTemplate.getSkills() returns the raw JAXB list, which is null for them, so the for-each of MaterialZoneHandler.java:46 throws a
	// NullPointerException at the first creature that enters such a zone - and nothing catches a throwing zone handler (m5a-client-session.md
	// F-1). The generated C++ template holds an empty vector, so the loop body simply never runs. Deviation: docs/deviations/P4-10.md.
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("BU_AB_DARKSP_TEST_121", 121);
	const model::templates::materials::MaterialTemplate* template_ = materials->getTemplate(121);
	ASSERT_TRUE(template_);
	EXPECT_TRUE(template_->getSkills().empty()) << "Java: null, not an empty list";
	EXPECT_EQ(template_->getSkillObstacle().value_or(0), 1) << "the attribute those four entries do carry";
	Ref<model::gameobjects::Npc> npc = makeNpc(true); // ELYOS against the Asmodian mesh, so the race check does not return first
	Ref<zone::ZoneInstance> zone = poetaSubZone();

	EXPECT_NO_THROW(handler->onEnterZone(*npc, *zone));
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "no skill matched, so no actor";
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(mesh->collisionChecks.load(), 0);
	EXPECT_NO_THROW(handler->onLeaveZone(*npc, *zone));
}

TEST_F(MaterialZoneHandlerTest, TheHandlerIsReachedThroughZoneInstanceOnEnter) {
	// the path a spawning npc takes: revalidateZones -> ZoneInstance::onEnter -> every handler of the zone
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("BU_AB_DARKSP_TEST_13", 13);
	Ref<model::gameobjects::Npc> npc = makeNpc(true);
	Ref<zone::ZoneInstance> zone = poetaSubZone();
	zone->addHandler(*handler);

	EXPECT_TRUE(zone->onEnter(*npc));
	EXPECT_TRUE(npc->getObserveController()->hasObservers()) << "ZoneInstance.onEnter calls onEnterZone of every handler";
	EXPECT_TRUE(zone->isInsideCreature(*npc));

	EXPECT_TRUE(zone->onLeave(*npc));
	EXPECT_FALSE(npc->getObserveController()->hasObservers());
	EXPECT_FALSE(zone->isInsideCreature(*npc));
}

TEST_F(MaterialZoneHandlerTest, TheQueuedCollisionCheckOfTheActorFindsNothing) {
	Ref<zone::handler::MaterialZoneHandler> handler = makeHandler("BU_AB_DARKSP_TEST_13", 13);
	Ref<model::gameobjects::Npc> npc = makeNpc(true);
	Ref<zone::ZoneInstance> zone = poetaSubZone();

	// onEnterZone ends with actor.moved() (MaterialZoneHandler.java:59), which AbstractCollisionObserver runs on the pool. The mesh has no
	// geometry child, so the ray hits nothing, ZoneCollisionMaterialActor.onMoved leaves isTouched false and AbstractMaterialSkillActor.act()
	// never adds the skill task. The recording mesh is what makes the queued body observable: without it, deleting the actor.moved() call
	// changes nothing a test can see.
	handler->onEnterZone(*npc, *zone);
	ASSERT_FALSE(npc->getController().hasTask(model::TaskId::ZONE_MATERIAL_ACTION));
	executor->advance(std::chrono::milliseconds(2000));
	ASSERT_EQ(mesh->collisionChecks.load(), 1) << "actor.moved() queued one collision check and it ran against this geometry";

	// the CheckType of a material id below 14 is TOUCH: the ray starts one bound radius above the creature and points straight down to
	// z - 0.11 (AbstractCollisionObserver.cpp:59-73; the npc is no Player, so the geo ground query is skipped)
	EXPECT_FLOAT_EQ(mesh->lastRay.getOrigin().getX(), npc->getX());
	EXPECT_FLOAT_EQ(mesh->lastRay.getOrigin().getY(), npc->getY());
	EXPECT_GT(mesh->lastRay.getOrigin().getZ(), npc->getZ()) << "zMax = z + 0.05 + boundRadius.upper";
	EXPECT_NEAR(mesh->lastRay.getDirection().getZ(), -1.0f, 1e-5f) << "normalized, straight down";
	EXPECT_NEAR(mesh->lastRay.getLimit(), mesh->lastRay.getOrigin().getZ() - (npc->getZ() - 0.11f), 1e-4f) << "zMax - zMin";

	EXPECT_FALSE(npc->getController().hasTask(model::TaskId::ZONE_MATERIAL_ACTION)) << "no collision, so no material skill task";
	EXPECT_TRUE(npc->getObserveController()->hasObservers()) << "an untouched actor stays attached";
	handler->onLeaveZone(*npc, *zone);
	EXPECT_FALSE(npc->getObserveController()->hasObservers());
	EXPECT_EQ(mesh->collisionChecks.load(), 1) << "onLeaveZone aborts the actor instead of checking again";
}

} // namespace
} // namespace aion::gameserver::world::test
