// M5d's quest-npc-ais lane (m5d-plan.md A-03 and A-04; chunk A1 under the lane's file lease on handlers/ai/quests/QuestItemNpcAI.*, tested
// here in P5-05's directory as AscensationNpcAiTest.cpp tests its A1 file): QuestItemNpcAI, the AI of the 610 npc templates with
// ai="quest_use_item" (QuestItemNpcAI.java:26-77) - the quest objects - on top of ActionItemNpcAI's use bar.
//
// The quest object is 700105, the kerub grain sack of Poeta (npc_templates.xml:440035-440039: talk distance 3, talk delay 3 s, no dialog), whose
// <quest_drop> makes it the action item of 1103 "Grain Thieves" (quest_data.xml:905-914: 3 Kerub Grain Sacks 182200201, dropped by 700105).
// 1103 is registered from its row of quest_script_data (poeta.xml:114, <item_collecting id="1103" start_npc_ids="203057"/>) through
// ItemCollectingData.register_, the way QuestEngine.init's loop registers the XML quests (QuestEngine.java:104-105), and the quest drops are
// indexed as QuestEngine.init's drop loop indexes them (QuestEngine.java:86-90). The player, the clock and the rows of the quest data, the
// items and the grain sack are the handler-base fixture's (tests/quest_handlers/QuestHandlerTestSupport.h, P5-06b): a quester online in a
// Poeta map instance, whose packets a real AionConnection records, on the fixture's DeterministicExecutor and ManualClock. A click is started
// the way CM_SHOW_DIALOG starts it: NpcController::onDialogRequest (talk range) -> DIALOG_START -> the AI.
//
// Expected packets are Java's bytes where the fields are the packet's own (SM_USE_OBJECT.java:25-30, SM_LOOT_STATUS.java:39-43,
// SM_LOOT_ITEMLIST.java:33-56, SM_DIALOG_WINDOW.java:29-40); SM_EMOTION, whose body the server builds from the player's state and speeds, is
// compared against the server's serialization of the SM_EMOTION Java constructs there.
//
// Not covered: the group and alliance arms of handleUseItemFinish (QuestItemNpcAI.java:55-64), which need a PlayerGroup / PlayerAlliance - M5g
// ports GeneralTeam's members and the team constructors (m5d-plan.md E-04, "the case waits for M5g").
//
// LINK WORKAROUND (reported as a manifest request, as for AscensationNpcAI): A1's handler library, aion_gs_handlers_ai_world, is not linked into
// this executable, and A1's own test executable would miss this directory's library, which defines QuestItemNpcAI's superclass
// ActionItemNpcAI. Until the manifest links the two, this file compiles the one leased source into this executable itself. It is the only
// translation unit that does, so the factory and the class are defined once here; the server links the A1 library through the registry.
#include "aion/gameserver/handlers/ai/quests/QuestItemNpcAI.cpp"

#include "../quest_handlers/QuestHandlerTestSupport.h"
#include "../dao/DaoTestDatabase.h"
#include "QuestNpcAiTestSupport.h"

#include <gtest/gtest.h>

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
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/CustomDrop.bind.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/XMLQuests.bind.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/handlers/ai/quests/QuestItemNpcAI.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/skillengine/effect/SummonOwner.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

namespace quests = ::aion::gameserver::handlers::ai::quests;

using ::aion::gameserver::ai::event::AIEventType;
using gameserver::model::EmotionType;
using gameserver::model::TaskId;
using gameserver::model::gameobjects::Npc;
using network::aion::serverpackets::SM_EMOTION;

// ServerPacketsOpcodes.java:68, 215, 223-224
constexpr int32_t SM_ABNORMAL_EFFECT_OPCODE = 50;
constexpr int32_t SM_USE_OBJECT_OPCODE = 197;
constexpr int32_t SM_LOOT_STATUS_OPCODE = 205;
constexpr int32_t SM_LOOT_ITEMLIST_OPCODE = 206;

/** quest_script_data/poeta.xml:114 */
constexpr const char* GRAIN_THIEVES_SCRIPT_XML = R"(<quest_scripts><item_collecting id="1103" start_npc_ids="203057"/></quest_scripts>)";

