// AI framework (P5-05, m5a-plan.md W-01): the AIState companion against Java's EnumSets, AIEngine.newAI and validateScripts with the C++-only
// setting gameserver.dev.missing_ai_handlers (fail by default, warn: DummyAI), and AbstractAI's event dispatch (state filter, canHandleEvent,
// handleGeneralEvent/handleCreatureEvent, the recursion guard, FREEZE/UNFREEZE, the event log). Expectations are hand-derived from AIState.java,
// AIEngine.java, AbstractAI.java, AITemplate.java, FreezeEventHandler.java and AIEventLog.java.

#include <gtest/gtest.h>

#include <algorithm>
#include <any>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AIStateInfo.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventLog.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "AiTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

using event::AIEventType;
using model::gameobjects::Creature;

// ---- AIState companion ------------------------------------------------------------------------------------------------------------------------

/** Java AIState constructors: the four states with an explicit EnumSet.of(first, rest), every other state EnumSet.allOf(AIEventType.class) */
bool javaHandledEvents(AIState state, AIEventType event) {
	switch (state) {
		case AIState::CREATED:
		case AIState::DESPAWNED:
			return event == AIEventType::BEFORE_SPAWNED || event == AIEventType::SPAWNED;
		case AIState::DIED:
			return event == AIEventType::DESPAWNED || event == AIEventType::DROP_REGISTERED;
		case AIState::FORCED_WALKING:
			return event == AIEventType::MOVE_ARRIVED || event == AIEventType::MOVE_VALIDATE || event == AIEventType::DESPAWNED ||
				event == AIEventType::DIED;
		default:
			return true;
	}
}

TEST(AIStateInfoTest, CanHandleEqualsJavasEnumSetsForEveryStateAndEvent) {
	const auto& states = xml::EnumTraits<AIState>::names;
	const auto& events = xml::EnumTraits<AIEventType>::names;
	ASSERT_EQ(states.size(), 11u);
	ASSERT_EQ(events.size(), 30u);
	int32_t handled = 0;
	for (size_t s = 0; s < states.size(); ++s) {
		for (size_t e = 0; e < events.size(); ++e) {
			auto state = static_cast<AIState>(s);
			auto event = static_cast<AIEventType>(e);
			EXPECT_EQ(canHandle(state, event), javaHandledEvents(state, event)) << states[s] << " " << events[e];
			handled += canHandle(state, event) ? 1 : 0;
		}
	}
	EXPECT_EQ(handled, 7 * 30 + 2 + 2 + 2 + 4) << "7 states handle all 30 events (NONE included)";
	EXPECT_FALSE(canHandle(AIState::CREATED, AIEventType::ACTIVATE));
	EXPECT_TRUE(canHandle(AIState::IDLE, AIEventType::NONE));
	EXPECT_FALSE(canHandle(AIState::FORCED_WALKING, AIEventType::CREATURE_SEE));
}

// ---- AIEventLog ---------------------------------------------------------------------------------------------------------------------------------

TEST(AIEventLogTest, OfferFirstDropsTheOldestEventWhenFull) {
	AI_TEST_SCOPE;
	runtime::Ref<event::AIEventLog> log = event::AIEventLog::create(3);
	EXPECT_TRUE(log->isEmpty());
	EXPECT_EQ(log->remainingCapacity(), 3);
	log->addFirst(AIEventType::SPAWNED);
	log->addFirst(AIEventType::ACTIVATE);
	log->addFirst(AIEventType::ATTACK);
	EXPECT_EQ(log->remainingCapacity(), 0);
	EXPECT_TRUE(log->offerFirst(AIEventType::DIED)) << "the override always accepts";
	std::vector<AIEventType> events;
	for (AIEventType e : *log)
		events.push_back(e);
	EXPECT_EQ(events, (std::vector<AIEventType>{AIEventType::DIED, AIEventType::ATTACK, AIEventType::ACTIVATE})) << "newest first, SPAWNED dropped";
	EXPECT_EQ(log->removeLast(), AIEventType::ACTIVATE);
	EXPECT_EQ(log->size(), 2);
	runtime::Ref<event::AIEventLog> unbounded = event::AIEventLog::create();
	EXPECT_EQ(unbounded->remainingCapacity(), 2147483647);
	EXPECT_THROW(static_cast<void>(unbounded->removeLast()), runtime::NoSuchElementException);
}

