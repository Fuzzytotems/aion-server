// P6I-7c (docs/design/p6-instances-plan.md §4): RentusBaseInstance, OccupiedRentusBaseInstance and the rentusBase AIs
// (game-server/data/handlers) on real Rentus Base instances, each AI installed with replaceAi. The teleport devices (HousingService loads
// from the database), the walks (a DummyAI cannot walk: Java's ClassCastException, logged by the task), the casts' effects and the forced walk
// of Vasharti are left to a gate.

#include "I5TestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/BrigadeGeneralVashartiAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/CaptainXastaAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/CollapsedReianBuildingAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/DancingFlameAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/DrakanBarricadeAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/ExplosiveDranaCrystalAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/FallenReianAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/IdyunHardXasta.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/ImprisonedReianAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/KuharaBombAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/KuharaTheVolatileAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/ReianBomberAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/SpilledOilAI.h"
#include "aion/gameserver/handlers/ai/instance/rentusBase/VashartiAssassinAI.h"
#include "aion/gameserver/handlers/instance/OccupiedRentusBaseInstance.h"
#include "aion/gameserver/handlers/instance/RentusBaseInstance.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#define AION_RENTUS_AIS(X)                                                                                                                    \
	X(BrigadeGeneralVashartiAI)                                                                                                                   \
	X(CaptainXastaAI)                                                                                                                             \
	X(CollapsedReianBuildingAI)                                                                                                                   \
	X(DancingFlameAI)                                                                                                                             \
	X(DrakanBarricadeAI)                                                                                                                          \
	X(ExplosiveDranaCrystalAI)                                                                                                                    \
	X(FallenReianAI)                                                                                                                              \
	X(IdyunHardXasta)                                                                                                                             \
	X(ImprisonedReianAI)                                                                                                                          \
	X(KuharaBombAI)                                                                                                                               \
	X(KuharaTheVolatileAI)                                                                                                                        \
	X(ReianBomberAI)                                                                                                                              \
	X(SpilledOilAI)                                                                                                                               \
	X(VashartiAssassinAI)

namespace aion::gameserver::handlers::ai::instance::rentusBase {
#define AION_DECLARE_FACTORY(cls) ::aion::gameserver::handlers::AIFactory cls##_aiFactory;
AION_RENTUS_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY
} // namespace aion::gameserver::handlers::ai::instance::rentusBase

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::rentusBase;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleDied;
	using AI::handleSpawned;
};

class RentusTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(RENTUS_BASE, RentusBaseInstance_instanceFactory);
		enter(*instance, 190.0f, 410.0f, 261.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 192.0f, float y = 410.0f) { return spawnNpc(*instance, npcId, x, y, 261.0f, int8_t{0}); }

	runtime::Ptr<Npc> dies(int32_t npcId) {
		runtime::Ptr<Npc> npc = place(npcId);
		instance->getInstanceHandler()->onDie(*npc);
		return npc;
	}

	size_t count(std::initializer_list<int32_t> ids) { return countIn(*instance, ids); }

	static size_t countIn(world::WorldMapInstance& in, std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(in, id).size();
		return n;
	}

	/** SKILL_DATA with one plain template per id (the base fixture publishes an empty holder; its TearDown resets this one) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="rentus" nameId="1" stack="RENTUS" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** RentusBaseInstance.java:205-211, OccupiedRentusBaseInstance: the bosses */
TEST_F(RentusTest, TheBosses) {
	auto& handler = dynamic_cast<RentusBaseInstance&>(*instance->getInstanceHandler());
	EXPECT_TRUE(handler.isBoss(*place(217313)));
	EXPECT_TRUE(handler.isBoss(*place(217309)));
	EXPECT_FALSE(handler.isBoss(*place(236300)));
	world::WorldMapInstance& occupied = create(OCCUPIED_RENTUS_BASE, OccupiedRentusBaseInstance_instanceFactory);
	auto& occupiedHandler = dynamic_cast<OccupiedRentusBaseInstance&>(*occupied.getInstanceHandler());
	EXPECT_TRUE(occupiedHandler.isBoss(*spawnNpc(occupied, 236300, 192.0f, 410.0f, 261.0f, int8_t{0})));
	EXPECT_FALSE(occupiedHandler.isBoss(*spawnNpc(occupied, 217313, 192.0f, 410.0f, 261.0f, int8_t{0})));
}

