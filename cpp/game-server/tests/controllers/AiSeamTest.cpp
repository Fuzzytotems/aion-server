// m5b-1 E-01a (m5b-plan.md §4, P4-11b): the AI-facing half of the ControllerStandIns deletion. The four stand-ins the AI lane's A-01/A-02
// made real - TargetEventHandler.onTargetReached, WalkManager.stopWalking, ShoutEventHandler.onEnemyAttack, ShoutEventHandler.onAttack - plus
// aiLoggerMoveinfo are gone, and the controller bodies call the classes directly, exactly as Java writes them
// (NpcMoveController.java:259/389/393 and its AILogger.moveinfo calls, NpcController.java:303, PlayerController.java:451).
//
// Every case below reaches a call site that was an `AION_UNPORTED()` stand-in until this item, so each one fails with an UnportedException if a
// stand-in comes back. Each also asserts what the real callee did - the AI state and substate WalkManager.stopWalking leaves behind, the IDLE
// TargetEventHandler.onTargetReached leaves behind, the CAN_SHOUT question ShoutEventHandler asks first, the line AILogger.moveinfo logs - so a
// call site emptied instead of redirected fails too.
//
// The npc doubles follow the controller tests: a real Npc through VisibleObject::create, with the parts Java's VisibleObjectSpawner gives a
// spawned npc (EffectController, NpcKnownList; VisibleObjectSpawner.java:60-61) added by hand. The AI is a real NpcAI leaf installed with
// replaceAi, because this test executable registers no AI handler.

#include "ControllersTestSupport.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.bind.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::controllers::testing {
namespace {

using ai::AIState;
using ai::AISubState;
using ai::poll::AIQuestion;
using model::gameobjects::state::CreatureState;
using model::templates::walker::WalkerTemplate;
using movement::NpcMoveController;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ptr;
using runtime::Ref;

/**
 * A real NpcAI leaf that records what the ai/handler statics ask it and answers every question false, which is the answer of
 * AITemplate<Npc> (and of AIEngine's DummyNpcAI). It adds no hook, so nothing but the recorded question can come from the AI.
 */
class ProbeNpcAI final : public ai::NpcAI {
public:
	std::vector<AIQuestion> asked;

	explicit ProbeNpcAI(model::gameobjects::Npc& owner) : NpcAI(owner) {}

	bool ask(AIQuestion question) override {
		asked.push_back(question);
		return false;
	}
};

/** Captures the messages of one logger while it exists ("level|message" per line) */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

/** Restores gameserver.dev.missing_ai_handlers, which one case sets to `warn` (docs/deviations/P4-01.md) */
class MissingAiHandlersGuard {
public:
	explicit MissingAiHandlersGuard(std::string value) : previous(*configs::main::AIConfig::MISSING_AI_HANDLERS.get()) {
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(std::move(value));
	}
	~MissingAiHandlersGuard() { configs::main::AIConfig::MISSING_AI_HANDLERS.set(previous); }
	MissingAiHandlersGuard(const MissingAiHandlersGuard&) = delete;
	MissingAiHandlersGuard& operator=(const MissingAiHandlersGuard&) = delete;

private:
	std::string previous;
};

/** Npc templates are immortal static data, like the holder's own (ControllersTestSupport.h) */
const model::templates::npc::NpcTemplate* npcTemplateFrom(std::string_view xmlText) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context, std::string(xmlText)).release();
}

/** A walker with three steps and loop_type=NONE, so the third step is the last one and sets NpcMoveController.isStop */
const WalkerTemplate* routeOfThreeSteps() {
	static const WalkerTemplate* route = [] {
		xml::LoadContext context;
		return xml::bindString<WalkerTemplate>(context,
			std::string(R"(<walker_template route_id="M5B_E01A" loop_type="NONE">)")
				+ R"(<routestep x="100" y="200" z="300" rest_time="0"/>)"
				  R"(<routestep x="110" y="200" z="300" rest_time="0"/>)"
				  R"(<routestep x="120" y="200" z="300" rest_time="0"/>)"
				  R"(</walker_template>)")
			.release();
	}();
	return route;
}

class AiSeamTest : public ControllersTest {
protected:
	/** The npc of the controller tests plus the two parts VisibleObjectSpawner adds when it spawns one */
	Ref<ControllersTestNpc> createSpawnableNpc(const model::templates::npc::NpcTemplate* objectTemplate) {
		Ref<ControllersTestNpc> npc =
			model::gameobjects::VisibleObject::create<ControllersTestNpc>(std::make_unique<RecordingNpcController>(), *spawnTemplate, objectTemplate);
		npc->setEffectController(std::make_unique<effect::EffectController>(*npc));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		return npc;
	}