// ---- AIEngine ------------------------------------------------------------------------------------------------------------------------------------

TEST_F(AiTest, NewAiWithoutNameCreatesTheDummyAi) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	AbstractAI& createdAi = npc->getAi();
	EXPECT_EQ(createdAi.getName(), "noname") << "Java: no @AIName annotation";
	EXPECT_EQ(createdAi.getRegistryEntry(), nullptr);
	EXPECT_EQ(createdAi.getState(), AIState::CREATED);
	EXPECT_EQ(createdAi.getSubState(), AISubState::NONE);
	EXPECT_FALSE(createdAi.isLogging());
	EXPECT_TRUE(createdAi.canThink()) << "AITemplate";
}

TEST_F(AiTest, MissingHandlerFailsByDefaultLikeJava) {
	EXPECT_FALSE(AIEngine::isMissingAiHandlersWarn());
	AI_TEST_SCOPE;
	try {
		static_cast<void>(createNpc(generalTemplate));
		ADD_FAILURE() << "the empty AI registry has no AI named general";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "No AI found for name general");
	}
}

TEST_F(AiTest, WarnModeCreatesADummyAiAndWarnsOncePerName) {
	configs::main::AIConfig::MISSING_AI_HANDLERS.set("WARN");
	EXPECT_TRUE(AIEngine::isMissingAiHandlersWarn()) << "the value is compared ignoring case";
	LogCapture capture("com.aionemu.gameserver.ai.AIEngine");
	AI_TEST_SCOPE;
	const model::templates::npc::NpcTemplate* warnOnceTemplate =
		npcTemplate(R"(npc_id="210003" level="12" name="warn once" rating="NORMAL" rank="VETERAN" tribe="GUARD" ai="warn_once_test_ai")");
	runtime::Ref<AiTestNpc> first = createNpc(warnOnceTemplate);
	runtime::Ref<AiTestNpc> second = createNpc(warnOnceTemplate);
	EXPECT_EQ(first->getAi().getName(), "noname");
	EXPECT_EQ(first->getAi().getRegistryEntry(), nullptr);
	EXPECT_NE(&first->getAi(), &second->getAi());
	const std::string text = capture.text();
	const std::string warning =
		"warning|No AI found for name warn_once_test_ai, creating a DummyAI instead (gameserver.dev.missing_ai_handlers=warn)";
	size_t at = text.find(warning);
	ASSERT_NE(at, std::string::npos) << text;
	EXPECT_EQ(text.find(warning, at + 1), std::string::npos) << "one warning per AI name";
	std::vector<std::string> seen = AIEngine::missingAiNamesSeen();
	EXPECT_NE(std::find(seen.begin(), seen.end(), "warn_once_test_ai"), seen.end());
	EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));

	configs::main::AIConfig::MISSING_AI_HANDLERS.set("fail");
	EXPECT_THROW(static_cast<void>(createNpc(warnOnceTemplate)), runtime::IllegalArgumentException) << "the setting is read on every newAI";
}

TEST_F(AiTest, OnCreateDebugEnablesLoggingOfNewAis) {
	configs::main::AIConfig::ONCREATE_DEBUG.store(true);
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	EXPECT_TRUE(npc->getAi().isLogging());
}

class NpcDataHolder {
public:
	explicit NpcDataHolder(const char* xmlText) {
		xml::LoadContext context;
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, xmlText));
	}
	~NpcDataHolder() { dataholders::DataManager::NPC_DATA.resetForTests(); }
	NpcDataHolder(const NpcDataHolder&) = delete;
	NpcDataHolder& operator=(const NpcDataHolder&) = delete;
};