/** RentusBaseInstance.java:43-100: the lieutenants together, Kuhara's turn, the smaller arms */
TEST_F(RentusTest, TheLieutenantsKuharaAndTheSmallDeaths) {
	place(701156);
	runtime::Ptr<Npc> umatha = place(217315);
	runtime::Ptr<Npc> kiriana = place(217316);
	umatha->getController().die();
	EXPECT_EQ(count({701156}), 1u) << "Kiriana lives";
	kiriana->getController().die();
	EXPECT_EQ(count({701156}), 0u);
	runtime::Ptr<Npc> kuhara = dies(217311);
	EXPECT_FALSE(kuhara->isSpawned());
	EXPECT_EQ(count({219215}), 1u);
	runtime::Ptr<Npc> barrel = dies(282394);
	EXPECT_FALSE(barrel->isSpawned());
	ASSERT_EQ(count({282395}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 282395)[0]->getX(), 192.0f);
	EXPECT_FALSE(dies(283000)->isSpawned());
	EXPECT_FALSE(dies(236283)->isSpawned());
	EXPECT_EQ(count({236284}), 1u);
}

/** :53-57: Kiriana first, while Umatha lives: the door stays */
TEST_F(RentusTest, KirianaFirstKeepsTheDoor) {
	place(701156);
	runtime::Ptr<Npc> umatha = place(217315);
	runtime::Ptr<Npc> kiriana = place(217316);
	kiriana->getController().die();
	EXPECT_EQ(count({701156}), 1u) << "Umatha lives";
	umatha->getController().die();
	EXPECT_EQ(count({701156}), 0u);
}

/** :68-86, 115-117, 152-160: Vasharti's death opens the exit and the box, Ariana speaks and nine soldiers come out (their walk is later) */
TEST_F(RentusTest, VashartisDeathEndsTheBase) {
	place(799669);
	dies(217313);
	EXPECT_EQ(count({799669}), 0u);
	EXPECT_EQ(count({730520}), 1u);
	EXPECT_EQ(count({833047}), 1u);
	EXPECT_EQ(count({799670}), 1u);
	EXPECT_EQ(count({800227}), 3u);
	EXPECT_EQ(count({800228}), 2u);
	EXPECT_EQ(count({800229}), 2u);
	EXPECT_EQ(count({800230}), 2u);
}

/** :102-113, 142-148: Kuhara's aggro speaks; the devices */
TEST_F(RentusTest, TheAggroAndTheDevices) {
	runtime::Ptr<Npc> kuhara = place(217311);
	InstanceKnownListPairing::pair(*kuhara, player());
	instance->getInstanceHandler()->onAggro(*kuhara);
	runtime::Ptr<Npc> device = place(701097);
	instance->getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *device);
	EXPECT_FALSE(device->isSpawned());
	runtime::Ptr<Npc> caller = place(701100);
	instance->getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *caller);
	instance->getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *caller);
	EXPECT_EQ(count({799543}), 1u) << "only while none is there";
}

/** :166-190: the watcher sends Xasta on his walk; at its end his health picks the form, the barricade breaks */
TEST_F(RentusTest, XastasWalkAndOutcome) {
	runtime::Ptr<Npc> watcher = place(856056);
	instance->getInstanceHandler()->onCreatureDetected(*watcher, player());
	instance->getInstanceHandler()->onCreatureDetected(*watcher, player());
	EXPECT_FALSE(watcher->isSpawned());
	EXPECT_EQ(count({236271}), 1u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDYUN_RASTA_SPAWN_01())), 1);
	runtime::Ptr<Npc> barricade = place(856015);
	runtime::Ptr<Npc> xasta = npcs(*instance, 236271)[0];
	xasta->getLifeStats()->setCurrentHpPercent(40);
	instance->getInstanceHandler()->onSpecialEvent(*xasta);
	EXPECT_EQ(count({217310}), 1u) << "the weakened form";
	EXPECT_TRUE(barricade->isDead());
	EXPECT_FALSE(xasta->isSpawned());
}

