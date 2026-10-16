// P6I-6 (docs/design/p6-instances-plan.md §4): the AIs of sauroBase, tallocsHollow and danuarReliquary
// (game-server/data/handlers/ai/instance) on npcs of a real Sauro Supply Base instance (the other AIs do not read the map), each AI installed
// with replaceAi. The skill casts (no skill rows), the effects, the walkers and Ahuradim's HP phases are left to a gate.

#include "I5TestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/CursedQueenModorAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/EnragedQueenModorAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/FinishThemAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/MaliciousIceStormAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/ModorsCloneAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/ModorsPositionIndicatorAI.h"
#include "aion/gameserver/handlers/ai/instance/danuarReliquary/VengefulOrbAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/CircusBoxesAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/EnragedNightmareAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/HarlequinLordReshkaAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/HarlequinLordReshkaSummonAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/MistressViloaAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareCrateAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareHarlequinAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareLordHeiramuneAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareLordHeiramuneCloneAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/SolidIronChainAI.h"
#include "aion/gameserver/handlers/ai/instance/nightmareCircus/YumeAI.h"
#include "aion/gameserver/handlers/ai/instance/sauroBase/BrigadeGeneralShebaAI.h"
#include "aion/gameserver/handlers/ai/instance/sauroBase/GuardCaptainAhuradim.h"
#include "aion/gameserver/handlers/ai/instance/sauroBase/GuardCaptainAhuradimGenerator.h"
#include "aion/gameserver/handlers/ai/instance/sauroBase/MoriataAI.h"
#include "aion/gameserver/handlers/ai/instance/sauroBase/SauroDroneAI.h"
#include "aion/gameserver/handlers/ai/instance/sauroBase/SauroFinalTeleportAI.h"
#include "aion/gameserver/handlers/ai/instance/tallocsHollow/CelestiusAI.h"
#include "aion/gameserver/handlers/ai/instance/tallocsHollow/KinquidAI.h"
#include "aion/gameserver/handlers/ai/instance/tallocsHollow/KinquidDebuffAI.h"
#include "aion/gameserver/handlers/ai/instance/tallocsHollow/MosquaEggAI.h"
#include "aion/gameserver/handlers/ai/instance/tallocsHollow/TallocsSummonAI.h"
#include "aion/gameserver/handlers/ai/instance/tallocsHollow/WrithingCocoonAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {
::aion::gameserver::handlers::AIFactory CursedQueenModorAI_aiFactory;
::aion::gameserver::handlers::AIFactory EnragedQueenModorAI_aiFactory;
::aion::gameserver::handlers::AIFactory FinishThemAI_aiFactory;
::aion::gameserver::handlers::AIFactory MaliciousIceStormAI_aiFactory;
::aion::gameserver::handlers::AIFactory ModorsCloneAI_aiFactory;
::aion::gameserver::handlers::AIFactory ModorsPositionIndicatorAI_aiFactory;
::aion::gameserver::handlers::AIFactory VengefulOrbAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
namespace aion::gameserver::handlers::ai::instance::nightmareCircus {
::aion::gameserver::handlers::AIFactory CircusBoxesAI_aiFactory;
::aion::gameserver::handlers::AIFactory EnragedNightmareAI_aiFactory;
::aion::gameserver::handlers::AIFactory HarlequinLordReshkaAI_aiFactory;
::aion::gameserver::handlers::AIFactory HarlequinLordReshkaSummonAI_aiFactory;
::aion::gameserver::handlers::AIFactory MistressViloaAI_aiFactory;
::aion::gameserver::handlers::AIFactory NightmareCrateAI_aiFactory;
::aion::gameserver::handlers::AIFactory NightmareHarlequinAI_aiFactory;
::aion::gameserver::handlers::AIFactory NightmareLordHeiramuneAI_aiFactory;
::aion::gameserver::handlers::AIFactory NightmareLordHeiramuneCloneAI_aiFactory;
::aion::gameserver::handlers::AIFactory SolidIronChainAI_aiFactory;
::aion::gameserver::handlers::AIFactory YumeAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
namespace aion::gameserver::handlers::ai::instance::sauroBase {
::aion::gameserver::handlers::AIFactory BrigadeGeneralShebaAI_aiFactory;
::aion::gameserver::handlers::AIFactory GuardCaptainAhuradim_aiFactory;
::aion::gameserver::handlers::AIFactory GuardCaptainAhuradimGenerator_aiFactory;
::aion::gameserver::handlers::AIFactory MoriataAI_aiFactory;
::aion::gameserver::handlers::AIFactory SauroDroneAI_aiFactory;
::aion::gameserver::handlers::AIFactory SauroFinalTeleportAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::sauroBase
namespace aion::gameserver::handlers::ai::instance::tallocsHollow {
::aion::gameserver::handlers::AIFactory CelestiusAI_aiFactory;
::aion::gameserver::handlers::AIFactory KinquidAI_aiFactory;
::aion::gameserver::handlers::AIFactory KinquidDebuffAI_aiFactory;
::aion::gameserver::handlers::AIFactory MosquaEggAI_aiFactory;
::aion::gameserver::handlers::AIFactory TallocsSummonAI_aiFactory;
::aion::gameserver::handlers::AIFactory WrithingCocoonAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance;
using model::gameobjects::Npc;
using gameserver::ai::poll::AIQuestion;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_PLAY_MOVIE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
struct ShebaProbe : ais::sauroBase::BrigadeGeneralShebaAI {
	using BrigadeGeneralShebaAI::BrigadeGeneralShebaAI;
	using BrigadeGeneralShebaAI::handleDied;
};
struct FinalTeleportProbe : ais::sauroBase::SauroFinalTeleportAI {
	using SauroFinalTeleportAI::SauroFinalTeleportAI;
	using SauroFinalTeleportAI::handleDialogStart;
};

struct MosquaEggProbe : ais::tallocsHollow::MosquaEggAI {
	using MosquaEggAI::MosquaEggAI;
	using MosquaEggAI::handleSpawned;
	using MosquaEggAI::handleDied;
};
struct KinquidProbe : ais::tallocsHollow::KinquidAI {
	using KinquidAI::KinquidAI;
	using KinquidAI::handleCreatureAggro;
	using KinquidAI::handleDied;
};
struct KinquidDebuffProbe : ais::tallocsHollow::KinquidDebuffAI {
	using KinquidDebuffAI::KinquidDebuffAI;
	using KinquidDebuffAI::handleCreatureMoved;
};
struct CocoonProbe : ais::tallocsHollow::WrithingCocoonAI {
	using WrithingCocoonAI::WrithingCocoonAI;
	using WrithingCocoonAI::handleDialogStart;
};
struct TallocsSummonProbe : ais::tallocsHollow::TallocsSummonAI {
	using TallocsSummonAI::TallocsSummonAI;
	using TallocsSummonAI::handleDialogStart;
};

struct EnragedModorProbe : ais::danuarReliquary::EnragedQueenModorAI {
	using EnragedQueenModorAI::EnragedQueenModorAI;
	using EnragedQueenModorAI::handleSpawned;
};
struct CursedModorProbe : ais::danuarReliquary::CursedQueenModorAI {
	using CursedQueenModorAI::CursedQueenModorAI;
	using CursedQueenModorAI::handleSpawned;
};
struct ModorsCloneProbe : ais::danuarReliquary::ModorsCloneAI {
	using ModorsCloneAI::ModorsCloneAI;
	using ModorsCloneAI::handleDied;
};
struct VengefulOrbProbe : ais::danuarReliquary::VengefulOrbAI {
	using VengefulOrbAI::VengefulOrbAI;
	using VengefulOrbAI::handleSpawned;
	using VengefulOrbAI::handleDespawned;
};

struct CircusBoxesProbe : ais::nightmareCircus::CircusBoxesAI {
	using CircusBoxesAI::CircusBoxesAI;
	using CircusBoxesAI::handleSpawned;
	using CircusBoxesAI::handleDespawned;
};
struct ReshkaProbe : ais::nightmareCircus::HarlequinLordReshkaAI {
	using HarlequinLordReshkaAI::HarlequinLordReshkaAI;
	using HarlequinLordReshkaAI::handleSpawned;
	using HarlequinLordReshkaAI::handleDied;
};
struct HeiramuneProbe : ais::nightmareCircus::NightmareLordHeiramuneAI {
	using NightmareLordHeiramuneAI::NightmareLordHeiramuneAI;
	using NightmareLordHeiramuneAI::handleAttack;
	using NightmareLordHeiramuneAI::handleDied;
};
struct ChainProbe : ais::nightmareCircus::SolidIronChainAI {
	using SolidIronChainAI::SolidIronChainAI;
	using SolidIronChainAI::handleDespawned;
};
struct CrateProbe : ais::nightmareCircus::NightmareCrateAI {
	using NightmareCrateAI::NightmareCrateAI;
	using NightmareCrateAI::handleUseItemFinish;
};

class I5AiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(SAURO_SUPPLY_BASE, SauroSupplyBaseInstance_instanceFactory);
		enter(*instance, 900.0f, 890.0f, 411.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 902.0f, 890.0f, 411.0f, int8_t{0}); }

