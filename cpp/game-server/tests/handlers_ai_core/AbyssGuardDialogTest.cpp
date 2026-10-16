// The ascension lane (m5d-plan.md A-01 and A-04, P5-05 / aion_gs_handlers_ai_core): the reason quest 1007 needs AbyssGuardSimpleAI. Jucleas
// (203752), whom the Daeva reports to in Sanctum, has ai="simple_abyssguard"; without the handler AIEngine gives him the warn-mode DummyNpcAI,
// whose hooks are empty (and whose empty handleSpawned leaves it CREATED, a state in which AbstractAI.canHandleEvent drops DIALOG_START), so a
// click on him does nothing at all. With it the click reaches GeneralNpcAI.handleDialogStart ->
// TalkEventHandler.onTalk (TalkEventHandler.java:22-53): the npc turns to the talker (sub state TALK, target) and, no quest handler taking the
// dialog, sends the start page DialogPage.getStartPageId picks (DialogPage.java:113-125): 1011 for a dialog npc without functions or quests.
// There is no case for the click without the handler: the warn-mode DummyNpcAI never leaves CREATED, so such a case would pass as well with a
// DummyNpcAI that talked (that mutant, in AIEngine.cpp, survived it), and it was dropped (P5-05.md, "Ascension lane").
//
// The template row is npc_templates.xml:9953-9968, verbatim except its <equipment> (QuestNpcAiTestSupport.h). The npc is spawned in the
// fixture's Poeta map instance beside the player of tests/cm_ak/ItemPacketTestSupport.h (a real AionConnection whose send queue the cases
// read), and the dialog is started the way CM_SHOW_DIALOG starts it: NpcController::onDialogRequest (talk range) -> DIALOG_START -> the AI.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"
#include "QuestNpcAiTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/AbyssGuardSimpleAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// The factory function the AION_AI marker of AbyssGuardSimpleAI.cpp defines, declared exactly as Registry.ai.gen.cpp declares it
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory AbyssGuardSimpleAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::gameobjects::Npc;

// ServerPacketsOpcodes.java:78
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;

/** Jucleas' template, bound through NpcData as the server loads npc_templates.xml; kept for the process like DataManager keeps its own */
const model::templates::npc::NpcTemplate* jucleasTemplate() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(
			context, "<npc_templates>" + std::string(gameserver::ai::testing::QUEST_NPC_AI_TEMPLATES_XML) + "</npc_templates>")
			.release();
	}();
	return holder->getNpcTemplate(gameserver::ai::testing::JUCLEAS);
}

class JucleasSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	JucleasSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class AbyssGuardDialogTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// The world holders are published once per process, and this executable's AI fixtures publish theirs without asking
		// (tests/ai/AiWorldTestSupport.h): publishing that set first keeps the order of the suites irrelevant (as PostboxAiTest.cpp does)
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		ItemPacketTest::SetUp();
		// the row names its AI ("simple_abyssguard"); this executable links the empty registry, so the warn mode gives the npc a substitute
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	}

	void TearDown() override {
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
	}

	/** Jucleas spawned `x` on the x axis from the player, level with him at (x, 100, 50) (Java VisibleObjectSpawner.spawnNpc) */
	Npc& jucleasAt(float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group =
			model::templates::spawns::SpawnGroup::create(210010000, gameserver::ai::testing::JUCLEAS, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<JucleasSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc =
			model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, jucleasTemplate());
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	/** The handler through the factory its marker defines, idle as a spawned npc's AI is (the spawn's SPAWNED event moves it to IDLE) */
	gameserver::handlers::ai::AbyssGuardSimpleAI& installGuardAi(Npc& npc) {
		std::unique_ptr<gameserver::ai::AbstractAI> ai = gameserver::handlers::ai::AbyssGuardSimpleAI_aiFactory(npc);
		auto* guard = dynamic_cast<gameserver::handlers::ai::AbyssGuardSimpleAI*>(ai.get());
		EXPECT_NE(guard, nullptr);
		npc.replaceAi(std::move(ai));
		guard->setStateIfNot(gameserver::ai::AIState::IDLE);
		return *guard;
	}

	/** SM_DIALOG_WINDOW.writeImpl (SM_DIALOG_WINDOW.java:29-41) for a page other than MAIL and TOWN_CHALLENGE_TASK: D(npc), H(page), D(0), H(0), H(0) */
	static std::vector<uint8_t> dialogWindow(int32_t npcObjectId, int32_t pageId) {
		return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(npcObjectId).H(pageId).D(0).H(0).H(0));
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
};

TEST_F(AbyssGuardDialogTest, TalkingToJucleasTurnsHimToTheTalkerAndOpensHisStartPage) {
	Npc& jucleas = jucleasAt(103.0f); // 3 m: inside talk distance 5 + 1 (PositionUtil.isInTalkRange)
	gameserver::handlers::ai::AbyssGuardSimpleAI& ai = installGuardAi(jucleas);

	jucleas.getController().onDialogRequest(player());

	// TalkEventHandler.onSimpleTalk (the row has is_dialog="true"): sub state TALK, the talker as target
	EXPECT_TRUE(ai.isInSubState(gameserver::ai::AISubState::TALK));
	EXPECT_TRUE(jucleas.isTargeting(player().getObjectId()));
	// no quest handler takes the dialog, the title is not Oriel's/Pernon's villager title 462877, and getStartPageId answers 1011
	// (HTML_PAGE_SELECT1): a dialog npc, interaction allowed, no function dialogs, no quest interaction, the talker no Daeva
	EXPECT_EQ(sent(), exactly({dialogWindow(jucleas.getObjectId(), 1011)}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