/** :192-203: the first Elyos swaps the six Asmodian devices for theirs, with the static ids */
TEST_F(RentusTest, TheElyosGetTheirDevices) {
	for (int32_t npcId = 702677; npcId <= 702682; npcId++)
		spawnStaticNpc(*instance, npcId, 150.0f + npcId - 702677, 300.0f, 261.0f, 100 + npcId - 702677);
	instance->getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(count({702677, 702678, 702679, 702680, 702681, 702682}), 0u);
	ASSERT_EQ(count({702683}), 1u);
	EXPECT_EQ(npcs(*instance, 702683)[0]->getSpawn()->getStaticId(), 100);
	EXPECT_EQ(count({702688}), 1u);
}

/** OccupiedRentusBaseInstance: its own Kuhara and Vasharti, and the two devices that cast and go */
TEST_F(RentusTest, TheOccupiedBase) {
	publishSkills({19909});
	world::WorldMapInstance& occupied = create(OCCUPIED_RENTUS_BASE, OccupiedRentusBaseInstance_instanceFactory);
	runtime::Ptr<Npc> kuhara = spawnNpc(occupied, 236298, 192.0f, 410.0f, 261.0f, int8_t{0});
	occupied.getInstanceHandler()->onDie(*kuhara);
	EXPECT_EQ(countIn(occupied, {236705}), 1u);
	occupied.getInstanceHandler()->onDie(*spawnNpc(occupied, 236300, 192.0f, 410.0f, 261.0f, int8_t{0}));
	EXPECT_EQ(countIn(occupied, {730401}), 1u);
	EXPECT_EQ(countIn(occupied, {833048}), 1u);
	runtime::Ptr<Npc> device = spawnNpc(occupied, 701151, 192.0f, 410.0f, 261.0f, int8_t{0});
	install<gameserver::handlers::ai::AggressiveNpcAI>(*device); // its cast needs an NpcAI
	occupied.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *device);
	EXPECT_FALSE(device->isSpawned());
}

/** The fourteen AION_AI factories build their classes */
TEST_F(RentusTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls) EXPECT_NE(dynamic_cast<ais::cls*>(ais::cls##_aiFactory(*npc).get()), nullptr) << #cls;
	AION_RENTUS_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** SpilledOilAI.java:23-51: every 4 s a burn, the seventh time it goes */
TEST_F(RentusTest, TheSpilledOilBurnsOut) {
	publishSkills({19658});
	runtime::Ptr<Npc> oil = place(900200);
	auto& ai = install<Probe<ais::SpilledOilAI>>(*oil);
	ai.handleSpawned();
	executor().advance(std::chrono::seconds(24));
	EXPECT_TRUE(oil->isSpawned());
	executor().advance(std::chrono::seconds(4));
	EXPECT_FALSE(oil->isSpawned());
}

/** DancingFlameAI.java:27-57: no damage; no respawn, loot or decay */
TEST_F(RentusTest, TheDancingFlame) {
	runtime::Ptr<Npc> flame = place(900200);
	auto& ai = install<ais::DancingFlameAI>(*flame);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 100.0f, nullptr), 0.0f);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
}

