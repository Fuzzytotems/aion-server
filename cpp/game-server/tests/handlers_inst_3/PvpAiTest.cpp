// P6I-8a (docs/design/p6-instances-plan.md §4): the AIs of the PvP instances (idgelDome, kamarBf, engulfedOphidianBridgeInstance in
// game-server/data/handlers/ai/instance) on npcs of real instances, each AI installed with replaceAi. The skills (no skill rows), the
// scrolls and siege powers (no item rows), the portals' teleports (HousingService loads from the database) are left to a gate.
//
// LINK WORKAROUND (the one tests/handlers_ai_core/TravelAiHandlersTest.cpp describes): KamarTeleportAI extends A1's PortalAI, and A1's handler
// library is not linked into this executable, so this file compiles PortalAI.cpp itself; it is the only translation unit here that does.
#include "aion/gameserver/handlers/ai/portals/PortalAI.cpp"

#include "I3TestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/ExplosionDeviceAI.h"
#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/GeneratorsAI.h"
#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/RaceGeneralsAI.h"
#include "aion/gameserver/handlers/ai/instance/engulfedOphidianBridgeInstance/TeleportsAI.h"
#include "aion/gameserver/handlers/ai/instance/idgelDome/DestroyerKunaxAI.h"
#include "aion/gameserver/handlers/ai/instance/idgelDome/FlameVentAI.h"
#include "aion/gameserver/handlers/ai/instance/idgelDome/RepellingFlameCannonAI.h"
#include "aion/gameserver/handlers/ai/instance/idgelDome/UnstableIdeEnergyAI.h"
#include "aion/gameserver/handlers/ai/instance/kamarBf/KamarBossesAI.h"
#include "aion/gameserver/handlers/ai/instance/kamarBf/KamarCannonsAI.h"
#include "aion/gameserver/handlers/ai/instance/kamarBf/KamarTeleportAI.h"
#include "aion/gameserver/handlers/ai/instance/kamarBf/ReianSupplyItemsAI.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::instance {
namespace idgelDome {
::aion::gameserver::handlers::AIFactory DestroyerKunaxAI_aiFactory;
::aion::gameserver::handlers::AIFactory FlameVentAI_aiFactory;
::aion::gameserver::handlers::AIFactory RepellingFlameCannonAI_aiFactory;
::aion::gameserver::handlers::AIFactory UnstableIdeEnergyAI_aiFactory;
} // namespace idgelDome
namespace kamarBf {
::aion::gameserver::handlers::AIFactory KamarBossesAI_aiFactory;
::aion::gameserver::handlers::AIFactory KamarCannonsAI_aiFactory;
::aion::gameserver::handlers::AIFactory KamarTeleportAI_aiFactory;
::aion::gameserver::handlers::AIFactory ReianSupplyItemsAI_aiFactory;
} // namespace kamarBf
namespace engulfedOphidianBridgeInstance {
::aion::gameserver::handlers::AIFactory ExplosionDeviceAI_aiFactory;
::aion::gameserver::handlers::AIFactory GeneratorsAI_aiFactory;
::aion::gameserver::handlers::AIFactory RaceGeneralsAI_aiFactory;
::aion::gameserver::handlers::AIFactory TeleportsAI_aiFactory;
} // namespace engulfedOphidianBridgeInstance
} // namespace aion::gameserver::handlers::ai::instance

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
struct FlameVentProbe : ais::idgelDome::FlameVentAI {
	using FlameVentAI::FlameVentAI;
	using FlameVentAI::handleUseItemFinish;
};
struct BossesProbe : ais::kamarBf::KamarBossesAI {
	using KamarBossesAI::KamarBossesAI;
	using KamarBossesAI::handleDespawned;
	using KamarBossesAI::handleDied;
	using KamarBossesAI::handleSpawned;
};
struct CannonsProbe : ais::kamarBf::KamarCannonsAI {
	using KamarCannonsAI::KamarCannonsAI;
	using KamarCannonsAI::handleDied;
	using KamarCannonsAI::handleSpawned;
	using KamarCannonsAI::handleUseItemFinish;
};
struct KamarTeleportProbe : ais::kamarBf::KamarTeleportAI {
	using KamarTeleportAI::KamarTeleportAI;
	using KamarTeleportAI::handleUseItemFinish;
};
struct SupplyProbe : ais::kamarBf::ReianSupplyItemsAI {
	using ReianSupplyItemsAI::ReianSupplyItemsAI;
	using ReianSupplyItemsAI::handleDespawned;
	using ReianSupplyItemsAI::handleSpawned;
};
struct ExplosionProbe : ais::engulfedOphidianBridgeInstance::ExplosionDeviceAI {
	using ExplosionDeviceAI::ExplosionDeviceAI;
	using ExplosionDeviceAI::handleUseItemFinish;
};
struct GeneratorsProbe : ais::engulfedOphidianBridgeInstance::GeneratorsAI {
	using GeneratorsAI::GeneratorsAI;
	using GeneratorsAI::handleDied;
	using GeneratorsAI::handleSpawned;
};
struct GeneralsProbe : ais::engulfedOphidianBridgeInstance::RaceGeneralsAI {
	using RaceGeneralsAI::RaceGeneralsAI;
	using RaceGeneralsAI::handleDialogStart;
};
struct TeleportsProbe : ais::engulfedOphidianBridgeInstance::TeleportsAI {
	using TeleportsAI::TeleportsAI;
	using TeleportsAI::handleUseItemFinish;
};

class PvpAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(OPHIDAN_BRIDGE, OphidanBridgeInstance_instanceFactory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, int32_t staticId = 0) {
		if (staticId != 0)
			return spawnStaticNpc(*instance, npcId, 302.0f, 300.0f, 300.0f, staticId);
		return spawnNpc(*instance, npcId, 302.0f, 300.0f, 300.0f, int8_t{0});
	}

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	world::WorldMapInstance* instance = nullptr;
};

/** The twelve AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(PvpAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::idgelDome::DestroyerKunaxAI*>(ais::idgelDome::DestroyerKunaxAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::idgelDome::FlameVentAI*>(ais::idgelDome::FlameVentAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::idgelDome::RepellingFlameCannonAI*>(ais::idgelDome::RepellingFlameCannonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::idgelDome::UnstableIdeEnergyAI*>(ais::idgelDome::UnstableIdeEnergyAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kamarBf::KamarBossesAI*>(ais::kamarBf::KamarBossesAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kamarBf::KamarCannonsAI*>(ais::kamarBf::KamarCannonsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kamarBf::KamarTeleportAI*>(ais::kamarBf::KamarTeleportAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kamarBf::ReianSupplyItemsAI*>(ais::kamarBf::ReianSupplyItemsAI_aiFactory(*npc).get()), nullptr);
	namespace eob = ais::engulfedOphidianBridgeInstance;
	EXPECT_NE(dynamic_cast<eob::ExplosionDeviceAI*>(eob::ExplosionDeviceAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eob::GeneratorsAI*>(eob::GeneratorsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eob::RaceGeneralsAI*>(eob::RaceGeneralsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eob::TeleportsAI*>(eob::TeleportsAI_aiFactory(*npc).get()), nullptr);
}

/** RepellingFlameCannonAI.java:25-28, UnstableIdeEnergyAI.java:33-36: no damage */
TEST_F(PvpAiTest, TheCannonAndTheEnergyTakeNoDamage) {
	runtime::Ptr<Npc> cannon = place(855010);
	auto& c = install<ais::idgelDome::RepellingFlameCannonAI>(*cannon);
	EXPECT_EQ(c.modifyDamage(player(), 500.0f, nullptr), 0.0f);
	runtime::Ptr<Npc> energy = place(900200);
	auto& e = install<ais::idgelDome::UnstableIdeEnergyAI>(*energy);
	EXPECT_EQ(e.modifyDamage(player(), 500.0f, nullptr), 0.0f);
}

/** FlameVentAI.java:23-51: the lever spawns a cannon and two effects on its side for a minute, and dies */
TEST_F(PvpAiTest, AFlameVentLeverFiresForAMinute) {
	runtime::Ptr<Npc> lever = place(802549);
	install<FlameVentProbe>(*lever).handleUseItemFinish(player());
	ASSERT_EQ(count({855010}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 855010).front()->getX(), 294.62436f) << "802549's side";
	EXPECT_EQ(count({702405}), 2u);
	EXPECT_TRUE(lever->isDead());
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_EQ(count({855010, 702405}), 3u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({855010, 702405}), 0u);
}

/** KamarBossesAI.java:39-82: a general's flag comes with it and goes with it */
TEST_F(PvpAiTest, AKamarGeneralsFlag) {
	runtime::Ptr<Npc> general = place(232857);
	BossesProbe& ai = install<BossesProbe>(*general);
	ai.handleSpawned();
	ASSERT_EQ(count({801957}), 1u);
	ai.handleDied();
	EXPECT_EQ(count({801957}), 0u);
	runtime::Ptr<Npc> other = place(232854);
	BossesProbe& second = install<BossesProbe>(*other);
	second.handleSpawned();
	EXPECT_EQ(count({801956}), 1u);
	second.handleDespawned();
	EXPECT_EQ(count({801956}), 0u);
}

