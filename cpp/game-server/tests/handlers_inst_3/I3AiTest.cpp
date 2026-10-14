// P6I-5 (docs/design/p6-instances-plan.md §4): the AIs of kromedesTrial and linkgateFoundry (game-server/data/handlers/ai/instance) on npcs of
// a real Kromede's Trial instance, each AI installed with replaceAi. The item grants (no item rows), the buffs (no skill rows) and the
// teleports (HousingService loads from the database) are left to a gate.

#include "I3TestSupport.h"

#include <cstdint>
#include <memory>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesBuffAI.h"
#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesCorpseAI.h"
#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesItemNpcsAI.h"
#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesMagasAI.h"
#include "aion/gameserver/handlers/ai/instance/kromedesTrial/KromedesPrisonersAI.h"
#include "aion/gameserver/handlers/ai/instance/linkgateFoundry/LinkgateFoundryBossTeleportAI.h"
#include "aion/gameserver/handlers/ai/instance/linkgateFoundry/LinkgateFoundrySecretTeleportAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::instance {
namespace kromedesTrial {
::aion::gameserver::handlers::AIFactory KromedesBuffAI_aiFactory;
::aion::gameserver::handlers::AIFactory KromedesCorpseAI_aiFactory;
::aion::gameserver::handlers::AIFactory KromedesItemNpcsAI_aiFactory;
::aion::gameserver::handlers::AIFactory KromedesMagasAI_aiFactory;
::aion::gameserver::handlers::AIFactory KromedesPrisonersAI_aiFactory;
} // namespace kromedesTrial
namespace linkgateFoundry {
::aion::gameserver::handlers::AIFactory LinkgateFoundryBossTeleportAI_aiFactory;
::aion::gameserver::handlers::AIFactory LinkgateFoundrySecretTeleportAI_aiFactory;
} // namespace linkgateFoundry
} // namespace aion::gameserver::handlers::ai::instance

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;

// the protected handlers the cases drive directly (Java: the AI's events)
struct PrisonersProbe : ais::kromedesTrial::KromedesPrisonersAI {
	using KromedesPrisonersAI::KromedesPrisonersAI;
	using KromedesPrisonersAI::handleDialogStart;
};
struct MagasProbe : ais::kromedesTrial::KromedesMagasAI {
	using KromedesMagasAI::KromedesMagasAI;
	using KromedesMagasAI::handleDialogStart;
};
struct ItemNpcsProbe : ais::kromedesTrial::KromedesItemNpcsAI {
	using KromedesItemNpcsAI::KromedesItemNpcsAI;
	using KromedesItemNpcsAI::handleUseItemFinish;
};
struct BossTeleportProbe : ais::linkgateFoundry::LinkgateFoundryBossTeleportAI {
	using LinkgateFoundryBossTeleportAI::LinkgateFoundryBossTeleportAI;
	using LinkgateFoundryBossTeleportAI::handleUseItemFinish;
};

class I3AiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(KROMEDES_TRIAL, KromedesTrialInstance_instanceFactory);
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

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 302.0f, 300.0f, 300.0f, int8_t{0}); }

	world::WorldMapInstance* instance = nullptr;
};

/** The seven AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(I3AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::kromedesTrial::KromedesBuffAI*>(ais::kromedesTrial::KromedesBuffAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kromedesTrial::KromedesCorpseAI*>(ais::kromedesTrial::KromedesCorpseAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kromedesTrial::KromedesItemNpcsAI*>(ais::kromedesTrial::KromedesItemNpcsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kromedesTrial::KromedesMagasAI*>(ais::kromedesTrial::KromedesMagasAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::kromedesTrial::KromedesPrisonersAI*>(ais::kromedesTrial::KromedesPrisonersAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::linkgateFoundry::LinkgateFoundryBossTeleportAI*>(
		          ais::linkgateFoundry::LinkgateFoundryBossTeleportAI_aiFactory(*npc).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ais::linkgateFoundry::LinkgateFoundrySecretTeleportAI*>(
		          ais::linkgateFoundry::LinkgateFoundrySecretTeleportAI_aiFactory(*npc).get()),
		nullptr);
}

/** KromedesPrisonersAI.java:23-35: the dialog opens 1011, the second page 1012, the first choice frees the prisoner */
TEST_F(I3AiTest, APrisonerGoesWhenFreed) {
	runtime::Ptr<Npc> prisoner = place(900200);
	PrisonersProbe& ai = install<PrisonersProbe>(*prisoner);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(prisoner->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(prisoner->getObjectId(), 1012))), 1);
	EXPECT_TRUE(prisoner->isSpawned());
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_FALSE(prisoner->isSpawned());
}

/** KromedesMagasAI.java:25-41: without a relic key Magas has no right page for the potion */
TEST_F(I3AiTest, MagasWantsTheRelicKey) {
	runtime::Ptr<Npc> magas = place(900200);
	MagasProbe& ai = install<MagasProbe>(*magas);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(magas->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(magas->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(magas->getObjectId(), 1012))), 1);
}

/** KromedesItemNpcsAI.java:26-62: the use opens the dialog; another npc's choice gives nothing */
TEST_F(I3AiTest, TheItemObjectsOpenTheirDialog) {
	runtime::Ptr<Npc> object = place(900200);
	ItemNpcsProbe& ai = install<ItemNpcsProbe>(*object);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(object->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(object->getObjectId(), 1012))), 0) << "900200 is none of the three";
}

/** LinkgateFoundryBossTeleportAI.java:28-82: without the keys no boss; an unknown choice does nothing */
TEST_F(I3AiTest, TheBossPortalWantsTheKeys) {
	runtime::Ptr<Npc> portal = place(900200);
	BossTeleportProbe& ai = install<BossTeleportProbe>(*portal);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(portal->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(portal->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 0);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT_BOSS_LEVEL2, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(portal->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_TRUE(portal->isSpawned());
	EXPECT_TRUE(npcs(*instance, 234990).empty());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