	/** SKILL_DATA with one plain template per id (the base fixture publishes an empty holder; its TearDown resets this one) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="sauro" nameId="1" stack="SAURO" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	static const skillengine::model::SkillTemplate* skill(int32_t id) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(id); }

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The six AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(I5AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::sauroBase::BrigadeGeneralShebaAI*>(ais::sauroBase::BrigadeGeneralShebaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::sauroBase::GuardCaptainAhuradim*>(ais::sauroBase::GuardCaptainAhuradim_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::sauroBase::GuardCaptainAhuradimGenerator*>(ais::sauroBase::GuardCaptainAhuradimGenerator_aiFactory(*npc).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ais::sauroBase::MoriataAI*>(ais::sauroBase::MoriataAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::sauroBase::SauroDroneAI*>(ais::sauroBase::SauroDroneAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::sauroBase::SauroFinalTeleportAI*>(ais::sauroBase::SauroFinalTeleportAI_aiFactory(*npc).get()), nullptr);
}

/** BrigadeGeneralShebaAI.java:25-56: her big skills hit at 55 % while cast; their end resets the multiplier */
TEST_F(I5AiTest, ShebasBigSkillsHitLess) {
	publishSkills({21188, 21189, 21184, 21187});
	runtime::Ptr<Npc> sheba = place(900200);
	ShebaProbe& ai = install<ShebaProbe>(*sheba);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	ai.onStartUseSkill(skill(21188), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 55.0f);
	ai.onEndUseSkill(skill(21188), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	ai.onStartUseSkill(skill(21187), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 55.0f);
	ai.onStartUseSkill(skill(21189), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f) << "the henchman skill hits fully";
}

/** :58-104: the henchmen; the channelers alternate one and eight; her death takes them all */
TEST_F(I5AiTest, ShebasAddsComeAndGo) {
	publishSkills({21189, 21184});
	runtime::Ptr<Npc> sheba = place(900200);
	ShebaProbe& ai = install<ShebaProbe>(*sheba);
	ai.onEndUseSkill(skill(21189), 1);
	EXPECT_EQ(count(284435), 4u);
	ai.onEndUseSkill(skill(21184), 1);
	EXPECT_EQ(count(284436), 1u);
	ai.onEndUseSkill(skill(21184), 1);
	EXPECT_EQ(count(284436), 9u);
	ai.onEndUseSkill(skill(21184), 1);
	EXPECT_EQ(count(284436), 10u) << "the centre again";
	ai.handleDied();
	EXPECT_EQ(count(284435) + count(284436), 0u);
}

/** SauroDroneAI.java:19-23: the self destruct's end removes the drone, another skill does not */
TEST_F(I5AiTest, TheDroneGoesAfterItsSelfDestruct) {
	publishSkills({19498, 21189});
	runtime::Ptr<Npc> drone = place(900200);
	ais::sauroBase::SauroDroneAI& ai = install<ais::sauroBase::SauroDroneAI>(*drone);
	ai.onEndUseSkill(skill(21189), 1);
	EXPECT_TRUE(drone->isSpawned());
	ai.onEndUseSkill(skill(19498), 1);
	EXPECT_FALSE(drone->isSpawned());
}

/** GuardCaptainAhuradimGenerator.java:18-25: without Ahuradim in sight the charge's end shields no one */
TEST_F(I5AiTest, TheGeneratorNeedsAhuradimInSight) {
	publishSkills({21200});
	runtime::Ptr<Npc> generator = place(900200);
	ais::sauroBase::GuardCaptainAhuradimGenerator& ai = install<ais::sauroBase::GuardCaptainAhuradimGenerator>(*generator);
	ai.onEndUseSkill(skill(21200), 1);
	EXPECT_TRUE(generator->isSpawned());
}

/** SauroFinalTeleportAI.java:25-59: the dialog opens 1011; without keys the passage stays closed (1352); other choices do nothing */
TEST_F(I5AiTest, TheHiddenPassageWantsKeys) {
	runtime::Ptr<Npc> passage = place(900200);
	FinalTeleportProbe& ai = install<FinalTeleportProbe>(*passage);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(passage->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(passage->getObjectId(), 1352))), 0);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT_BOSS_LEVEL4, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(passage->getObjectId(), 1352))), 1);
	EXPECT_TRUE(passage->isSpawned());
	EXPECT_EQ(count(730877), 0u);
}

