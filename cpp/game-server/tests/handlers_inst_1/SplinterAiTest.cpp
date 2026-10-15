// P6I-4f (docs/design/p6-instances-plan.md §4): the AIs of abyssal_splinter and unstableSplinterpath (game-server/data/handlers/ai/instance)
// on npcs of a real Abyssal Splinter instance (the AIs spawn at its coordinates), each AI installed with replaceAi. The walks to a target (the
// move controller), Kaluva's summons percentages (an ai template), the pieces' effect checks and the casts' effects are left to a gate.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/DayshadeAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/EbonsoulAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/GatesSummonedAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/KaluvaAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/KaluvaSpawnAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/PazuzuAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/RukrilAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/YamenessPortalSummonedAI.h"
#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/YamennesAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/PieceOfMidnightAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/PieceOfSplendorAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableDayshadeAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableEbonsoulAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableGatesSummonedAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableKaluvaAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableKaluvaSpawnAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstablePazuzuAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstablePazuzuWormAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableRukrilAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableYamenessPortalSummonedAI.h"
#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableYamennesAI.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

#define AION_SPLINTER_AIS(X)                                                                                                                  \
	X(abyssal_splinter, DayshadeAI)                                                                                                               \
	X(abyssal_splinter, EbonsoulAI)                                                                                                               \
	X(abyssal_splinter, GatesSummonedAI)                                                                                                          \
	X(abyssal_splinter, KaluvaAI)                                                                                                                 \
	X(abyssal_splinter, KaluvaSpawnAI)                                                                                                            \
	X(abyssal_splinter, PazuzuAI)                                                                                                                 \
	X(abyssal_splinter, RukrilAI)                                                                                                                 \
	X(abyssal_splinter, YamenessPortalSummonedAI)                                                                                                 \
	X(abyssal_splinter, YamennesAI)                                                                                                               \
	X(unstableSplinterpath, PieceOfMidnightAI)                                                                                                    \
	X(unstableSplinterpath, PieceOfSplendorAI)                                                                                                    \
	X(unstableSplinterpath, UnstableDayshadeAI)                                                                                                   \
	X(unstableSplinterpath, UnstableEbonsoulAI)                                                                                                   \
	X(unstableSplinterpath, UnstableGatesSummonedAI)                                                                                              \
	X(unstableSplinterpath, UnstableKaluvaAI)                                                                                                     \
	X(unstableSplinterpath, UnstableKaluvaSpawnAI)                                                                                                \
	X(unstableSplinterpath, UnstablePazuzuAI)                                                                                                     \
	X(unstableSplinterpath, UnstablePazuzuWormAI)                                                                                                 \
	X(unstableSplinterpath, UnstableRukrilAI)                                                                                                     \
	X(unstableSplinterpath, UnstableYamenessPortalSummonedAI)                                                                                     \
	X(unstableSplinterpath, UnstableYamennesAI)

#define AION_DECLARE_FACTORY(dir, cls)                                                                                                        \
	namespace aion::gameserver::handlers::ai::instance::dir {                                                                                 \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_SPLINTER_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;

// the protected handlers the cases drive directly (Java: the AI's events)
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleDied;
	using AI::handleSpawned;
};
template <class AI>
struct SeeingProbe : AI {
	using AI::AI;
	using AI::handleCreatureMoved;
	using AI::handleSpawned;
};

class SplinterAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		// the AIs spawn at the splinter's coordinates: its map (1024), not Haramel's (512)
		instance = &create(300220000, abyss::AbyssalSplinterInstance_instanceFactory);
		enter(*instance, 224.0f, 300.0f, 142.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 230.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 142.0f, int8_t{7}); }

	size_t count(int32_t id) { return npcs(*instance, id).size(); }

	/** SKILL_DATA with one plain template per id (the base fixture publishes an empty holder; its TearDown resets this one) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="splinter" nameId="1" stack="SPLINTER" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	/** The player is on the npc's aggro list (an AggressiveNpcAI without a target goes home on each attack) */
	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The 21 AION_AI factories build their classes; none of them gives loot or AP (each class's ask) */
TEST_F(SplinterAiTest, TheFactoriesBuildTheirClassesWithoutLootOrAp) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(dir, cls)                                                                                                          \
	{                                                                                                                                         \
		auto ai = ais::dir::cls##_aiFactory(*npc);                                                                                          \
		EXPECT_NE(dynamic_cast<ais::dir::cls*>(ai.get()), nullptr) << #cls;                                                                   \
		EXPECT_FALSE(ai->ask(AIQuestion::REWARD_LOOT)) << #cls;                                                                               \
		EXPECT_FALSE(ai->ask(AIQuestion::REWARD_AP)) << #cls;                                                                                 \
	}
	AION_SPLINTER_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** DayshadeAI.java:25-34 and the unstable one: the first attack makes Dayshade die into Ebonsoul and Rukril */
TEST_F(SplinterAiTest, DayshadeDiesIntoTheTwins) {
	runtime::Ptr<Npc> dayshade = place(900200);
	auto& ai = install<Probe<ais::abyssal_splinter::DayshadeAI>>(*dayshade);
	aggro(*dayshade);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(216949), 1u);
	EXPECT_EQ(count(216948), 1u);
	EXPECT_FALSE(dayshade->isSpawned());
	runtime::Ptr<Npc> unstable = place(900300);
	auto& unstableAi = install<Probe<ais::unstableSplinterpath::UnstableDayshadeAI>>(*unstable);
	aggro(*unstable);
	unstableAi.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(219552), 1u);
	EXPECT_EQ(count(219551), 1u);
	EXPECT_FALSE(unstable->isSpawned());
}