	/** Replaces the npc's AI with a real NpcAI leaf (this executable registers no AI handler, so newAI cannot build one) */
	ProbeNpcAI& installProbe(model::gameobjects::Npc& npc) {
		auto probe = std::make_unique<ProbeNpcAI>(npc);
		ProbeNpcAI& result = *probe;
		npc.replaceAi(std::move(probe));
		return result;
	}

	static inline const model::templates::npc::NpcTemplate* walkerNpcTemplate = npcTemplateFrom(
		R"(<npc_template npc_id="210010" name_id="1" level="12" name="walker" rating="NORMAL" rank="VETERAN" tribe="GUARD")"
		R"(><stats maxHp="199" maxMp="0" attack="10"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");

	/** No AI handler is registered under this name in any executable, so AIEngine::newAI takes its missing-handler arm */
	static inline const model::templates::npc::NpcTemplate* unregisteredAiTemplate = npcTemplateFrom(
		R"(<npc_template npc_id="210011" name_id="1" level="12" name="ghost" rating="NORMAL" rank="VETERAN" tribe="GUARD")"
		R"( ai="m5b_e01a_unregistered"><stats maxHp="199" maxMp="0" attack="10"/></npc_template>)");
};

TEST_F(AiSeamTest, TheLastRouteStepStopsWalkingThroughWalkManager) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createSpawnableNpc(walkerNpcTemplate);
	ProbeNpcAI& ai = installProbe(*npc);
	Ptr<NpcMoveController> move = npc->getMoveController();

	// what WalkManager.startRouteWalking leaves behind (WalkManager.java:60-76): the template, the first step, WALKING/WALK_PATH, WALK_MODE
	move->setWalkerTemplate(routeOfThreeSteps(), 0);
	ASSERT_TRUE(ai.setStateIfNot(AIState::WALKING));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::WALK_PATH));
	npc->setState(CreatureState::WALK_MODE); // EmoteManager.emoteStartWalking
	move->moveToNextPoint();
	ASSERT_EQ(npc->recordingController().startMoves, 1);

	// two route steps: isStop stays false, so isNextRouteStepChosen only advances the step
	EXPECT_TRUE(move->isNextRouteStepChosen());
	EXPECT_FALSE(move->isStop());
	EXPECT_EQ(move->getCurrentStep()->getStepIndex(), 1);
	EXPECT_TRUE(move->isNextRouteStepChosen());
	EXPECT_EQ(move->getCurrentStep()->getStepIndex(), 2);
	EXPECT_TRUE(move->isStop()) << "the last step of a loop_type=NONE route (NpcMoveController.setRouteStep)";
	EXPECT_EQ(ai.getState(), AIState::WALKING) << "nothing has called into the AI yet";

	// the seam: NpcMoveController.java:389 `WalkManager.stopWalking((NpcAI) owner.getAi())`
	EXPECT_FALSE(move->isNextRouteStepChosen());
	EXPECT_EQ(ai.getState(), AIState::IDLE) << "WalkManager.stopWalking (WalkManager.java:213-219)";
	EXPECT_EQ(ai.getSubState(), AISubState::NONE);
	EXPECT_FALSE(npc->isInState(CreatureState::WALK_MODE)) << "EmoteManager.emoteStopWalking unsets WALK_MODE";
	EXPECT_EQ(npc->recordingController().stopMoves, 1) << "stopWalking aborts the started move";
	EXPECT_TRUE(ai.asked.empty()) << "stopWalking asks the AI nothing";
}

TEST_F(AiSeamTest, AReturningNpcAtItsDestinationReachesTargetEventHandler) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createSpawnableNpc(walkerNpcTemplate);
	ProbeNpcAI& ai = installProbe(*npc);
	npc->getPosition()->setXYZH(100.0f, 200.0f, 300.0f, int8_t{0});
	Ptr<NpcMoveController> move = npc->getMoveController();
	ASSERT_FALSE(npc->isAtSpawnLocation()) << "the spawn template is at (10, 20, 30)";

	// a return that is already at its point: moveToLocation computes dist == 0 while the AI is RETURNING
	ASSERT_TRUE(move->moveToPoint(100.0f, 200.0f, 300.0f));
	ASSERT_TRUE(ai.setStateIfNot(AIState::RETURNING));
	ASSERT_EQ(npc->recordingController().startMoves, 1);

	// the seam: NpcMoveController.java:259 `TargetEventHandler.onTargetReached((NpcAI) owner.getAi())`
	move->moveToDestination();

	EXPECT_EQ(ai.getState(), AIState::IDLE) << "TargetEventHandler.onTargetReached, RETURNING arm: abortMove, then, because the npc is not at "
											   "its spawn, IDLE and the NOT_AT_HOME event (TargetEventHandler.java:40-48)";
	EXPECT_EQ(npc->recordingController().stopMoves, 1) << "the abortMove of the RETURNING arm";
	EXPECT_FALSE(move->isInMove());
}