/** The six tallocsHollow AION_AI factories build their classes */
TEST_F(I5AiTest, TheTallocsFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::tallocsHollow::CelestiusAI*>(ais::tallocsHollow::CelestiusAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::tallocsHollow::KinquidAI*>(ais::tallocsHollow::KinquidAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::tallocsHollow::KinquidDebuffAI*>(ais::tallocsHollow::KinquidDebuffAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::tallocsHollow::MosquaEggAI*>(ais::tallocsHollow::MosquaEggAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::tallocsHollow::TallocsSummonAI*>(ais::tallocsHollow::TallocsSummonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::tallocsHollow::WrithingCocoonAI*>(ais::tallocsHollow::WrithingCocoonAI_aiFactory(*npc).get()), nullptr);
}

/** MosquaEggAI.java:24-51: 17 s after its spawn the egg hatches a Supraklaw in its place and goes; no decay, no rewards */
TEST_F(I5AiTest, TheMosquaEggHatches) {
	runtime::Ptr<Npc> egg = place(900200);
	MosquaEggProbe& ai = install<MosquaEggProbe>(*egg);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(16999));
	EXPECT_EQ(count(217132), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count(217132), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 217132)[0]->getX(), 902.0f);
	EXPECT_FALSE(egg->isSpawned());
}

/** :39-43, 53-58: a dead egg hatches nothing */
TEST_F(I5AiTest, ADeadMosquaEggHatchesNothing) {
	runtime::Ptr<Npc> egg = place(900200);
	MosquaEggProbe& ai = install<MosquaEggProbe>(*egg);
	ai.handleSpawned();
	ai.handleDied();
	executor().advance(std::chrono::milliseconds(17000));
	EXPECT_EQ(count(217132), 0u);
}

/** KinquidAI.java:29-39, 100-110: his aggro brings a destroyer at once and a new one every 25 s; his death stops them */
TEST_F(I5AiTest, KinquidsDestroyers) {
	runtime::Ptr<Npc> kinquid = place(900200);
	KinquidProbe& ai = install<KinquidProbe>(*kinquid);
	ai.handleCreatureAggro(player());
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count(282008) + count(282009), 1u);
	executor().advance(std::chrono::milliseconds(25000));
	EXPECT_EQ(count(282008) + count(282009), 1u) << "the old one goes, a new one comes";
	ai.handleCreatureAggro(player());
	ai.handleDied();
	for (const runtime::Ptr<Npc>& destroyer : npcs(*instance, 282008))
		destroyer->getController().delete_();
	for (const runtime::Ptr<Npc>& destroyer : npcs(*instance, 282009))
		destroyer->getController().delete_();
	executor().advance(std::chrono::milliseconds(50000));
	EXPECT_EQ(count(282008) + count(282009), 0u) << "stopped";
}

/** KinquidDebuffAI.java:20-26: only Kinquid moving close is debuffed (another npc: nothing) */
TEST_F(I5AiTest, TheDestroyerWaitsForKinquid) {
	runtime::Ptr<Npc> destroyer = place(282008);
	KinquidDebuffProbe& ai = install<KinquidDebuffProbe>(*destroyer);
	runtime::Ptr<Npc> other = place(900300);
	ai.handleCreatureMoved(*other);
	EXPECT_TRUE(destroyer->isSpawned());
}

/** WrithingCocoonAI.java:24-56: without the corrosive fluid the cocoon stays shut (1097) */
TEST_F(I5AiTest, TheCocoonWantsTheFluid) {
	runtime::Ptr<Npc> cocoon = place(900200);
	CocoonProbe& ai = install<CocoonProbe>(*cocoon);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(cocoon->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(cocoon->getObjectId(), 1097))), 1);
	EXPECT_TRUE(cocoon->isSpawned());
	EXPECT_EQ(count(799500) + count(799501), 0u);
}