/** EbonsoulAI.java:28-51, RukrilAI.java:28-51: below 95 % a cast every 70 s from 5 s, with two pieces when none is left; death stops it */
TEST_F(SplinterAiTest, TheTwinsCallTheirPieces) {
	publishSkills({19159, 19266});
	runtime::Ptr<Npc> ebonsoul = place(900200);
	auto& ai = install<Probe<ais::abyssal_splinter::EbonsoulAI>>(*ebonsoul);
	aggro(*ebonsoul);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count(281908), 0u) << "at full HP no phase";
	ebonsoul->getLifeStats()->setCurrentHpPercent(90);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(count(281908), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(281908), 2u);
	executor().advance(std::chrono::seconds(70));
	EXPECT_EQ(count(281908), 2u) << "none called while two are left";
	ai.handleDied();
	runtime::Ptr<Npc> rukril = place(900300);
	auto& rukrilAi = install<Probe<ais::abyssal_splinter::RukrilAI>>(*rukril);
	aggro(*rukril);
	rukril->getLifeStats()->setCurrentHpPercent(90);
	rukrilAi.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::seconds(5));
	EXPECT_EQ(count(281907), 2u);
}

/** UnstableEbonsoulAI.java:42-72: the unstable twins call one piece each round, and one for the living other twin */
TEST_F(SplinterAiTest, TheUnstableTwinsCallPiecesForBoth) {
	publishSkills({19159, 19266});
	runtime::Ptr<Npc> rukril = place(219551, 240.0f, 300.0f);
	install<gameserver::handlers::ai::AggressiveNpcAI>(*rukril); // his cast needs an NpcAI (a DummyAI cannot use a skill)
	runtime::Ptr<Npc> ebonsoul = place(900200);
	auto& ai = install<Probe<ais::unstableSplinterpath::UnstableEbonsoulAI>>(*ebonsoul);
	aggro(*ebonsoul);
	ebonsoul->getLifeStats()->setCurrentHpPercent(90);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::seconds(5));
	EXPECT_EQ(count(283205), 1u);
	EXPECT_EQ(count(283204), 1u);
	executor().advance(std::chrono::seconds(70));
	EXPECT_EQ(count(283205), 1u) << "her piece only when none is left";
	EXPECT_EQ(count(283204), 2u) << "his piece each round while he lives";
	ai.handleDied();
}

/** UnstableEbonsoulAI.java:66-72: an attack near the living Rukril (5 m) heals her by 10000, not when far */
TEST_F(SplinterAiTest, TheUnstableTwinsHealEachOtherNearby) {
	runtime::Ptr<Npc> rukril = place(219551, 240.0f, 300.0f);
	runtime::Ptr<Npc> ebonsoul = place(900200);
	auto& ai = install<Probe<ais::unstableSplinterpath::UnstableEbonsoulAI>>(*ebonsoul);
	aggro(*ebonsoul);
	ebonsoul->getLifeStats()->setCurrentHpPercent(99);
	int32_t hp = ebonsoul->getLifeStats()->getCurrentHp();
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(ebonsoul->getLifeStats()->getCurrentHp(), hp) << "10 m apart";
	runtime::Ptr<Npc> near = place(219551, 233.0f, 300.0f);
	rukril->getController().delete_();
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(ebonsoul->getLifeStats()->getCurrentHp(), ebonsoul->getLifeStats()->getMaxHp()) << "healed up to the full";
	ai.handleDied();
}