/** SM_USE_OBJECT.writeImpl (SM_USE_OBJECT.java:25-30): D(player), D(target), D(time), C(actionType) */
std::vector<uint8_t> useObject(int32_t playerObjId, int32_t targetObjId, int32_t time, int32_t actionType) {
	return items::javaPacket(SM_USE_OBJECT_OPCODE, PacketWriter().D(playerObjId).D(targetObjId).D(time).C(actionType));
}

/** SM_LOOT_STATUS.writeImpl (SM_LOOT_STATUS.java:39-43): D(target), C(status: LOOT_ENABLE 0, OPEN_DROP_LIST 2), D(lootEffectId: 0 for a quest item) */
std::vector<uint8_t> lootStatus(int32_t targetObjId, int32_t status) {
	return items::javaPacket(SM_LOOT_STATUS_OPCODE, PacketWriter().D(targetObjId).C(status).D(0));
}

/**
 * SM_ABNORMAL_EFFECT.writeImpl (SM_ABNORMAL_EFFECT.java:39-46) of an npc without effects, as EffectController.broadCastEffects(null) sends it
 * (EffectController.java:304-308): D(effected), C(effectType 1: not a player), D(0), D(abnormals 0), D(0), C(slots FULLSLOTS 127), H(0 effects)
 */
std::vector<uint8_t> noAbnormalEffects(int32_t effectedObjId) {
	return items::javaPacket(SM_ABNORMAL_EFFECT_OPCODE, PacketWriter().D(effectedObjId).C(1).D(0).D(0).D(0).C(127).H(0));
}

/**
 * SM_LOOT_ITEMLIST.writeImpl (SM_LOOT_ITEMLIST.java:33-56) with one entry the player may see: D(target), C(1), then C(index), D(item), D(count),
 * C(optional socket 0), C(0), C(0), C(loot confirmation: 0 without team members nearby)
 */
std::vector<uint8_t> lootItemList(int32_t targetObjId, int32_t index, int32_t itemId, int32_t count) {
	return items::javaPacket(SM_LOOT_ITEMLIST_OPCODE, PacketWriter().D(targetObjId).C(1).C(index).D(itemId).D(count).C(0).C(0).C(0).C(0));
}

/** The rows of QuestNpcAiTestSupport.h, bound through NpcData as the server loads npc_templates.xml; kept for the process like DataManager's */
const gameserver::model::templates::npc::NpcTemplate* supportTemplateOf(int32_t npcId) {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(
			context, "<npc_templates>" + std::string(gameserver::ai::testing::QUEST_NPC_AI_TEMPLATES_XML) + "</npc_templates>")
			.release();
	}();
	return holder->getNpcTemplate(npcId);
}

/**
 * The registry entry AIEngine.newAI would have created the AI from (HandlerRegistry.h): AbstractAI.getName answers its name, and
 * DropRegistrationService.registerDrop asks it - a "quest_use_item" npc gets no global drops (DropRegistrationService.java:84-86)
 */
const ::aion::gameserver::handlers::AIHandlerEntry QUEST_USE_ITEM_ENTRY{
	"quest_use_item", "ai.quests.QuestItemNpcAI", &quests::QuestItemNpcAI_aiFactory, "ai/quests/QuestItemNpcAI.cpp:21"};

/** QuestItemNpcAI with its protected finish callable, the call the bar's task makes at its end (ActionItemNpcAI.java:72) */
class QuestItemNpcAiProbe final : public quests::QuestItemNpcAI {
public:
	using QuestItemNpcAI::QuestItemNpcAI;

	void finishUse(gameserver::model::gameobjects::player::Player& player) { handleUseItemFinish(player); }
};

/** A fabricated handler for a real quest that records the players QuestEngine.onAtDistance hands it for one npc */
class AtDistanceProbe final : public AbstractQuestHandler {
public:
	AtDistanceProbe(int32_t questId, int32_t npcId, std::vector<int32_t>& seen) : AbstractQuestHandler(questId), npcId(npcId), seen(seen) {}

	void register_() override { qe.registerQuestNpc(npcId)->addOnAtDistanceEvent(questId); }

	bool onAtDistanceEvent(QuestEnv& env) override {
		seen.push_back(env.getPlayer()->getObjectId());
		return true;
	}

private:
	const int32_t npcId;
	std::vector<int32_t>& seen;
};

class QuestItemNpcAiTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		// the world holders are published once per process: ItemPacketTest publishes its Poeta row only where none is published, while this
		// executable's AI fixtures publish theirs without asking (tests/ai/AiWorldTestSupport.h); publishing that set first keeps the order of the
		// suites irrelevant when one process runs them all (as ActionItemNpcAiTest.cpp does)
		::aion::gameserver::ai::testing::publishAiMapStaticDataOnce();
		QuestHandlerTest::SetUp();
		dataholders::DataManager::XML_QUESTS.resetForTests();
		dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(contexts.emplace_back(), GRAIN_THIEVES_SCRIPT_XML));
		// DropRegistrationService.registerDrop asks the custom npc drops first (none for the grain sack), and its drop modifiers ask the looter's
		// house (none: no house land either)
		dataholders::DataManager::CUSTOM_NPC_DROP.resetForTests();
		dataholders::DataManager::CUSTOM_NPC_DROP.publish(xml::bindString<dataholders::CustomDrop>(contexts.emplace_back(), "<custom_drop/>"));
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(contexts.emplace_back(), "<house_lands/>"));
		// and the looter's drop rate: Java's default of gameserver.rates.drop (RatesConfig.java:81-82), membership 0 (the unit tests load no
		// configuration, and an empty list logs "Missing rates")
		savedDropRates = configs::main::RatesConfig::DROP_RATES.get();
		configs::main::RatesConfig::DROP_RATES.set({1.0f, 2.0f});
		// QuestEngine.init's drop loop (QuestEngine.java:86-90), then its XML loop for 1103 (:104-105)
		for (const gameserver::model::templates::QuestTemplate* quest : dataholders::DataManager::QUEST_DATA->getQuestTemplates()) {
			for (const gameserver::model::templates::quest::QuestDrop& drop : quest->getQuestDrop())
				services::QuestService::addQuestDrop(*drop.getNpcId(), &drop);
		}
		dataholders::DataManager::XML_QUESTS->getQuest(1103)->register_(QuestEngine::getInstance());
	}

	void TearDown() override {
		for (const Ref<Npc>& object : objects)
			services::drop::DropService::getInstance().unregisterDrop(*object);
		objects.clear();
		objectGroups.clear();
		QuestHandlerTest::TearDown(); // QuestEngine::clear also clears the quest drops
		if (savedDropRates)
			configs::main::RatesConfig::DROP_RATES.set(*savedDropRates);
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		dataholders::DataManager::CUSTOM_NPC_DROP.resetForTests();
		dataholders::DataManager::XML_QUESTS.resetForTests();
		seen.clear();
	}

	/** A quest object of the template, spawned `dx` on the x axis from the quester, level with him, in the fixture's map instance */
	Npc& objectAt(const gameserver::model::templates::npc::NpcTemplate* template_, float dx) {
		const float x = 100.0f + dx;
		Ref<gameserver::model::templates::spawns::SpawnGroup> group =
			gameserver::model::templates::spawns::SpawnGroup::create(210010000, template_->getTemplateId(), 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawn =
			group->addSpawnTemplate(std::make_unique<QuestHandlerSpawnTemplate>(*group, x, 100.0f, 50.0f));
		Ref<Npc> npc = gameserver::model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, template_);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		objectGroups.push_back(group);
		objects.push_back(npc);
		return *npc;
	}

	/** The grain sack 1 m from the quester (talk distance 3 + 1) */
	Npc& grainSack() { return objectAt(dataholders::DataManager::NPC_DATA->getNpcTemplate(GRAIN_SACK), 1.0f); }

	/**
	 * The handler through the factory its marker defines, with the registry entry AIEngine.newAI stores, idle as a spawned npc's AI is (the
	 * spawn's SPAWNED event moves it to IDLE)
	 */
	quests::QuestItemNpcAI& installAi(Npc& npc) {
		std::unique_ptr<::aion::gameserver::ai::AbstractAI> ai = quests::QuestItemNpcAI_aiFactory(npc);
		auto* typed = dynamic_cast<quests::QuestItemNpcAI*>(ai.get());
		EXPECT_NE(typed, nullptr);
		ai->setRegistryEntry(&QUEST_USE_ITEM_ENTRY);
		npc.replaceAi(std::move(ai));
		typed->setStateIfNot(::aion::gameserver::ai::AIState::IDLE);
		return *typed;
	}

	/** The probing leaf, likewise */
	QuestItemNpcAiProbe& installProbe(Npc& npc) {
		auto ai = std::make_unique<QuestItemNpcAiProbe>(npc);
		QuestItemNpcAiProbe& result = *ai;
		ai->setRegistryEntry(&QUEST_USE_ITEM_ENTRY);
		npc.replaceAi(std::move(ai));
		result.setStateIfNot(::aion::gameserver::ai::AIState::IDLE);
		return result;
	}

	/** 1103 held in START (var 0) with `sacks` Kerub Grain Sacks in the cube */
	void questerWithSacks(int32_t sacks) {
		hold(*me, 1103, QuestStatus::START);
		if (sacks > 0)
			holdItem(*me, 820201, KERUB_GRAIN_SACK, sacks);
	}

	/** SM_EMOTION(player, type, 0, target) as the server serializes it for the quester's connection, in the player's current state */
	std::vector<uint8_t> emotion(EmotionType type, int32_t target) { return me->serializedFor(SM_EMOTION(player(), type, 0, target)); }

	void move() { player().getObserveController()->notifyMoveObservers(); }

	std::shared_ptr<const std::vector<float>> savedDropRates;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnGroup>> objectGroups;
	std::vector<Ref<Npc>> objects;
	/** What the AtDistanceProbes received */
	std::vector<int32_t> seen;
};