const char* const NPCS_WITH_AI_NAMES = R"(<npc_templates>)"
									   R"(<npc_template npc_id="1" level="10" name_id="1" name="a" rank="NOVICE" rating="NORMAL" tribe="GENERAL" ai="general"><stats maxHp="10"/></npc_template>)"
									   R"(<npc_template npc_id="2" level="10" name_id="1" name="b" rank="NOVICE" rating="NORMAL" tribe="GENERAL" ai="dummy"><stats maxHp="10"/></npc_template>)"
									   R"(<npc_template npc_id="3" level="10" name_id="1" name="c" rank="NOVICE" rating="NORMAL" tribe="GENERAL" ai="general"><stats maxHp="10"/></npc_template>)"
									   R"(<npc_template npc_id="4" level="10" name_id="1" name="d" rank="NOVICE" rating="NORMAL" tribe="GENERAL"><stats maxHp="10"/></npc_template>)"
									   R"(</npc_templates>)";

TEST_F(AiTest, InitValidatesTheNpcTemplateAiNames) {
	NpcDataHolder npcData(NPCS_WITH_AI_NAMES);
	ASSERT_TRUE(handlers::aiHandlerEntries().empty()) << "this test executable links the empty registry";
	try {
		AIEngine::getInstance().init();
		ADD_FAILURE() << "Java: GameServerError for template AI names without a handler";
	} catch (const commons::utils::Exception& e) {
		// Java joins a HashSet (unspecified order); C++ sorts the distinct names
		EXPECT_STREQ(e.what(), "No AIs could be found for the following npc_template AI names: dummy, general");
	}

	configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	LogCapture capture("com.aionemu.gameserver.ai.AIEngine");
	EXPECT_NO_THROW(AIEngine::getInstance().init());
	const std::string text = capture.text();
	EXPECT_NE(text.find("warning|No AIs could be found for the following npc_template AI names: dummy, general (gameserver.dev.missing_ai_handlers=warn: "
						"these NPCs get a DummyAI)"),
		std::string::npos)
		<< text;
	EXPECT_NE(text.find("info|Loaded 0 AI handlers."), std::string::npos) << text;
}

TEST_F(AiTest, RegisterAiAcceptsTheEntryTheRegistryHoldsUnderItsName) {
	handlers::AIHandlerEntry entry{"not_registered", "ai.NotRegisteredAI", nullptr, "ai/NotRegisteredAI.cpp:1"};
	EXPECT_NO_THROW(AIEngine::getInstance().registerAI(entry)) << "no other entry is registered under the name";
}

// ---- AbstractAI dispatch ----------------------------------------------------------------------------------------------------------------------

/** An AITemplate over Creature that records the hooks the dispatch reaches */
class RecordingAI final : public AITemplate<Creature> {
public:
	explicit RecordingAI(Creature& owner) : AITemplate<Creature>(owner) {}

	std::vector<std::string> calls;
	int32_t thinks = 0;
	int32_t recursionLimit = 0;
	int32_t recursions = 0;
	bool needsSupportResult = false;

	void think() override { thinks++; }

protected:
	void handleActivate() override { calls.push_back("activate"); }
	void handleDeactivate() override { calls.push_back("deactivate"); }
	void handleSpawned() override { calls.push_back("spawned"); }
	void handleBeforeSpawned() override { calls.push_back("beforeSpawned"); }
	void handleDespawned() override { calls.push_back("despawned"); }
	void handleDied() override { calls.push_back("died"); }
	void handleMoveArrived() override { calls.push_back("moveArrived"); }
	void handleDropRegistered() override { calls.push_back("dropRegistered"); }
	void handleAttack(runtime::Ptr<Creature> creature) override { calls.push_back("attack " + std::to_string(creature->getObjectId())); }
	bool handleCreatureNeedsSupport(Creature& creature) override {
		calls.push_back("needsSupport");
		return needsSupportResult;
	}
	bool handleCreatureNeedsSupportByGuard(Creature& creature) override {
		calls.push_back("needsSupportByGuard");
		return false;
	}
	void handleCreatureSee(Creature& creature) override {
		calls.push_back("see");
		if (recursions < recursionLimit) {
			recursions++;
			onCreatureEvent(AIEventType::CREATURE_SEE, creature);
		}
	}
	void handleCreatureMoved(Creature& creature) override { calls.push_back("moved"); }
	void handleDialogStart(model::gameobjects::player::Player& player) override { calls.push_back("dialogStart"); }
	void handleCustomEvent(int32_t eventId, std::span<const std::any> args) override {
		calls.push_back("custom " + std::to_string(eventId) + " " + std::to_string(args.size()) + " " + std::to_string(std::any_cast<int32_t>(args[0])));
	}
};

