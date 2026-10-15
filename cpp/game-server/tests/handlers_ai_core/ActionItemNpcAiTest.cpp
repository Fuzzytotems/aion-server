// M5d's quest-npc-ais lane (m5d-plan.md A-02 and A-04; P5-05 / aion_gs_handlers_ai_core): ActionItemNpcAI, the AI of the 489 npc templates
// with ai="useitem" and the superclass of QuestItemNpcAI (ActionItemNpcAI.java:24-99).
//
// The npcs stand in a Poeta map instance of their own whose instance handler records the calls AIActions.handleUseItemFinish makes
// (AIActions.java: the handler of the owner's instance; GeneralInstanceHandler's own body is empty, so on a world map the finish of a plain
// useitem npc does nothing else). The user is the player of tests/cm_ak/ItemPacketTestSupport.h, moved into that instance: a real AionConnection
// whose send queue the cases read, and the fixture's DeterministicExecutor on its ManualClock, which the cases advance through the use bar.
// A click is started the way CM_SHOW_DIALOG starts it: NpcController::onDialogRequest (talk range) -> DIALOG_START -> the AI.
//
// The rows are npc_templates.xml:121225-121229 (218610 defensive artillery: talk distance 1, talk delay 3 s) and :8883-8887 (203671 altgard
// teleport device: no talk delay), verbatim (QuestNpcAiTestSupport.h). Expected packets: SM_USE_OBJECT as Java writes it (SM_USE_OBJECT.java:25-30,
// opcode 197); SM_EMOTION, whose body the server builds from the player's state and speeds, as the server serializes the SM_EMOTION Java
// constructs there (its bytes are pinned by the packet tests of tests/sm_ak).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"
#include "QuestNpcAiTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/effect/SummonOwner.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// The factory function the AION_AI marker of ActionItemNpcAI.cpp defines, declared exactly as Registry.ai.gen.cpp declares it
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory ActionItemNpcAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;

using gameserver::ai::event::AIEventType;
using model::EmotionType;
using model::TaskId;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_EMOTION;

// ServerPacketsOpcodes.java:215
constexpr int32_t SM_USE_OBJECT_OPCODE = 197;

/** SM_USE_OBJECT.writeImpl (SM_USE_OBJECT.java:25-30): D(player), D(target), D(time), C(actionType) */
std::vector<uint8_t> useObject(int32_t playerObjId, int32_t targetObjId, int32_t time, int32_t actionType) {
	return javaPacket(SM_USE_OBJECT_OPCODE, PacketWriter().D(playerObjId).D(targetObjId).D(time).C(actionType));
}

/** The rows of QuestNpcAiTestSupport.h, bound through NpcData as the server loads npc_templates.xml; kept for the process like DataManager's */
const model::templates::npc::NpcTemplate* templateOf(int32_t npcId) {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(
			context, "<npc_templates>" + std::string(gameserver::ai::testing::QUEST_NPC_AI_TEMPLATES_XML) + "</npc_templates>")
			.release();
	}();
	return holder->getNpcTemplate(npcId);
}

/** GeneralInstanceHandler recording the calls of handleUseItemFinish (player object id, npc object id), which AIActions.handleUseItemFinish makes */
class UseItemFinishRecorder final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit UseItemFinishRecorder(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<UseItemFinishRecorder> create(world::WorldMapInstance& instance) { return runtime::makeRef<UseItemFinishRecorder>(instance); }

	void handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player> player, Npc& npc) override {
		finishes.emplace_back(player ? player->getObjectId() : 0, npc.getObjectId());
	}

	std::vector<std::pair<int32_t, int32_t>> finishes;

protected:
	~UseItemFinishRecorder() override = default;
};

class UseItemSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	UseItemSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class ActionItemNpcAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// the world holders are published once per process, and this executable's AI fixtures publish theirs without asking
		// (tests/ai/AiWorldTestSupport.h): publishing that set first keeps the order of the suites irrelevant (as AbyssGuardDialogTest.cpp does)
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		// Player::postConstruct loads the toy pets from the database; the tests have none (as tests/quest_handlers' fixture does)
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		// the rows name their AI ("useitem"); this executable links the empty registry, so the warn mode gives an npc a substitute until a case
		// installs the handler through its factory
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		useInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [this](world::WorldMapInstance& instance) {
			recorder = UseItemFinishRecorder::create(instance);
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(recorder);
		});
		standIn(player());
	}

	void TearDown() override {
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (second.player) {
			second.player->setClientConnection(nullptr);
			secondClient.reset();
			second = {};
		}
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		useInstance->detachInstanceHandler();
		recorder = nullptr;
		useInstance = nullptr; // the player's position keeps its region until ItemPacketTest releases him
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** Places a player at (100, 100, 50) of the recording instance (Java World.setPosition; spawned) */
	void standIn(model::gameobjects::player::Player& user) {
		user.setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		user.getPosition()->setIsSpawned(true);
	}

	/** An npc of the row, spawned `dx` on the x axis from the player, level with him (Java VisibleObjectSpawner.spawnNpc) */
	Npc& npcAt(int32_t npcId, float dx) {
		const float x = 100.0f + dx;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<UseItemSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, templateOf(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	/** The handler through the factory its marker defines, idle as a spawned npc's AI is (the spawn's SPAWNED event moves it to IDLE) */
	roots::ActionItemNpcAI& installAi(Npc& npc) {
		std::unique_ptr<gameserver::ai::AbstractAI> ai = roots::ActionItemNpcAI_aiFactory(npc);
		auto* typed = dynamic_cast<roots::ActionItemNpcAI*>(ai.get());
		EXPECT_NE(typed, nullptr);
		npc.replaceAi(std::move(ai));
		typed->setStateIfNot(gameserver::ai::AIState::IDLE);
		return *typed;
	}

	/** A second user with his own client, standing beside the first */
	model::gameobjects::player::Player& secondUser() {
		second = makePlayer(710102, 9902, "Second");
		standIn(*second.player);
		secondClient = std::make_unique<TestClient>();
		secondClient->enterWorld(second);
		(*secondClient)->clearSent();
		return *second.player;
	}

	std::vector<std::vector<uint8_t>> sentToSecond() { return (*secondClient)->sentBytes(); }

	/**
	 * SM_EMOTION(user, type, 0, target) (ActionItemNpcAI.java:49, 64, 66) as the server serializes it for the connection of `receiver`: the
	 * user's own client, or a watcher's
	 */
	std::vector<uint8_t> emotion(model::gameobjects::player::Player& user, TestClient& receiver, EmotionType type, int32_t target) {
		return serialized(SM_EMOTION(user, type, 0, target), receiver.con());
	}

	std::vector<uint8_t> emotion(EmotionType type, int32_t target) { return emotion(player(), *client, type, target); }

	/** The player moves: PlayerMoveController / CM_MOVE notify the move observers (ObserveController.notifyMoveObservers) */
	void move(model::gameobjects::player::Player& user) { user.getObserveController()->notifyMoveObservers(); }

	/**
	 * Ends the fixture's task scope, lets the Reclaimer destroy what is no longer referenced (a released object's own Refs are released only by
	 * its destructor, runtime/lifetime/Reclaimer.h) and opens a new scope for the rest of the case
	 */
	void settle() {
		scope.reset();
		runtime::Reclaimer::getInstance().drain();
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	runtime::Ref<world::WorldMapInstance> useInstance;
	runtime::Ref<UseItemFinishRecorder> recorder;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	PlayerFixture second;
	std::unique_ptr<TestClient> secondClient;
};

using Finishes = std::vector<std::pair<int32_t, int32_t>>;

// ActionItemNpcAI.java:35-38, 41-73: a click through the interaction check starts the bar - SM_USE_OBJECT(talk delay in ms, startBarAnimation 1)
// to the user, then the START_QUESTLOOT emotion broadcast to him and his watchers; an ItemUseObserver in his ObserveController and the task
// ACTION_ITEM_NPC in his controller. When the delay has passed the task sends END_QUESTLOOT and SM_USE_OBJECT(delay, cancelBarAnimation 2),
// removes the observer and finishes: AIActions.handleUseItemFinish -> the instance handler's handleUseItemFinish(player, npc).
TEST_F(ActionItemNpcAiTest, AClickRunsTheUseBarAndTheTalkDelayFinishesIt) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f); // inside talk distance 1 + 1 (PositionUtil.isInTalkRange)
	installAi(artillery);
	const int32_t me = player().getObjectId();
	const int32_t target = artillery.getObjectId();

	artillery.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({useObject(me, target, 3000, 1), emotion(EmotionType::START_QUESTLOOT, target)}));
	EXPECT_TRUE(player().getObserveController()->hasObservers());
	EXPECT_TRUE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	EXPECT_EQ(recorder->finishes, Finishes{});

	clearSent();
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_EQ(sent(), exactly({})) << "the bar runs the template's talk delay: 3 s";
	EXPECT_EQ(recorder->finishes, Finishes{});

	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(sent(), exactly({emotion(EmotionType::END_QUESTLOOT, target), useObject(me, target, 3000, 2)}));
	EXPECT_EQ(recorder->finishes, (Finishes{{me, target}}));
	EXPECT_FALSE(player().getObserveController()->hasObservers()) << "the task removes the observer from the user";

	// and from the AI's list: a move no longer aborts anything, the npc's death aborts nothing
	clearSent();
	move(player());
	artillery.getAi().onGeneralEvent(AIEventType::DIED);
	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(recorder->finishes, (Finishes{{me, target}}));
}