/** ExplosiveDranaCrystalAI.java:30-71: an unused crystal goes after a minute */
TEST_F(RentusTest, AnUnusedCrystalGoes) {
	runtime::Ptr<Npc> crystal = place(900200);
	auto& ai = install<Probe<ais::ExplosiveDranaCrystalAI>>(*crystal);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_TRUE(crystal->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(crystal->isSpawned());
}

/** KuharaTheVolatileAI.java:42-131: 50 s after the first attack two barrels, 14 s later eight bombs, 11 s later both go; his death stops it */
TEST_F(RentusTest, KuharasBarrelsAndBombs) {
	publishSkills({19703, 19704, 19705, 19375});
	runtime::Ptr<Npc> kuhara = place(900200);
	auto& ai = install<Probe<ais::KuharaTheVolatileAI>>(*kuhara);
	aggro(*kuhara);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(49999));
	EXPECT_EQ(count({282394}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({282394}), 2u);
	executor().advance(std::chrono::seconds(14));
	EXPECT_EQ(count({282396}), 8u);
	EXPECT_FALSE(ai.canThink());
	executor().advance(std::chrono::seconds(11));
	EXPECT_EQ(count({282394, 282396}), 0u);
	EXPECT_TRUE(ai.canThink());
	ai.handleDied();
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(count({282394, 282396}), 0u) << "stopped";
}

/** BrigadeGeneralVashartiAI.java:98-114: the end of her shield buff brings the two spheres (once each) */
TEST_F(RentusTest, VashartisShieldSpheres) {
	publishSkills({20530});
	runtime::Ptr<Npc> vasharti = place(900200);
	auto& ai = install<Probe<ais::BrigadeGeneralVashartiAI>>(*vasharti);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(20530), 1);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(20530), 1);
	EXPECT_EQ(count({283000}), 1u);
	EXPECT_EQ(count({283001}), 1u);
	ai.handleDied();
	EXPECT_EQ(count({283000, 283001}), 0u);
}

/** BrigadeGeneralVashartiAI.java:104-157 with the correction of 2026-10-19 (was J-P6I7-1): at the middle of her forced walk she casts the Sea
 * of Fire's start (20533) and then the Sea of Fire (20534); its end deletes the fire effects 283010-283012 she knows, no other npc */
/** Vasharti without thinking: with no target her FIGHT state would send her home, whose clearSpawns deletes the fire effects too */
struct StillVasharti : Probe<ais::BrigadeGeneralVashartiAI> {
	using Probe::Probe;
	bool canThink() override { return false; }
};

TEST_F(RentusTest, TheEndOfTheSeaOfFireTakesTheFire) {
	publishSkills({20533, 20534});
	runtime::Ptr<Npc> vasharti = place(900200, 188.17f, 414.06f);
	auto& ai = install<StillVasharti>(*vasharti);
	std::vector<runtime::Ptr<Npc>> fires{place(283010, 186.0f, 414.0f), place(283011, 190.0f, 414.0f), place(283012, 188.0f, 416.0f)};
	runtime::Ptr<Npc> bystander = place(900300, 188.0f, 412.0f);
	for (const runtime::Ptr<Npc>& npc : fires)
		InstanceKnownListPairing::pair(*vasharti, *npc);
	InstanceKnownListPairing::pair(*vasharti, *bystander);
	ai.setStateIfNot(gameserver::ai::AIState::FORCED_WALKING);
	ai.isDestinationReached(); // the middle: the Sea of Fire event starts (20533); its own answer is the base class's (the walk's end)
	for (const runtime::Ptr<Npc>& npc : fires)
		ASSERT_TRUE(npc->isSpawned()) << npc->getNpcId() << " before the end of the Sea of Fire";
	ai.onEffectEnd(skillengine::model::Effect::create(*vasharti, runtime::Ptr<model::gameobjects::Creature>(vasharti),
		dataholders::DataManager::SKILL_DATA->getSkillTemplate(20534), 1)); // the end of the Sea of Fire
	for (const runtime::Ptr<Npc>& npc : fires)
		EXPECT_FALSE(npc->isSpawned()) << npc->getNpcId() << " (Java: kept)";
	EXPECT_TRUE(bystander->isSpawned());
	ai.handleDied();
}

/** CaptainXastaAI.java:180-188: the first form's death brings the second and takes the helpers */
TEST_F(RentusTest, XastasFirstFormDies) {
	runtime::Ptr<Npc> xasta = place(217309);
	auto& ai = install<Probe<ais::CaptainXastaAI>>(*xasta);
	place(282604);
	ai.handleDied();
	EXPECT_EQ(count({217310}), 1u);
	EXPECT_EQ(count({282604}), 0u);
	EXPECT_FALSE(xasta->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