class AbstractAITest : public AiTest {
protected:
	RecordingAI& installRecordingAi(AiTestNpc& npc) {
		auto recordingAi = std::make_unique<RecordingAI>(npc);
		RecordingAI& result = *recordingAi;
		npc.replaceAi(std::move(recordingAi));
		return result;
	}
};

TEST_F(AbstractAITest, GeneralEventsPassTheStateFilterAndReachTheirHandler) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	RecordingAI& ai = installRecordingAi(*npc);

	ai.onGeneralEvent(AIEventType::ACTIVATE); // CREATED handles only BEFORE_SPAWNED and SPAWNED
	ai.onGeneralEvent(AIEventType::BEFORE_SPAWNED);
	ai.onGeneralEvent(AIEventType::SPAWNED);
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"beforeSpawned", "spawned"}));

	EXPECT_TRUE(ai.setStateIfNot(AIState::IDLE));
	EXPECT_FALSE(ai.setStateIfNot(AIState::IDLE));
	EXPECT_TRUE(ai.isInState(AIState::IDLE));
	ai.calls.clear();
	ai.onGeneralEvent(AIEventType::ACTIVATE);
	ai.onGeneralEvent(AIEventType::DEACTIVATE);
	ai.onGeneralEvent(AIEventType::NONE); // handled by the state, but no case in handleGeneralEvent
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"activate", "deactivate"}));

	ai.setStateIfNot(AIState::DIED);
	ai.calls.clear();
	ai.onGeneralEvent(AIEventType::SPAWNED);
	ai.onGeneralEvent(AIEventType::DESPAWNED);
	ai.onGeneralEvent(AIEventType::DROP_REGISTERED);
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"despawned", "dropRegistered"}));

	ai.setStateIfNot(AIState::FORCED_WALKING);
	ai.calls.clear();
	ai.onGeneralEvent(AIEventType::MOVE_ARRIVED);
	ai.onGeneralEvent(AIEventType::ACTIVATE);
	ai.onGeneralEvent(AIEventType::DIED);
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"moveArrived", "died"}));
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *npc);
	EXPECT_EQ(ai.calls.size(), 2u) << "FORCED_WALKING drops creature events";
}

TEST_F(AbstractAITest, FreezeSetsTheSubStateAndUnfreezeClearsIt) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	RecordingAI& ai = installRecordingAi(*npc);
	ai.setStateIfNot(AIState::IDLE);

	ai.onGeneralEvent(AIEventType::UNFREEZE);
	EXPECT_EQ(ai.thinks, 0) << "not frozen: onUnfreeze does nothing";
	ai.onGeneralEvent(AIEventType::FREEZE);
	EXPECT_TRUE(ai.isInSubState(AISubState::FREEZE));
	EXPECT_EQ(ai.thinks, 1);
	EXPECT_FALSE(ai.setSubStateIfNot(AISubState::FREEZE));
	ai.onGeneralEvent(AIEventType::UNFREEZE);
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE));
	EXPECT_EQ(ai.thinks, 2);
}

TEST_F(AbstractAITest, CreatureEventsFollowCanHandleEventAndTheHandlerSwitch) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	runtime::Ref<AiTestNpc> other = createNpc(plainTemplate);
	RecordingAI& ai = installRecordingAi(*npc);
	ai.setStateIfNot(AIState::FIGHT);

	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *other); // canHandleEvent: only IDLE and WALKING
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *other);
	ai.onCreatureEvent(AIEventType::ATTACK, *other);
	ai.onCreatureEvent(AIEventType::CREATURE_NEEDS_SUPPORT, *other);
	ai.needsSupportResult = true;
	ai.onCreatureEvent(AIEventType::CREATURE_NEEDS_SUPPORT, *other);
	ai.onCreatureEvent(AIEventType::SPAWNED, *other); // a general event: no case in handleCreatureEvent
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"see", "attack " + std::to_string(other->getObjectId()), "needsSupport", "needsSupportByGuard",
							"needsSupport"}));

	ai.setStateIfNot(AIState::WALKING);
	ai.calls.clear();
	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *other);
	EXPECT_EQ(ai.calls, (std::vector<std::string>{"moved"}));
	EXPECT_THROW(ai.onCreatureEvent(AIEventType::DIALOG_START, *other), runtime::ClassCastException) << "Java: (Player) creature";

	ai.onCustomEvent(7, {std::any(int32_t{42}), std::any(std::string("x"))});
	EXPECT_EQ(ai.calls.back(), "custom 7 2 42");
}