// ActionItemNpcAI.java:46-55: a move of the user aborts the bar - the task is cancelled, END_QUESTLOOT is broadcast, SM_USE_OBJECT(0,
// cancelBarAnimation 2) sent, and the observer leaves the AI's list and the user's ObserveController; the use never finishes
TEST_F(ActionItemNpcAiTest, AMoveAbortsTheBarAndTheUseNeverFinishes) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f);
	installAi(artillery);
	const int32_t me = player().getObjectId();
	const int32_t target = artillery.getObjectId();
	artillery.getController().onDialogRequest(player());
	executor->advance(std::chrono::milliseconds(1000));
	clearSent();

	move(player());

	EXPECT_EQ(sent(), exactly({emotion(EmotionType::END_QUESTLOOT, target), useObject(me, target, 0, 2)}));
	EXPECT_FALSE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	EXPECT_FALSE(player().getObserveController()->hasObservers());

	clearSent();
	executor->advance(std::chrono::milliseconds(5000));
	move(player());
	artillery.getAi().onGeneralEvent(AIEventType::DIED); // the observer left the AI's list too: its death aborts nothing
	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(recorder->finishes, Finishes{});
}

// ActionItemNpcAI.java:74-76: without a talk delay the click finishes at once - no bar, no observer, no task
TEST_F(ActionItemNpcAiTest, WithoutATalkDelayTheClickFinishesAtOnce) {
	Npc& device = npcAt(gameserver::ai::testing::ALTGARD_TELEPORT_DEVICE, 3.0f); // talk distance 6
	installAi(device);

	device.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(recorder->finishes, (Finishes{{player().getObjectId(), device.getObjectId()}}));
	EXPECT_FALSE(player().getObserveController()->hasObservers());
	EXPECT_FALSE(player().getController().hasTask(TaskId::ACTION_ITEM_NPC));
}