// QuestItemNpcAI.java:33-38: the object asks QuestEngine.onCanAct(ACTION_ITEM_USE) first. 1103 is registered for it (registerCanAct), but
// AbstractQuestHandler.onCanAct answers false without the quest in START (AbstractQuestHandler.java:226-228) and while the quester already holds
// the 3 sacks the object drops (:247-251): the click does nothing, no bar
TEST_F(QuestItemNpcAiTest, TheObjectRefusesTheClickUnlessAStartedQuestStillNeedsIt) {
	Npc& sack = grainSack();
	installAi(sack);

	sack.getController().onDialogRequest(player());
	EXPECT_EQ(me->sent(), cp::exactly({})) << "no quest";

	questerWithSacks(3);
	sack.getController().onDialogRequest(player());
	EXPECT_EQ(me->sent(), cp::exactly({})) << "the sacks are complete";

	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(me->sent(), cp::exactly({}));
	EXPECT_FALSE(player().getObserveController()->hasObservers());
	EXPECT_FALSE(sack.isDead());
}

// QuestItemNpcAI.java:39: after the quests, the superclass's handleDialogStart still asks DialogService.isInteractionAllowed
// (ActionItemNpcAI.java:37): a quest object summoned for someone else (a PRIVATE summon owner, and the quester is not its creator) refuses a
// quester whose quest wants it
TEST_F(QuestItemNpcAiTest, AnObjectTheQuesterMayNotInteractWithRefusesHimDespiteTheQuest) {
	questerWithSacks(0);
	Npc& sack = grainSack();
	installAi(sack);
	sack.setSummonOwner(skillengine::effect::SummonOwner::PRIVATE);

	sack.getController().onDialogRequest(player());
	executor->advance(std::chrono::milliseconds(5000));

	EXPECT_EQ(me->sent(), cp::exactly({}));
	EXPECT_FALSE(player().getObserveController()->hasObservers());
	EXPECT_FALSE(sack.isDead());
}

// ActionItemNpcAI.java:46-55 on a quest object: a move aborts the bar, and the object is neither asked nor killed
TEST_F(QuestItemNpcAiTest, AMoveAbortsTheQuestersBar) {
	questerWithSacks(0);
	Npc& sack = grainSack();
	installAi(sack);
	const int32_t target = sack.getObjectId();
	sack.getController().onDialogRequest(player());
	me->clearSent();

	move();
	EXPECT_EQ(me->sent(), cp::exactly({emotion(EmotionType::END_QUESTLOOT, target), useObject(player().getObjectId(), target, 0, 2)}));

	me->clearSent();
	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(me->sent(), cp::exactly({}));
	EXPECT_FALSE(sack.isDead());
	EXPECT_FALSE(services::drop::DropRegistrationService::getInstance().getDropRegistrationMap().get(target));
}