/** PazuzuAI.java:29-65: the first attack calls five worms at once (none left), a round every 70 s; death stops it */
TEST_F(SplinterAiTest, PazuzuCallsHisWorms) {
	runtime::Ptr<Npc> pazuzu = place(900200);
	auto& ai = install<Probe<ais::abyssal_splinter::PazuzuAI>>(*pazuzu);
	aggro(*pazuzu);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count(281909), 5u);
	for (const runtime::Ptr<Npc>& worm : npcs(*instance, 281909))
		worm->getController().delete_();
	executor().advance(std::chrono::seconds(70));
	EXPECT_EQ(count(281909), 5u);
	ai.handleDied();
	for (const runtime::Ptr<Npc>& worm : npcs(*instance, 281909))
		worm->getController().delete_();
	executor().advance(std::chrono::seconds(70));
	EXPECT_EQ(count(281909), 0u) << "stopped";
}

/** UnstablePazuzuAI.java:52-62: four worms from 5 s */
TEST_F(SplinterAiTest, TheUnstablePazuzuCallsFourWorms) {
	publishSkills({19145});
	runtime::Ptr<Npc> pazuzu = place(900200);
	auto& ai = install<Probe<ais::unstableSplinterpath::UnstablePazuzuAI>>(*pazuzu);
	aggro(*pazuzu);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(count(283206), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(283206), 4u);
	ai.handleDied();
}

/** YamenessPortalSummonedAI.java:20-37 and the unstable one: a living portal calls its two summons after 12 s and again a minute later */
TEST_F(SplinterAiTest, AYamennesPortalCallsSummonsTwice) {
	runtime::Ptr<Npc> portal = place(900200);
	auto& ai = install<Probe<ais::abyssal_splinter::YamenessPortalSummonedAI>>(*portal);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(11999));
	EXPECT_EQ(count(281903) + count(281904), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(281903), 1u);
	EXPECT_EQ(count(281904), 1u);
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(count(281903) + count(281904), 4u);
	runtime::Ptr<Npc> unstable = place(900300);
	auto& unstableAi = install<Probe<ais::unstableSplinterpath::UnstableYamenessPortalSummonedAI>>(*unstable);
	unstableAi.handleSpawned();
	executor().advance(std::chrono::seconds(12));
	EXPECT_EQ(count(219565) + count(219566), 2u);
}

/** YamennesAI.java:33-76: a minute after the first attack the three lower gates, 3 s later the three spheres; the next minute no gates while
 * they stand; her death takes the spheres */
TEST_F(SplinterAiTest, YamennesCallsTheGates) {
	runtime::Ptr<Npc> yamennes = place(900200);
	auto& ai = install<Probe<ais::abyssal_splinter::YamennesAI>>(*yamennes);
	aggro(*yamennes);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_EQ(count(282014), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count(282014), 1u);
	EXPECT_EQ(count(282015), 1u);
	EXPECT_EQ(count(282131), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 282014)[0]->getZ(), 198.7f) << "the lower gates first";
	executor().advance(std::chrono::seconds(3));
	EXPECT_EQ(count(282107), 3u);
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(count(282014), 1u) << "no gates while they stand";
	EXPECT_EQ(count(282107), 3u) << "the spheres are renewed";
	ai.handleDied();
	EXPECT_EQ(count(282107), 0u);
}

/** KaluvaSpawnAI.java:31-52: 22 s after its spawn the egg hatches one of the formations and goes */
TEST_F(SplinterAiTest, KaluvasEggHatches) {
	runtime::Ptr<Npc> egg = place(900200);
	auto& ai = install<Probe<ais::abyssal_splinter::KaluvaSpawnAI>>(*egg);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(21999));
	EXPECT_TRUE(egg->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_GT(count(281911) + count(281912) + count(282057), 0u);
	EXPECT_FALSE(egg->isSpawned());
}

/** UnstableKaluvaSpawnAI.java:44-74: Kaluva within 7 m starts the 28 s hatch, another npc does not */
TEST_F(SplinterAiTest, TheUnstableEggHatchesWhenKaluvaComes) {
	publishSkills({19222, 19152});
	runtime::Ptr<Npc> egg = place(900200);
	auto& ai = install<SeeingProbe<ais::unstableSplinterpath::UnstableKaluvaSpawnAI>>(*egg);
	ai.handleSpawned();
	runtime::Ptr<Npc> other = place(900300, 232.0f, 300.0f);
	ai.handleCreatureMoved(*other);
	runtime::Ptr<Npc> farKaluva = place(219553, 240.0f, 300.0f);
	ai.handleCreatureMoved(*farKaluva);
	executor().advance(std::chrono::seconds(30));
	EXPECT_TRUE(egg->isSpawned()) << "no hatch yet";
	runtime::Ptr<Npc> kaluva = place(219553, 233.0f, 300.0f);
	ai.handleCreatureMoved(*kaluva);
	executor().advance(std::chrono::milliseconds(27999));
	EXPECT_TRUE(egg->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_GT(count(219572) + count(219573) + count(219584), 0u);
	EXPECT_FALSE(egg->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
