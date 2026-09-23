// DropRegistrationService::registerDrop as the AION_PARTIAL of m5b-plan.md D5 (M5b-1; loot itself is M5b-3, O-05).
//
// Why this file exists: m5b-client-session.md S-1. On 2026-09-22 a real 4.8 client killed five monsters and the game server logged five
// "onDie() exception for Npc [...]" ERROR lines, one per kill, every one of them the UnportedException of this body. NpcController::onDie
// reaches it through doReward inside a try that only logs (NpcController.cpp:172-181, Java NpcController.java:146-155), so the kill completed
// and the experience was kept - but InstanceHandler::onDie and the DIED AI event were skipped, and the gate's "no ERROR line" bar (§6.3 Q1)
// could never be met. The fix is the partial, not the drop system.
//
// Expectations are derived by hand from DropRegistrationService.java:52-54 (the delegating overload) and :59-109 (the body the partial stands
// in for), and from NpcController.java:161-184 for what the death path does with the two maps afterwards.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "EconomyTestSupport.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::economy::test {
namespace {

using runtime::Ptr;
using runtime::Ref;

/**
 * The reason of the AION_PARTIAL, and therefore the row the M5b gate's allow-list carries in its §A section (m5b-plan.md §6.1, R3: hit exactly
 * once per kill). It is asserted here so that a lane which changes the wording sees the gate's contract break in its own tests.
 */
constexpr std::string_view DROP_PARTIAL = "npc drops are not registered yet (M5b-3)";

/** the file the allow-list row names, as the partial trace shortens it (runtime/base/Unported.h) */
constexpr std::string_view DROP_PARTIAL_FILE = "aion/gameserver/services/drop/DropRegistrationService.cpp";

/** One AION_PARTIAL site, found by the reason its macro carries; a reason nothing has reached yet has no site at all. */
const runtime::PartialHit* partialSiteFor(std::string_view reason, std::vector<runtime::PartialHit>& hits) {
	hits = runtime::partialHits();
	for (const runtime::PartialHit& hit : hits) {
		if (hit.reason == reason)
			return &hit;
	}
	return nullptr;
}

uint64_t partialHitsFor(std::string_view reason) {
	std::vector<runtime::PartialHit> hits;
	const runtime::PartialHit* site = partialSiteFor(reason, hits);
	return site == nullptr ? 0 : site->hits;
}

/** how many distinct AION_PARTIAL sites carry that reason, i.e. how many rows the gate's allow-list would need */
size_t partialSitesWith(std::string_view reason) {
	size_t count = 0;
	for (const runtime::PartialHit& hit : runtime::partialHits()) {
		if (hit.reason == reason)
			count++;
	}
	return count;
}

/** A spawn template of the group, like the spawn data of a map */
class DropSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit DropSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
const model::templates::npc::NpcTemplate* npcTemplate(int32_t npcId) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context,
		R"(<npc_template name_id="1" npc_id=")" + std::to_string(npcId) +
			R"(" level="2" name="juvenile sparkie" attack_speed="2142" arange="2" rating="NORMAL" tribe="MONSTER">)"
			R"(<stats maxHp="199" maxMp="100" pdef="130" mdef="70" attack="16" evasion="45" parry="30" block="25" accuracy="200" macc="60")"
			R"( pcrit="10" mcrit="20"><speeds walk="0.8" run="2.0" run_fight="3.0" group_walk="0.5" group_run_fight="2.5" fly="4.0"/></stats>)"
			R"(</npc_template>)")
		.release();
}

class DropRegistrationServiceTest : public EconomyTest {
protected:
	/** Java SpawnEngine: the spawn creates the Npc and gives it a known list */
	Ref<model::gameobjects::Npc> spawnNpc(int32_t npcId) {
		spawnGroup = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawnTemplate = spawnGroup->addSpawnTemplate(std::make_unique<DropSpawnTemplate>(*spawnGroup));
		Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawnTemplate, npcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		return npc;
	}

	Ref<model::templates::spawns::SpawnGroup> spawnGroup;
};