// QuestItemNpcAI.java:51-52: a quest takes the finish, but the object has no quest drop (QuestService.getQuestDrop(npcId) is empty): nothing
// is registered and the object stays alive
TEST_F(QuestItemNpcAiTest, WithoutAQuestDropTheFinishLeavesTheObjectAlive) {
	questerWithSacks(0);
	Npc& sack = grainSack();
	installAi(sack);
	const int32_t target = sack.getObjectId();
	sack.getController().onDialogRequest(player());
	const std::vector<uint8_t> endQuestLoot = emotion(EmotionType::END_QUESTLOOT, target);
	me->clearSent();
	services::QuestService::clearQuestDrops(); // the canAct registration of 1103 stays: it comes from the handler's own action items

	executor->advance(std::chrono::milliseconds(3000));

	EXPECT_EQ(me->sent(), cp::exactly({endQuestLoot, useObject(player().getObjectId(), target, 3000, 2)}));
	EXPECT_FALSE(sack.isDead());
	EXPECT_FALSE(services::drop::DropRegistrationService::getInstance().getDropRegistrationMap().get(target));
}

// QuestItemNpcAI.java:44-49: the finish asks the object's talk quests with QuestEnv(object, player, 0, USE_OBJECT) (QuestEngine.onDialog walks
// the npc's onTalkEvent list, QuestEngine.java:165-175); when none takes it, a dialog object shows its default page 1011 (SM_DIALOG_WINDOW(npc,
// 1011): quest 0) and any other object nothing - the ancient cube 700001 (a dialog npc, npc_templates.xml:439522-439526), asked by a fabricated
// handler of the real quest 1101 that answers false, and the grain sack before 1103 starts (ItemCollecting.java:95-116: no quest state and
// not a start npc, so false)
TEST_F(QuestItemNpcAiTest, WhenNoQuestTakesTheFinishOnlyADialogObjectShowsItsDefaultPage) {
	Npc& cube = objectAt(supportTemplateOf(gameserver::ai::testing::ANCIENT_CUBE), 1.0f);
	probe(1101, {}, {gameserver::ai::testing::ANCIENT_CUBE}, false);
	installProbe(cube).finishUse(player());
	ASSERT_EQ(calls.size(), 1u);
	EXPECT_EQ(calls[0].dialogActionId, gameserver::model::DialogAction::USE_OBJECT);
	EXPECT_EQ(calls[0].envQuestId, 1101) << "set by QuestEngine.onDialog while it asks the handler";
	EXPECT_EQ(calls[0].targetObjectId, cube.getObjectId());
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(cube.getObjectId(), 1011, 0)}));
	EXPECT_FALSE(cube.isDead());

	me->clearSent();
	Npc& sack = grainSack();
	installProbe(sack).finishUse(player());
	EXPECT_EQ(me->sent(), cp::exactly({}));
	EXPECT_FALSE(sack.isDead());
}

// QuestItemNpcAI.java:73-76: a quest object that sees a creature runs CreatureEventHandler.onCreatureSee - checkAggro, then, for a player,
// QuestEngine.onAtDistance (CreatureEventHandler.java:32-43), which hands the player to the at-distance events registered for the object (a
// fabricated handler for the real quest 1104; the object is 1 m away, inside the default quest range). NpcAI itself does nothing on a see.
TEST_F(QuestItemNpcAiTest, SeeingAPlayerRunsTheQuestsAtDistanceEvents) {
	Npc& sack = grainSack();
	quests::QuestItemNpcAI& ai = installAi(sack);
	QuestEngine::getInstance().addQuestHandler(std::make_unique<AtDistanceProbe>(1104, GRAIN_SACK, seen));

	ai.onCreatureEvent(AIEventType::CREATURE_SEE, player());

	EXPECT_EQ(seen, (std::vector<int32_t>{player().getObjectId()}));
}