TEST_F(AiSeamTest, AnAttackedNpcReachesShoutEventHandlerOnEnemyAttack) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createSpawnableNpc(walkerNpcTemplate);
	Ref<ControllersTestNpc> attacker = createSpawnableNpc(walkerNpcTemplate);
	ProbeNpcAI& ai = installProbe(*npc);

	// NpcController.onAttack (NpcController.java:290-305). The npc is not spawned, so CreatureController.onAttack returns at its first
	// statement and the shout call is all that is left; the attacker is an Npc, so the QuestEngine branch is not taken either.
	EXPECT_NO_THROW(npc->recordingController().onAttack(*attacker, Ptr<skillengine::model::Effect>(), SM_ATTACK_STATUS_TYPE::REGULAR, 10, true,
		SM_ATTACK_STATUS_LOG::REGULAR, std::nullopt, std::nullopt));

	// the seam: `ShoutEventHandler.onEnemyAttack((NpcAI) npc.getAi(), attacker)`, whose first statement is the CAN_SHOUT poll
	ASSERT_EQ(ai.asked.size(), 1u) << "ShoutEventHandler.onEnemyAttack asks CAN_SHOUT first (ShoutEventHandler.java:100)";
	EXPECT_EQ(ai.asked.front(), AIQuestion::CAN_SHOUT);
	EXPECT_EQ(npc->getAttackedCount(), 0) << "CreatureController.onAttack returned before incrementAttackedCount (the npc is not spawned)";
}

TEST_F(AiSeamTest, AnNpcWithNoAiHandlerStillTakesAHit) {
	// D15 / A-00: with gameserver.dev.missing_ai_handlers=warn, AIEngine::newAI substitutes a DummyNpcAI (an NpcAI) for an npc whose AI name
	// has no handler, which is what keeps the Java-faithful `(NpcAI) npc.getAi()` of NpcController.java:303 working. Before A-00 the
	// substitute was a DummyAI<Creature> and this call threw ClassCastException - reachable from the moment this item's direct call exists.
	MissingAiHandlersGuard warn("warn");
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createSpawnableNpc(unregisteredAiTemplate);
	Ref<ControllersTestNpc> attacker = createSpawnableNpc(walkerNpcTemplate);

	EXPECT_NO_THROW(runtime::cast<ai::NpcAI>(npc->getAi())) << "AIEngine::newAI gave the npc a DummyNpcAI";
	EXPECT_NO_THROW(npc->recordingController().onAttack(*attacker, Ptr<skillengine::model::Effect>(), SM_ATTACK_STATUS_TYPE::REGULAR, 10, true,
		SM_ATTACK_STATUS_LOG::REGULAR, std::nullopt, std::nullopt));

	// The negative control, and the reason the cast stays unguarded: an npc whose template names no AI at all keeps Java's own DummyAI (an
	// AITemplate<Creature>, not an NpcAI, AIEngine.cpp:155-156), and there NpcController.onAttack throws exactly as Java's cast would. An
	// `if (auto npcAi = runtime::as<NpcAI>(...))` at the call site - the C++-only branch D15 rejects - would swallow this.
	Ref<ControllersTestNpc> aiLess = createSpawnableNpc(walkerNpcTemplate);
	ASSERT_FALSE(walkerNpcTemplate->getAiName()) << "the template names no AI";
	EXPECT_THROW(aiLess->recordingController().onAttack(*attacker, Ptr<skillengine::model::Effect>(), SM_ATTACK_STATUS_TYPE::REGULAR, 10, true,
					 SM_ATTACK_STATUS_LOG::REGULAR, std::nullopt, std::nullopt),
		runtime::ClassCastException);
}

TEST_F(AiSeamTest, MoveInfoIsLoggedByTheRealAILogger) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createSpawnableNpc(walkerNpcTemplate);
	ProbeNpcAI& ai = installProbe(*npc);
	ai.setLogging(true);
	const bool previousMoveDebug = configs::main::AIConfig::MOVE_DEBUG.exchange(true);
	std::string logged;
	{
		LogCapture capture("com.aionemu.gameserver.ai.AILogger");
		// the seam: `AILogger.moveinfo(owner, "MC: moveToPoint (startedMoving=" + startedMoving + ")")` (NpcMoveController.java:88)
		EXPECT_TRUE(npc->getMoveController()->moveToPoint(1.0f, 2.0f, 3.0f));
		logged = capture.text();
	}
	configs::main::AIConfig::MOVE_DEBUG.store(previousMoveDebug);

	EXPECT_NE(logged.find("MC: moveToPoint (startedMoving=true)"), std::string::npos) << logged;
	EXPECT_NE(logged.find("[AI] " + std::to_string(npc->getObjectId()) + " - "), std::string::npos)
		<< "AILogger.moveinfo writes \"[AI] <objectId> - <message>\": " << logged;
}

} // namespace
} // namespace aion::gameserver::controllers::testing