// ActionItemNpcAI.java:37: DialogService.isInteractionAllowed refuses a summoned npc of another owner (DialogService.java:298-302: a PRIVATE
// summon owner and a creator that is not the user; the npc's creator id is its spawn's, 0) - the click does nothing
TEST_F(ActionItemNpcAiTest, AnNpcTheUserMayNotInteractWithIgnoresTheClick) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f);
	installAi(artillery);
	artillery.setSummonOwner(skillengine::effect::SummonOwner::PRIVATE);

	artillery.getController().onDialogRequest(player());
	executor->advance(std::chrono::milliseconds(5000));

	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(recorder->finishes, Finishes{});
	EXPECT_FALSE(player().getObserveController()->hasObservers());
}

// ActionItemNpcAI.java:87-97: the npc's death aborts every bar still running on it, one abort per user, and the uses never finish
TEST_F(ActionItemNpcAiTest, TheNpcsDeathAbortsTheBarOfEveryUser) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f);
	roots::ActionItemNpcAI& ai = installAi(artillery);
	model::gameobjects::player::Player& other = secondUser();
	const int32_t target = artillery.getObjectId();
	artillery.getController().onDialogRequest(player());
	artillery.getController().onDialogRequest(other);
	clearSent();
	(*secondClient)->clearSent();

	ai.onGeneralEvent(AIEventType::DIED);

	EXPECT_TRUE(ai.isInState(gameserver::ai::AIState::DIED)) << "the superclass's NpcAI.handleDied ran (DiedEventHandler.onDie)";
	EXPECT_EQ(sent(), exactly({emotion(EmotionType::END_QUESTLOOT, target), useObject(player().getObjectId(), target, 0, 2)}));
	EXPECT_EQ(sentToSecond(),
		exactly({emotion(other, *secondClient, EmotionType::END_QUESTLOOT, target), useObject(other.getObjectId(), target, 0, 2)}));
	EXPECT_FALSE(player().getObserveController()->hasObservers());
	EXPECT_FALSE(other.getObserveController()->hasObservers());
	EXPECT_FALSE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	EXPECT_FALSE(other.getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));

	clearSent();
	(*secondClient)->clearSent();
	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(sentToSecond(), exactly({}));
	EXPECT_EQ(recorder->finishes, Finishes{});
}

// ActionItemNpcAI.java:49, 64, 66 against :50, 63, 67: the START_QUESTLOOT and END_QUESTLOOT emotions are broadcast to the user and to every
// player who sees him (PacketSendUtility.broadcastPacket(player, packet, true): sendPacket, then the user's known players), while SM_USE_OBJECT,
// the bar itself, goes to the user alone (sendPacket). Two users who see each other (Java's knownlist update pairs them): the first runs his bar
// to its finish, then the second starts his and moves, which aborts it.
TEST_F(ActionItemNpcAiTest, TheUsersWatchersSeeHisEmotionsButNotHisBar) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f);
	installAi(artillery);
	model::gameobjects::player::Player& other = secondUser();
	ASSERT_TRUE(gameserver::ai::testing::AiKnownListPairing::pair(player(), other));
	const int32_t me = player().getObjectId();
	const int32_t watcher = other.getObjectId();
	const int32_t target = artillery.getObjectId();
	clearSent(); // the two SM_PLAYER_INFO of the pairing
	(*secondClient)->clearSent();

	artillery.getController().onDialogRequest(player());
	EXPECT_EQ(sent(), exactly({useObject(me, target, 3000, 1), emotion(EmotionType::START_QUESTLOOT, target)}));
	EXPECT_EQ(sentToSecond(), exactly({emotion(player(), *secondClient, EmotionType::START_QUESTLOOT, target)}))
		<< "the watcher sees the user's START_QUESTLOOT, not his bar";

	clearSent();
	(*secondClient)->clearSent();
	executor->advance(std::chrono::milliseconds(3000));
	EXPECT_EQ(sent(), exactly({emotion(EmotionType::END_QUESTLOOT, target), useObject(me, target, 3000, 2)}));
	EXPECT_EQ(sentToSecond(), exactly({emotion(player(), *secondClient, EmotionType::END_QUESTLOOT, target)}))
		<< "the watcher sees the finish's END_QUESTLOOT, not the bar's end";
	EXPECT_EQ(recorder->finishes, (Finishes{{me, target}}));

	clearSent();
	(*secondClient)->clearSent();
	artillery.getController().onDialogRequest(other);
	move(other);
	EXPECT_EQ(sentToSecond(), exactly({useObject(watcher, target, 3000, 1), emotion(other, *secondClient, EmotionType::START_QUESTLOOT, target),
								   emotion(other, *secondClient, EmotionType::END_QUESTLOOT, target), useObject(watcher, target, 0, 2)}));
	EXPECT_EQ(sent(), exactly({emotion(other, *client, EmotionType::START_QUESTLOOT, target), emotion(other, *client, EmotionType::END_QUESTLOOT, target)}))
		<< "the first user now watches: the second's START_QUESTLOOT and the END_QUESTLOOT of his abort, neither SM_USE_OBJECT";
	EXPECT_EQ(recorder->finishes, (Finishes{{me, target}})) << "the aborted use never finishes";
}

