// M5c D-05 (m5c-plan.md §5, P5-05 / aion_gs_handlers_ai_core): PostboxAI, the AI of the mailbox npcs (m5c-plan.md W-02: without the handler
// AIEngine gave the Poeta and Ishalgen mailboxes 700000 and 700079 a DummyNpcAI whose hooks are empty, so a click did nothing, silently).
//
// Java: data/handlers/ai/PostboxAI.java:22-30. The mailbox row is npc_templates.xml:439517-439521, copied verbatim. The npc is spawned in the
// fixture's Poeta map instance beside the player of tests/cm_ak/ItemPacketTestSupport.h (a real AionConnection whose send queue the cases read),
// and the dialog is started the way CM_SHOW_DIALOG starts it: NpcController::onDialogRequest (talk range) -> DIALOG_START -> the AI.
// This executable links the empty AI registry (a handler target's tests do, AionChunks.cmake), so, as RootAiHandlersTest does for the three
// root handlers, the AION_AI marker is reached through the factory function it defines and the npc gets the AI through replaceAi.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/PostboxAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// The factory function the AION_AI marker of PostboxAI.cpp defines, declared exactly as Registry.ai.gen.cpp declares it (handlers::AIFactory)
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory PostboxAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::gameobjects::Npc;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::player::PlayerMailboxState;

// ServerPacketsOpcodes.java:78
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;

constexpr int32_t MAILBOX = 700000;

/** npc_templates.xml:439517-439521, verbatim: the Poeta mailbox (ai="postbox", talk distance 5, no dialog and no function dialog) */
constexpr std::string_view MAILBOX_XML =
	R"(<npc_template npc_id="700000" level="1" name="mailbox" name_id="350707" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="postbox" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="172" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" can_talk_invisible="false" />
	</npc_template>)";

/** The template of the row, bound through NpcData as the server loads npc_templates.xml; kept for the process like DataManager keeps its own */
const model::templates::npc::NpcTemplate* mailboxTemplate() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, "<npc_templates>" + std::string(MAILBOX_XML) + "</npc_templates>").release();
	}();
	return holder->getNpcTemplate(MAILBOX);
}

class PostboxSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	PostboxSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class PostboxAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// The world holders are published once per process, and this executable's AI fixtures publish theirs without asking
		// (tests/ai/AiWorldTestSupport.h): publishing that set first keeps the order of the suites irrelevant. Its Poeta row is the item
		// fixture's, whose own once-publisher then finds the holders published and leaves them (ItemPacketTestSupport.h).
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		ItemPacketTest::SetUp();
		// the row names its AI ("postbox"); this executable links the empty registry, so the warn mode gives the npc a substitute first
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		// Java PlayerService.loadPlayer: player.setMailbox(new Mailbox(player)) (the fixture's player has none)
		player().setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(player()));
	}

	void TearDown() override {
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
	}

	/** The mailbox spawned `x` on the x axis from the player, level with him at (x, 100, 50) (Java VisibleObjectSpawner.spawnNpc) */
	Npc& mailboxAt(float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, MAILBOX, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<PostboxSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc =
			model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, mailboxTemplate());
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	/**
	 * Gives the npc the handler through the factory its marker defines, in the state a spawned npc's AI is in (a new AI is CREATED, which
	 * handles only the spawn events, AIState.java; the spawn's SPAWNED event moves it to IDLE)
	 */
	gameserver::handlers::ai::PostboxAI& installPostboxAi(Npc& npc) {
		std::unique_ptr<gameserver::ai::AbstractAI> ai = gameserver::handlers::ai::PostboxAI_aiFactory(npc);
		auto* postbox = dynamic_cast<gameserver::handlers::ai::PostboxAI*>(ai.get());
		EXPECT_NE(postbox, nullptr);
		npc.replaceAi(std::move(ai));
		postbox->setStateIfNot(gameserver::ai::AIState::IDLE);
		return *postbox;
	}

	int8_t mailboxState() { return player().getMailbox()->mailBoxState.get(); }

	/** SM_DIALOG_WINDOW.writeImpl (SM_DIALOG_WINDOW.java:29-41) for the MAIL page 18: D(npc), H(18), D(quest 0), H(0), H(mailbox state) */
	static std::vector<uint8_t> mailWindow(int32_t npcObjectId, int32_t state) {
		return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(npcObjectId).H(18).D(0).H(0).H(state));
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
};

TEST_F(PostboxAiTest, TheMarkerDefinesAFactoryThatBuildsAPostboxAiForAnNpc) {
	Npc& mailbox = mailboxAt(103.0f);

	std::unique_ptr<gameserver::ai::AbstractAI> ai = gameserver::handlers::ai::PostboxAI_aiFactory(mailbox);
	ASSERT_TRUE(ai);
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::PostboxAI*>(ai.get()));
	EXPECT_FALSE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>(ai.get())) << "PostboxAI extends NpcAI directly";
	EXPECT_EQ(gameserver::handlers::ai::PostboxAI_aiFactory(player()), nullptr) << "a Player is no Npc";
}

TEST_F(PostboxAiTest, TalkingToTheMailboxOpensTheRegularMailWindow) {
	Npc& mailbox = mailboxAt(103.0f); // 3 m: inside talk distance 5 + 1 (PositionUtil.isInTalkRange)
	installPostboxAi(mailbox);
	ASSERT_EQ(mailboxState(), PlayerMailboxState::CLOSED);

	mailbox.getController().onDialogRequest(player());

	// PostboxAI.java:24-25: the state is set before the packet is built, and SM_DIALOG_WINDOW writes it for the MAIL page
	EXPECT_EQ(mailboxState(), PlayerMailboxState::REGULAR);
	EXPECT_EQ(sent(), exactly({mailWindow(mailbox.getObjectId(), PlayerMailboxState::REGULAR)}));
}

TEST_F(PostboxAiTest, TheMailboxOutOfTalkRangeSaysItIsTooFarAndOpensNothing) {
	Npc& mailbox = mailboxAt(120.0f); // 20 m
	installPostboxAi(mailbox);

	mailbox.getController().onDialogRequest(player());

	// NpcController.java:252-257: the mailbox row has no is_dialog, so the warehouse message
	EXPECT_EQ(mailboxState(), PlayerMailboxState::CLOSED);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_TOO_FAR_FROM_NPC())}));
}

TEST_F(PostboxAiTest, ClosingTheDialogClosesTheMailboxAndTheFinishHookSendsNothing) {
	Npc& mailbox = mailboxAt(103.0f);
	installPostboxAi(mailbox);
	mailbox.getController().onDialogRequest(player());
	clearSent();

	// CM_CLOSE_DIALOG -> DialogService.onCloseDialog: DIALOG_FINISH to the AI (PostboxAI.handleDialogFinish is empty), then the open mailbox
	// is closed (DialogService.java:57-66)
	services::DialogService::onCloseDialog(player(), runtime::Ptr<model::gameobjects::VisibleObject>(mailbox));

	EXPECT_EQ(mailboxState(), PlayerMailboxState::CLOSED);
	EXPECT_TRUE(sent().empty());
}

TEST_F(PostboxAiTest, APlayerWithoutAMailboxIsJavasNullPointerException) {
	Npc& mailbox = mailboxAt(103.0f);
	installPostboxAi(mailbox);
	player().setMailbox(nullptr);

	// PostboxAI.java:24 dereferences player.getMailbox() without a check
	EXPECT_THROW(mailbox.getController().onDialogRequest(player()), runtime::NullPointerException);
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