// QuestItemNpcAI.java:33-71 on the bar of ActionItemNpcAI.java:41-73: a quester of 1103 without sacks clicks the object - the 3000 ms bar; at
// its end the object asks the quests with USE_OBJECT (ItemCollecting.java:151-152 answers true, "looting"), finds its quest drop, registers
// the drop for the quester alone (DropRegistrationService.registerDrop: SM_LOOT_STATUS LOOT_ENABLE to the looter, DropRegistrationService.java:
// 104-106; QuestService.getQuestDrop: one Kerub Grain Sack for him, index 1, QuestService.java:727-729), dies by him and opens his loot window
// (DropService.requestDropList, DropService.java:90-136: the item list, OPEN_DROP_LIST, the START_LOOT emotion).
// registerDrop's drop modifiers ask the looter's house (DropRegistrationService.calculateBoostDropRate -> Player.getActiveHouse ->
// HousingService, whose constructor loads the houses and the player ids from the database), so the case needs the test database of the DAO
// tests (tests/dao/DaoTestDatabase.h, emptied: no house, no player), as the drop tests do (tests/economy/P5-09a/DropTestSupport.h). It is the
// last case of the file because it initializes DatabaseFactory for its process (ctest runs every case in its own).
TEST_F(QuestItemNpcAiTest, AQuesterRunsTheBarAndTheFinishLootsTheObject) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: registerDrop reads the looter's houses (HousingService, the database)";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	questerWithSacks(0);
	Npc& sack = grainSack();
	installAi(sack);
	const int32_t questerId = player().getObjectId();
	const int32_t target = sack.getObjectId();
	// the quester and the object see each other (Java's knownlist update), so the object's death reaches him: CreatureController.onDie
	// broadcasts SM_EMOTION(object, DIE, 0, killer) to the object's known players (CreatureController.java:164-165)
	ASSERT_TRUE(::aion::gameserver::ai::testing::AiKnownListPairing::pair(player(), sack));
	me->clearSent(); // SM_NPC_INFO of the pairing

	sack.getController().onDialogRequest(player());

	EXPECT_EQ(me->sent(), cp::exactly({useObject(questerId, target, 3000, 1), emotion(EmotionType::START_QUESTLOOT, target)}));
	EXPECT_TRUE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	const std::vector<uint8_t> endQuestLoot = emotion(EmotionType::END_QUESTLOOT, target); // before the loot window changes his state
	me->clearSent();

	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_EQ(me->sent(), cp::exactly({}));
	EXPECT_FALSE(sack.isDead());

	executor->advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(sack.isDead()) << "AIActions.die(this, player)";
	// registerDrop (LOOT_ENABLE), then the death by the quester - CreatureController.onDie's removeAllEffects broadcasts the object's effects
	// (CreatureController.java:157, EffectController.java:660-671) and its DIE emotion names him as the killer (:164-165) -, then the loot window
	const std::vector<uint8_t> diedByTheQuester = me->serializedFor(SM_EMOTION(sack, EmotionType::DIE, 0, questerId));
	EXPECT_EQ(me->sent(), cp::exactly({endQuestLoot, useObject(questerId, target, 3000, 2), lootStatus(target, 0), noAbnormalEffects(target),
							  diedByTheQuester, lootItemList(target, 1, KERUB_GRAIN_SACK, 1), lootStatus(target, 2),
							  emotion(EmotionType::START_LOOT, target)}));
	Ptr<runtime::RcHashSet<Ref<gameserver::model::drop::DropItem>>> drops =
		services::drop::DropRegistrationService::getInstance().getCurrentDropMap().get(target);
	ASSERT_TRUE(drops);
	std::vector<Ptr<gameserver::model::drop::DropItem>> entries = drops->snapshot();
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_EQ(entries[0]->getDropTemplate()->getItemId(), KERUB_GRAIN_SACK);
	EXPECT_EQ(entries[0]->getCount(), 1);
	EXPECT_EQ(entries[0]->getIndex(), 1);
	EXPECT_EQ(entries[0]->getPlayerObjIds().snapshot(), std::vector<int32_t>{questerId}) << "the solo quester's own entry";
	Ptr<gameserver::model::gameobjects::DropNpc> dropNpc = services::drop::DropRegistrationService::getInstance().getDropRegistrationMap().get(target);
	ASSERT_TRUE(dropNpc);
	EXPECT_TRUE(dropNpc->isAllowedToLoot(player()));
	EXPECT_EQ(player().getLootingNpcOid(), target);
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