/** KamarCannonsAI.java:28-67: the siege weapons have a flag (801961 for 701808, else 801960), the cannons none; without a Siege Power
 * the weapon says so and stays */
TEST_F(PvpAiTest, KamarsSiegeWeapons) {
	runtime::Ptr<Npc> weapon = place(701808);
	CannonsProbe& ai = install<CannonsProbe>(*weapon);
	ai.handleSpawned();
	EXPECT_EQ(count({801961}), 1u);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDKAMAR_CANT_USE_SEIGEWEAPON())), 1);
	EXPECT_TRUE(weapon->isSpawned());
	ai.handleDied();
	EXPECT_EQ(count({801961}), 0u);
	install<CannonsProbe>(*place(701912)).handleSpawned();
	EXPECT_EQ(count({801960}), 1u);
	install<CannonsProbe>(*place(701806)).handleSpawned();
	EXPECT_EQ(count({801960, 801961}), 1u) << "the cannons have no flag";
}

/** KamarTeleportAI.java:28-39: without uses left (none before the spawn) the portal says so */
TEST_F(PvpAiTest, AnUnspawnedKamarTeleporterHasNoUses) {
	runtime::Ptr<Npc> teleporter = place(900200);
	install<KamarTeleportProbe>(*teleporter).handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1500906, std::vector<std::string>{}))), 1);
}

/** ReianSupplyItemsAI.java:22-40: the supply's flag */
TEST_F(PvpAiTest, AReianSupplysFlag) {
	runtime::Ptr<Npc> supply = place(900200);
	SupplyProbe& ai = install<SupplyProbe>(*supply);
	ai.handleSpawned();
	EXPECT_EQ(count({801959}), 1u);
	ai.handleDespawned();
	EXPECT_EQ(count({801959}), 0u);
}

/** GeneratorsAI.java:22-91: the flag by the spawn's static id, gone with the generator */
TEST_F(PvpAiTest, AGeneratorsFlagByItsBase) {
	runtime::Ptr<Npc> generator = place(701943, 175);
	GeneratorsProbe& ai = install<GeneratorsProbe>(*generator);
	ai.handleSpawned();
	EXPECT_EQ(count({802036}), 1u) << "175: the Elyos north flag";
	EXPECT_EQ(count({802038, 802037}), 0u);
	ai.handleDied();
	EXPECT_EQ(count({802036}), 0u);
}

/** ExplosionDeviceAI.java:28-76, 99-104: without a scroll the device spawns no bombs */
TEST_F(PvpAiTest, AnExplosionDeviceWantsAScroll) {
	runtime::Ptr<Npc> device = place(701969);
	install<ExplosionProbe>(*device).handleUseItemFinish(player());
	EXPECT_EQ(count({701939, 701953}), 0u);
}

/** RaceGeneralsAI.java:24-119: the Asmodian general sends his men to the chosen base, the race's other generals go, and so does he */
TEST_F(PvpAiTest, TheRaceGeneralsGoToTheChosenBase) {
	runtime::Ptr<Npc> general = place(701989);
	runtime::Ptr<Npc> other1 = place(701987);
	runtime::Ptr<Npc> other2 = place(701985);
	runtime::Ptr<Npc> elyos = place(701986);
	GeneralsProbe& ai = install<GeneralsProbe>(*general);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(general->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_FALSE(other1->isSpawned());
	EXPECT_FALSE(other2->isSpawned());
	EXPECT_TRUE(elyos->isSpawned());
	ASSERT_EQ(count({233495}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 233495).front()->getX(), 519.66113f);
	EXPECT_EQ(count({233493}), 2u);
	EXPECT_FALSE(general->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(general->getObjectId(), 0))), 1);

	runtime::Ptr<Npc> elyosGeneral = place(701988);
	install<GeneralsProbe>(*elyosGeneral).onDialogSelect(player(), model::DialogAction::SETPRO4, 0, 0);
	EXPECT_FALSE(elyos->isSpawned());
	ASSERT_EQ(count({233494}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 233494).front()->getX(), 478.23563f);
	EXPECT_EQ(count({233492}), 2u);
}

/** TeleportsAI.java:22-48: the use opens the dialog; without a scroll no teleport, the dialog closes */
TEST_F(PvpAiTest, TheTeleportersWantAScroll) {
	runtime::Ptr<Npc> teleporter = place(900200);
	TeleportsProbe& ai = install<TeleportsProbe>(*teleporter);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(teleporter->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(teleporter->getObjectId(), 0))), 1);
	EXPECT_EQ(player().getWorldId(), OPHIDAN_BRIDGE);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