// Java: a despawn (NpcAI.handleDespawned) leaves a running bar alone - it ends by its task, as it would on a living npc. The C++-only
// ActionItemNpcAI::handleDespawned keeps that: it drops the list's references without aborting.
TEST_F(ActionItemNpcAiTest, ADespawnLeavesARunningBarToItsTask) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f);
	roots::ActionItemNpcAI& ai = installAi(artillery);
	const int32_t me = player().getObjectId();
	const int32_t target = artillery.getObjectId();
	artillery.getController().onDialogRequest(player());
	clearSent();

	ai.onGeneralEvent(AIEventType::DESPAWNED);
	EXPECT_EQ(sent(), exactly({})) << "the despawn aborts nothing";
	EXPECT_TRUE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	EXPECT_TRUE(player().getObserveController()->hasObservers());

	executor->advance(std::chrono::milliseconds(3000));
	EXPECT_EQ(sent(), exactly({emotion(EmotionType::END_QUESTLOOT, target), useObject(me, target, 3000, 2)}));
	EXPECT_EQ(recorder->finishes, (Finishes{{me, target}}));
	EXPECT_FALSE(player().getObserveController()->hasObservers());
}

// ActionItemNpcAI::handleDespawned, the C++-only lifetime breaker (P5-05.md, "M5d stage 1"). The observer of a bar holds its AI, and a Ref to
// the AI part retains the npc (runtime/lifetime/Parts.h), while the AI's list holds the observer: an npc despawned while a bar runs on it,
// whose user then logs out (CreatureController.onDelete cancels his tasks; LogoutBreakers L7 empties his ObserveController without an abort),
// would keep itself alive through its own list. Java's collector frees that pair; here the despawn drops the list, so once the Reclaimer has
// run nothing of the bar holds the npc any more.
TEST_F(ActionItemNpcAiTest, ADespawnedNpcIsNotKeptAliveByABarItsUserLeft) {
	Npc& artillery = npcAt(gameserver::ai::testing::DEFENSIVE_ARTILLERY, 1.0f);
	roots::ActionItemNpcAI& ai = installAi(artillery);
	settle();
	const uint32_t before = artillery.refCount();

	artillery.getController().onDialogRequest(player());
	ai.onGeneralEvent(AIEventType::DESPAWNED);
	player().getController().cancelTask(TaskId::ACTION_ITEM_NPC); // the logout: CreatureController.onDelete -> cancelAllTasks
	player().getObserveController()->clearWithoutNotify();         // LogoutBreakers L7
	executor->advance(std::chrono::milliseconds(5000));
	settle();

	EXPECT_EQ(artillery.refCount(), before) << "the bar's observer, task and pins released the npc";
	EXPECT_EQ(sent(), exactly({useObject(player().getObjectId(), artillery.getObjectId(), 3000, 1),
						  emotion(EmotionType::START_QUESTLOOT, artillery.getObjectId())}))
		<< "and the bar was neither finished nor aborted";
	EXPECT_EQ(recorder->finishes, Finishes{});
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