/** TallocsSummonAI.java:28-45: the mercenary choice makes the npc the player's, once */
TEST_F(I5AiTest, ShishirBecomesTheCompanion) {
	runtime::Ptr<Npc> shishir = place(900200);
	TallocsSummonProbe& ai = install<TallocsSummonProbe>(*shishir);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(shishir->getObjectId(), 10))), 1);
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::MAKE_MERCENARY, 0, 0));
	EXPECT_EQ(shishir->getCreatorId(), player().getObjectId());
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::MAKE_MERCENARY, 0, 0)) << "already the player's";
}

/** Five danuarReliquary AION_AI factories build their classes (the Modors: below) */
TEST_F(I5AiTest, TheDanuarFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::FinishThemAI*>(ais::danuarReliquary::FinishThemAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::MaliciousIceStormAI*>(ais::danuarReliquary::MaliciousIceStormAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::ModorsCloneAI*>(ais::danuarReliquary::ModorsCloneAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::ModorsPositionIndicatorAI*>(
		          ais::danuarReliquary::ModorsPositionIndicatorAI_aiFactory(*npc).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::VengefulOrbAI*>(ais::danuarReliquary::VengefulOrbAI_aiFactory(*npc).get()), nullptr);
}

/** FinishThemAI.java:26-30, MaliciousIceStormAI.java:28-37: their own skill's end removes them; the storm hits at half */
TEST_F(I5AiTest, TheBlowAndTheStormGoAfterTheirSkill) {
	publishSkills({21199, 21180});
	runtime::Ptr<Npc> blow = place(900200);
	ais::danuarReliquary::FinishThemAI& finish = install<ais::danuarReliquary::FinishThemAI>(*blow);
	finish.onEndUseSkill(skill(21180), 1);
	EXPECT_TRUE(blow->isSpawned());
	finish.onEndUseSkill(skill(21199), 1);
	EXPECT_FALSE(blow->isSpawned());
	runtime::Ptr<Npc> storm = place(900300);
	ais::danuarReliquary::MaliciousIceStormAI& ice = install<ais::danuarReliquary::MaliciousIceStormAI>(*storm);
	EXPECT_FLOAT_EQ(ice.modifyOwnerDamage(100.0f, player(), nullptr), 50.0f);
	ice.onEndUseSkill(skill(21199), 1);
	EXPECT_TRUE(storm->isSpawned());
	ice.onEndUseSkill(skill(21180), 1);
	EXPECT_FALSE(storm->isSpawned());
}

/** ModorsCloneAI.java:37-105: 135 %, 20 % while casting; the real clone's orb skill brings the orb, her death takes it; 3 s first skill */
TEST_F(I5AiTest, ModorsRealCloneAndHerOrb) {
	publishSkills({21177, 21175});
	runtime::Ptr<Npc> clone = place(284383);
	ModorsCloneProbe& ai = install<ModorsCloneProbe>(*clone);
	EXPECT_EQ(ai.modifyInitialSkillDelay(0), 3000);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 135.0f);
	ai.onStartUseSkill(skill(21175), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 20.0f);
	ai.onEndUseSkill(skill(21175), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 135.0f);
	EXPECT_EQ(count(284387), 0u);
	ai.onStartUseSkill(skill(21177), 1);
	EXPECT_EQ(count(284387), 1u);
	ai.handleDied();
	EXPECT_EQ(count(284387), 0u);
}

/** :43-52: a fake's orb skill brings no orb */
TEST_F(I5AiTest, AFakeCloneBringsNoOrb) {
	publishSkills({21177});
	runtime::Ptr<Npc> fake = place(284384);
	ModorsCloneProbe& ai = install<ModorsCloneProbe>(*fake);
	ai.onStartUseSkill(skill(21177), 1);
	EXPECT_EQ(count(284387), 0u);
}

/** VengefulOrbAI.java:24-48: the despawn cancels the cast; the skill's end removes the orb */
TEST_F(I5AiTest, TheVengefulOrb) {
	publishSkills({21178});
	runtime::Ptr<Npc> orb = place(284387);
	VengefulOrbProbe& ai = install<VengefulOrbProbe>(*orb);
	ai.handleSpawned();
	ai.handleDespawned();
	executor().advance(std::chrono::milliseconds(100)); // cancelled: no cast
	EXPECT_TRUE(orb->isSpawned());
	ai.onEndUseSkill(skill(21178), 1);
	EXPECT_FALSE(orb->isSpawned());
}

/** The two Modors' AION_AI factories build their classes */
TEST_F(I5AiTest, TheModorFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::CursedQueenModorAI*>(ais::danuarReliquary::CursedQueenModorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::danuarReliquary::EnragedQueenModorAI*>(ais::danuarReliquary::EnragedQueenModorAI_aiFactory(*npc).get()), nullptr);
}

/** CursedQueenModorAI.java:83-122, 156-166, 195-226: the wrath hits at 120 %, the sphere at 50 %; the level 4 sphere's end leaves one real and
 * four fake clones in her place */
TEST_F(I5AiTest, CursedModorLeavesHerClones) {
	publishSkills({21171, 21175});
	runtime::Ptr<Npc> modor = place(231304);
	CursedModorProbe& ai = install<CursedModorProbe>(*modor);
	EXPECT_EQ(ai.modifyInitialSkillDelay(3000), 0);
	ai.onStartUseSkill(skill(21171), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 120.0f);
	ai.onStartUseSkill(skill(21175), 4);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 50.0f);
	ai.onEndUseSkill(skill(21175), 4);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	EXPECT_EQ(count(284383), 1u);
	EXPECT_EQ(count(284384), 4u);
	EXPECT_FALSE(modor->isSpawned());
}

/** EnragedQueenModorAI.java:117-149, 194-197: the electric vengeance hits at 35 %, any other skill at 50 %, the end resets; the ice storm's
 * end brings a storm */
TEST_F(I5AiTest, EnragedModorsMultipliersAndIceStorm) {
	publishSkills({21176, 21229, 21179});
	runtime::Ptr<Npc> modor = place(231305);
	EnragedModorProbe& ai = install<EnragedModorProbe>(*modor);
	ai.onStartUseSkill(skill(21176), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 35.0f);
	ai.onStartUseSkill(skill(21229), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 50.0f);
	ai.onEndUseSkill(skill(21229), 1);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	ai.onEndUseSkill(skill(21179), 1);
	EXPECT_EQ(count(284528), 1u);
}

/** :46-53, 156-170, 237-268: the first rend space brings the first stage's adds; Crazed Modor brings the infernal ones */
TEST_F(I5AiTest, EnragedAndCrazedModorsAdds) {
	publishSkills({21165});
	runtime::Ptr<Npc> modor = place(231305);
	EnragedModorProbe& ai = install<EnragedModorProbe>(*modor);
	ai.handleSpawned();
	ai.onStartUseSkill(skill(21165), 1);
	ai.onEndUseSkill(skill(21165), 1);
	EXPECT_EQ(count(284659) + count(284660), 2u);
	EXPECT_EQ(count(284661), 2u);
	runtime::Ptr<Npc> crazed = place(234691);
	EnragedModorProbe& crazedAi = install<EnragedModorProbe>(*crazed);
	crazedAi.handleSpawned();
	crazedAi.onStartUseSkill(skill(21165), 1);
	crazedAi.onEndUseSkill(skill(21165), 1);
	EXPECT_EQ(count(856493) + count(856494), 2u);
	EXPECT_EQ(count(856495), 2u);
}

/** The eleven nightmareCircus AION_AI factories build their classes */
TEST_F(I5AiTest, TheCircusFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::CircusBoxesAI*>(ais::nightmareCircus::CircusBoxesAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::EnragedNightmareAI*>(ais::nightmareCircus::EnragedNightmareAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::HarlequinLordReshkaAI*>(ais::nightmareCircus::HarlequinLordReshkaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::HarlequinLordReshkaSummonAI*>(
		          ais::nightmareCircus::HarlequinLordReshkaSummonAI_aiFactory(*npc).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::MistressViloaAI*>(ais::nightmareCircus::MistressViloaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::NightmareCrateAI*>(ais::nightmareCircus::NightmareCrateAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::NightmareHarlequinAI*>(ais::nightmareCircus::NightmareHarlequinAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::NightmareLordHeiramuneAI*>(
		          ais::nightmareCircus::NightmareLordHeiramuneAI_aiFactory(*npc).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::NightmareLordHeiramuneCloneAI*>(
		          ais::nightmareCircus::NightmareLordHeiramuneCloneAI_aiFactory(*npc).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::SolidIronChainAI*>(ais::nightmareCircus::SolidIronChainAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::nightmareCircus::YumeAI*>(ais::nightmareCircus::YumeAI_aiFactory(*npc).get()), nullptr);
}

/** SolidIronChainAI.java:20-29: it does not think; its despawn plays movie 983 */
TEST_F(I5AiTest, TheChainsDespawnPlaysTheMovie) {
	runtime::Ptr<Npc> chain = place(900200);
	ChainProbe& ai = install<ChainProbe>(*chain);
	EXPECT_FALSE(ai.canThink());
	ai.handleDespawned();
	EXPECT_EQ(countSent(serialized(SM_PLAY_MOVIE(false, 0, 0, 983, true))), 1);
}

/** CircusBoxesAI.java:24-75: 33 s after its spawn a terror box goes and lets out 3 to 5 jesters; another box lets out none; the despawn takes
 * the jesters */
TEST_F(I5AiTest, ACircusBoxLetsOutItsJesters) {
	runtime::Ptr<Npc> box = place(831348);
	CircusBoxesProbe& ai = install<CircusBoxesProbe>(*box);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(32999));
	EXPECT_TRUE(box->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(box->isSpawned());
	EXPECT_GE(count(233462), 3u);
	EXPECT_LE(count(233462), 5u);
	ai.handleDespawned();
	EXPECT_EQ(count(233462), 0u);
	runtime::Ptr<Npc> other = place(900200);
	CircusBoxesProbe& otherAi = install<CircusBoxesProbe>(*other);
	otherAi.handleSpawned();
	executor().advance(std::chrono::seconds(33));
	EXPECT_EQ(count(233462) + count(233463), 0u);
}

/** HarlequinLordReshkaAI.java:27-91: twelve terror boxes at once, twelve nightmare boxes after 36 s; his death takes the boxes */
TEST_F(I5AiTest, ReshkasBoxes) {
	publishSkills({21477});
	runtime::Ptr<Npc> reshka = place(900200);
	ReshkaProbe& ai = install<ReshkaProbe>(*reshka);
	ai.handleSpawned();
	EXPECT_EQ(count(831348), 12u);
	executor().advance(std::chrono::milliseconds(35999));
	EXPECT_EQ(count(831349), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(831349), 12u);
	ai.handleDied();
	EXPECT_EQ(count(831348) + count(831349), 0u);
}

/** NightmareLordHeiramuneAI.java:27-88: below 80 % HP two helpers at once and every 20 s; his death takes them */
TEST_F(I5AiTest, HeiramunesHelpers) {
	runtime::Ptr<Npc> heiramune = place(900200);
	HeiramuneProbe& ai = install<HeiramuneProbe>(*heiramune);
	InstanceKnownListPairing::pair(*heiramune, player());
	heiramune->getAggroList().addDamage(player(), 1, true, std::nullopt);
	heiramune->getLifeStats()->setCurrentHpPercent(79);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count(233457), 2u);
	executor().advance(std::chrono::seconds(20));
	EXPECT_EQ(count(233457), 4u);
	ai.handleDied();
	EXPECT_EQ(count(233457), 0u);
}

/** NightmareCrateAI.java:31-51: without keys the treasure box message */
TEST_F(I5AiTest, ACrateWantsKeys) {
	runtime::Ptr<Npc> crate = place(831745);
	CrateProbe& ai = install<CrateProbe>(*crate);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDEVENT01_TREASUREBOX())), 1);
	EXPECT_TRUE(crate->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