TEST_F(AbstractAITest, NestedCreatureEventsAbortAfterDepthTwentyAndTheDepthResets) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	RecordingAI& ai = installRecordingAi(*npc);
	ai.setStateIfNot(AIState::IDLE);

	ai.recursionLimit = 20; // depth 0..20 are allowed: 21 nested handler calls
	EXPECT_NO_THROW(ai.onCreatureEvent(AIEventType::CREATURE_SEE, *npc));
	EXPECT_EQ(ai.calls.size(), 21u);

	ai.calls.clear();
	ai.recursions = 0;
	ai.recursionLimit = 100;
	// the 22nd nested call sees depth 21 > 20; Java's message names the most hated target of the aggro list, whose getTarget is P5-01's
	EXPECT_ANY_THROW(ai.onCreatureEvent(AIEventType::CREATURE_SEE, *npc));
	EXPECT_EQ(ai.calls.size(), 21u);

	ai.calls.clear();
	ai.recursions = 0;
	ai.recursionLimit = 0;
	EXPECT_NO_THROW(ai.onCreatureEvent(AIEventType::CREATURE_SEE, *npc)) << "the finally block reset the depth";
	EXPECT_EQ(ai.calls.size(), 1u);
}

TEST_F(AbstractAITest, EventLogKeepsTheLastTenLoggedEventsWhenEventDebugIsOn) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	RecordingAI& ai = installRecordingAi(*npc);
	ai.setStateIfNot(AIState::IDLE);
	ai.onGeneralEvent(AIEventType::ACTIVATE);
	EXPECT_FALSE(ai.getEventLog()) << "EVENT_DEBUG is off";

	configs::main::AIConfig::EVENT_DEBUG.store(true);
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *npc); // not logged by handleCreatureEvent
	EXPECT_FALSE(ai.getEventLog());
	ai.onCreatureEvent(AIEventType::ATTACK, *npc);
	ASSERT_TRUE(ai.getEventLog());
	for (int32_t i = 0; i < 10; i++)
		ai.onGeneralEvent(i % 2 == 0 ? AIEventType::ACTIVATE : AIEventType::DEACTIVATE);
	std::vector<AIEventType> events;
	for (AIEventType e : *ai.getEventLog())
		events.push_back(e);
	ASSERT_EQ(events.size(), 10u) << "capacity 10: ATTACK was dropped";
	EXPECT_EQ(events.front(), AIEventType::DEACTIVATE);
	EXPECT_EQ(events.back(), AIEventType::ACTIVATE);
}

TEST_F(AbstractAITest, NameThinkingAndOwnerAccessors) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	RecordingAI& ai = installRecordingAi(*npc);
	handlers::AIHandlerEntry entry{"recording", "ai.RecordingAI", nullptr, "ai/RecordingAI.cpp:1"};
	EXPECT_EQ(ai.getName(), "noname");
	ai.setRegistryEntry(&entry);
	EXPECT_EQ(ai.getName(), "recording");
	ai.setRegistryEntry(nullptr);

	EXPECT_TRUE(ai.setThinking());
	EXPECT_FALSE(ai.setThinking());
	ai.unsetThinking();
	EXPECT_TRUE(ai.setThinking());

	EXPECT_EQ(ai.getObjectId(), npc->getObjectId());
	EXPECT_EQ(ai.getPosition().get(), npc->getPosition().get());
	EXPECT_FALSE(ai.getTarget());
}

} // namespace
} // namespace aion::gameserver::ai::testing