// DropRegistrationService.java:59-109. The whole body stands in, because there is no Java-exact partial answer: a drop set built from the
// ported predicates alone is a set Java never computes, and LOOT_ENABLE for a corpse with no currentDropMap entry makes CM_START_LOOT answer an
// empty list. What matters for M5b-1 is that it RETURNS, so NpcController::onDie finishes the reward block, runs InstanceHandler::onDie and
// fires the DIED AI event.
TEST_F(DropRegistrationServiceTest, RegisterDropIsAPartialThatReturnsInsteadOfThrowing) {
	PublishedHolder npcSkills(dataholders::DataManager::NPC_SKILL_DATA,
		bindXml<dataholders::NpcSkillData>(R"(<npc_skill_templates></npc_skill_templates>)"));
	PlayerFixture fixture = makePlayer(100001, 1001);
	Ref<model::gameobjects::Npc> npc = spawnNpc(210663);
	services::drop::DropRegistrationService& service = services::drop::DropRegistrationService::getInstance();

	const uint64_t before = partialHitsFor(DROP_PARTIAL);
	// the call NpcController::doReward makes (NpcController.cpp:279, Java NpcController.java:244)
	EXPECT_NO_THROW(service.registerDrop(*npc, *fixture.player, fixture.player->getLevel(), {}))
		<< "an AION_UNPORTED here is m5b-client-session.md S-1: one ERROR line per kill, and onDie/DIED skipped";
	EXPECT_EQ(partialHitsFor(DROP_PARTIAL), before + 1) << "the site must be recorded once per call: m5b-plan.md R3 counts it per kill";

	std::vector<runtime::PartialHit> hits;
	const runtime::PartialHit* site = partialSiteFor(DROP_PARTIAL, hits);
	ASSERT_NE(site, nullptr);
	EXPECT_EQ(site->file, DROP_PARTIAL_FILE) << "the allow-list row of the M5b gate names this file and this site's line";
}

// DropRegistrationService.java:52-54: `registerDrop(npc, player, player.getLevel(), groupMembers)`. Ported as the delegation it is, so
// AIActions::registerDrop (AIActions.cpp:111-113, the only other caller in the tree) reaches the same partial instead of a second throw - and so
// the M5b gate's allow-list has ONE row to count, not two.
TEST_F(DropRegistrationServiceTest, TheThreeArgumentOverloadDelegatesToTheSamePartialSite) {
	PublishedHolder npcSkills(dataholders::DataManager::NPC_SKILL_DATA,
		bindXml<dataholders::NpcSkillData>(R"(<npc_skill_templates></npc_skill_templates>)"));
	PlayerFixture fixture = makePlayer(100001, 1001);
	Ref<model::gameobjects::Npc> npc = spawnNpc(210663);
	services::drop::DropRegistrationService& service = services::drop::DropRegistrationService::getInstance();

	// warm the four-argument site first: an AION_PARTIAL site only appears in partialHits() once it has been reached, so without this the site
	// counts below would depend on which test of this binary ran first
	service.registerDrop(*npc, *fixture.player, fixture.player->getLevel(), {});
	const uint64_t before = partialHitsFor(DROP_PARTIAL);
	const size_t sitesBefore = runtime::partialHits().size();

	EXPECT_NO_THROW(service.registerDrop(*npc, *fixture.player, {}));
	EXPECT_EQ(partialHitsFor(DROP_PARTIAL), before + 1) << "the delegation must reach the four-argument body";
	EXPECT_EQ(runtime::partialHits().size(), sitesBefore) << "and must not be a partial of its own: one site, one allow-list row";
	EXPECT_EQ(partialSitesWith(DROP_PARTIAL), 1u) << "the M5b allow-list counts one row for registerDrop (m5b-plan.md §6.1 §A, R3)";
}

// What the skipped statements leave behind. NpcController::petLoot and findPetForLooting run AFTER the try of onDie, unguarded
// (NpcController.cpp:196-230, Java NpcController.java:172-184), and both start by reading these maps; DropService::unregisterDrop reads them
// again on the despawn path. Empty maps are what makes all three return quietly - and are why no DropNpc exists at M5b-1 (CheckOutput.h).
TEST_F(DropRegistrationServiceTest, RegisterDropLeavesTheDropMapsEmptySoTheRestOfTheDeathPathIsSafe) {
	PublishedHolder npcSkills(dataholders::DataManager::NPC_SKILL_DATA,
		bindXml<dataholders::NpcSkillData>(R"(<npc_skill_templates></npc_skill_templates>)"));
	PlayerFixture fixture = makePlayer(100001, 1001);
	Ref<model::gameobjects::Npc> npc = spawnNpc(210663);
	services::drop::DropRegistrationService& service = services::drop::DropRegistrationService::getInstance();

	service.registerDrop(*npc, *fixture.player, fixture.player->getLevel(), {});

	EXPECT_FALSE(service.getCurrentDropMap().get(npc->getObjectId()))
		<< "Java puts the drop set here (DropRegistrationService.java:80); the partial must not put a half-built one";
	EXPECT_FALSE(service.getDropRegistrationMap().get(npc->getObjectId()))
		<< "and no DropNpc is registered (initDropNpc, :68), so findPetForLooting returns null instead of walking an allowed-looter list";
}

} // namespace
} // namespace aion::gameserver::economy::test
