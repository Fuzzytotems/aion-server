// Q10 (P6-Q slice 2, 2026-09-29): the five hand-ported quests of altgard and pandaemonium (docs/deviations/Q10.md, "Hand ports") through the
// real QuestEngine on the fixture of AsmodaeQuestTestSupport.h, every hook of each, with its Java lines beside the case; and the two generated
// paths that kill their target, which the golden harness cannot pair (24012's cart, 2223's bones). The review of 2026-09-29 added the cases
// its mutants showed missing (the section at the end) and the generated hooks the golden harness leaves unobserved (24010, 24016, 2925, 2263).

#include "AsmodaeQuestTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/MapRegion.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

#define AION_ASMODAE_FACTORY(dir, Class)                                                                                                       \
	namespace aion::gameserver::handlers::quest::dir {                                                                                           \
	::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> Class##_questFactory();                                  \
	}
AION_ASMODAE_FACTORY(altgard, _2208MauInTenMinutesADay)
AION_ASMODAE_FACTORY(altgard, _2230AFriendlyWager)
AION_ASMODAE_FACTORY(altgard, _2252ChasingtheLegend)
AION_ASMODAE_FACTORY(altgard, _24013PoisonInTheWaters)
AION_ASMODAE_FACTORY(pandaemonium, _2900NoEscapingDestiny)
AION_ASMODAE_FACTORY(altgard, _2223AMythicalMonster)
AION_ASMODAE_FACTORY(altgard, _24012AnOminousCrop)
AION_ASMODAE_FACTORY(altgard, _24010SuthransOrders)
AION_ASMODAE_FACTORY(altgard, _24016AStrangeNewThread)
AION_ASMODAE_FACTORY(altgard, _2263ShugoPotion)
AION_ASMODAE_FACTORY(pandaemonium, _2925AHeartfeltConfession)
#undef AION_ASMODAE_FACTORY

namespace aion::gameserver::questEngine::handlers::test::asmodae {
namespace {

namespace DA = ::aion::gameserver::model::DialogAction;
namespace altgard = ::aion::gameserver::handlers::quest::altgard;
namespace pandaemonium = ::aion::gameserver::handlers::quest::pandaemonium;
using gameserver::model::PlayerClass;
using gameserver::model::Race;
using gameserver::model::TaskId;

std::vector<int32_t> talkList(int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().snapshot(); }
std::vector<int32_t> killList(int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnKillEvent().snapshot(); }
bool startsAt(int32_t npcId, int32_t questId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnQuestStart().contains(questId); }

class AsmodaeHandPortsTest : public AsmodaeQuestTest {};

// --- the registrations (Java register(), in statement order; the order across lists is pinned by tools/parity's ordered checks) ----------

TEST_F(AsmodaeHandPortsTest, TheRegistrationsAreTheJavaOnes) {
	// _2208MauInTenMinutesADay.java:27-32
	install(altgard::_2208MauInTenMinutesADay_questFactory());
	EXPECT_TRUE(startsAt(203591, 2208));
	EXPECT_EQ(talkList(203591), std::vector<int32_t>{2208});
	EXPECT_EQ(talkList(203589), std::vector<int32_t>{2208});
	EXPECT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(182203205));
	// _2230AFriendlyWager.java:27-32 (the timer end and the logout reach it below)
	install(altgard::_2230AFriendlyWager_questFactory());
	EXPECT_TRUE(startsAt(203621, 2230));
	EXPECT_EQ(talkList(203621), std::vector<int32_t>{2230});
	// _2252ChasingtheLegend.java:33-39
	install(altgard::_2252ChasingtheLegend_questFactory());
	EXPECT_TRUE(startsAt(203646, 2252));
	EXPECT_EQ(talkList(203646), std::vector<int32_t>{2252});
	EXPECT_EQ(talkList(700060), std::vector<int32_t>{2252});
	EXPECT_EQ(killList(210634), std::vector<int32_t>{2252});
	EXPECT_EQ(killList(210635), std::vector<int32_t>{2252});
	// _24013PoisonInTheWaters.java:27-36: the five mobs, the item, 203631 and 203621 (after 2230's talk)
	install(altgard::_24013PoisonInTheWaters_questFactory());
	for (int32_t mob : {210455, 210456, 214039, 210458, 214032})
		EXPECT_EQ(killList(mob), std::vector<int32_t>{24013}) << mob;
	EXPECT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(182215359));
	EXPECT_EQ(talkList(203631), std::vector<int32_t>{24013});
	EXPECT_EQ(talkList(203621), (std::vector<int32_t>{2230, 24013}));
	EXPECT_FALSE(startsAt(203631, 24013)) << "a mission: no start npc";
	// _2900NoEscapingDestiny.java:30-43
	install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	EXPECT_EQ(killList(204263), std::vector<int32_t>{2900});
	for (int32_t npc : {204182, 203550, 790003, 790002, 203546, 204264, 204061})
		EXPECT_EQ(talkList(npc), std::vector<int32_t>{2900}) << npc;

	// the engine events reach exactly the registered hooks: 2230's timer end and logout, 2900's death (a quest state makes the hook act; a
	// Daeva class, which has a stigma stone)
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	hold(*q, 2230, QuestStatus::START);
	hold(*q, 2900, QuestStatus::START, 95);
	QuestEngine::getInstance().onQuestTimerEnd(envFor(*q, nullptr, 0));
	EXPECT_EQ(statusOf(*q, 2230), QuestStatus::START) << "the timer end only cancels the task";
	QuestEngine::getInstance().onDie(envFor(*q, nullptr, 0));
	EXPECT_EQ(varOf(*q, 2900), 4) << "onDieEvent: the step back to 4 (_2900NoEscapingDestiny.java:211-214)";
	QuestEngine::getInstance().onLogOut(envFor(*q, nullptr, 0));
	EXPECT_EQ(statusOf(*q, 2230), std::nullopt) << "onLogOutEvent abandons 2230 (never completed: deleted)";
	EXPECT_EQ(statusOf(*q, 2900), QuestStatus::START) << "2900 has no logout hook";
}

// --- 2208 Mau In Ten Minutes A Day -----------------------------------------------------------------------------------------------------

TEST_F(AsmodaeHandPortsTest, MauGivesTheItemAtTheStartAndTheReportGoesToReward) {
	install(altgard::_2208MauInTenMinutesADay_questFactory());
	Quester* q = quester(Race::ASMODIANS, 11); // quest_data.xml: 2208 minlevel_permitted 11, <finished quest_id="2207"/>
	hold(*q, 2207, QuestStatus::COMPLETE);
	Npc& aki = spawnNpc(203591);
	Npc& mau = spawnNpc(203589, 104.0f);
	EXPECT_TRUE(dialog(*q, aki, 2208, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(aki.getObjectId(), 1011, 2208))) << ":43-44";
	// the review of 2026-09-29: every other action goes to sendQuestStartDialog (:48-49): the refusal's page 1004
	EXPECT_TRUE(dialog(*q, aki, 2208, DA::QUEST_REFUSE_1));
	EXPECT_TRUE(sentTo(*q, dialogWindow(aki.getObjectId(), 1004, 2208))) << ":48-49";
	EXPECT_EQ(statusOf(*q, 2208), std::nullopt);
	EXPECT_TRUE(dialog(*q, aki, 2208, DA::QUEST_ACCEPT_1));
	EXPECT_EQ(held(*q, 182203205), 1) << ":45-47 giveQuestItem, then the start";
	EXPECT_EQ(statusOf(*q, 2208), QuestStatus::START);
	q->clearSent();
	EXPECT_TRUE(dialog(*q, mau, 2208, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(mau.getObjectId(), 1693, 2208))) << ":56-57 var 0";
	q->player().getQuestStateList()->getQuestState(2208)->setQuestVarById(0, 1);
	EXPECT_TRUE(dialog(*q, mau, 2208, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(mau.getObjectId(), 1352, 2208))) << ":58-59 var 1";
	q->clearSent();
	EXPECT_TRUE(dialog(*q, mau, 2208, DA::SETPRO1));
	EXPECT_EQ(statusOf(*q, 2208), QuestStatus::REWARD) << ":60-63";
	EXPECT_TRUE(sentTo(*q, questUpdate(2208, REWARD, 1))) << ":62 updateQuestStatus";
	EXPECT_TRUE(sentTo(*q, dialogWindow(mau.getObjectId(), 10, 0))) << ":63 sendQuestSelectionDialog";
	q->clearSent();
	EXPECT_FALSE(dialog(*q, mau, 2208, DA::QUEST_SELECT)) << "REWARD: only Aki answers (:66-68)";
	EXPECT_TRUE(dialog(*q, aki, 2208, DA::USE_OBJECT)) << ":67-68 sendQuestEndDialog: the reward page";
}

TEST_F(AsmodaeHandPortsTest, MausItemPlaysItsAnimationAndSetsTheVarAfterThreeSeconds) {
	AbstractQuestHandler& handler = install(altgard::_2208MauInTenMinutesADay_questFactory());
	Quester* q = quester(Race::ASMODIANS, 9);
	gameserver::model::gameobjects::Item& other = holdItem(*q, 840001, 182203223, 1);
	EXPECT_EQ(handler.onItemUseEvent(envFor(*q, nullptr, 2208), other), HandlerResult::UNKNOWN) << ":79-80 another item";
	gameserver::model::gameobjects::Item& mauItem = holdItem(*q, 840002, 182203205, 2);
	EXPECT_EQ(handler.onItemUseEvent(envFor(*q, nullptr, 2208), mauItem), HandlerResult::FAILED) << ":81-83 no quest state";
	hold(*q, 2208, QuestStatus::START);
	q->clearSent();
	int32_t playerObjId = q->player().getObjectId();
	EXPECT_EQ(handler.onItemUseEvent(envFor(*q, nullptr, 2208), mauItem), HandlerResult::SUCCESS);
	EXPECT_TRUE(sentTo(*q, itemUsageAnimation(playerObjId, 840002, 182203205, 3000, 0, 0))) << ":84";
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_EQ(varOf(*q, 2208), 0);
	EXPECT_EQ(held(*q, 182203205), 2);
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(sentTo(*q, itemUsageAnimation(playerObjId, 840002, 182203205, 0, 1, 0))) << ":89";
	EXPECT_EQ(held(*q, 182203205), 1) << ":90 decreaseByObjectId by 1";
	EXPECT_EQ(varOf(*q, 2208), 1) << ":91";
	EXPECT_TRUE(sentTo(*q, questUpdate(2208, START, 1))) << ":92 updateQuestStatus";
}

// --- 2230 A Friendly Wager --------------------------------------------------------------------------------------------------------------

TEST_F(AsmodaeHandPortsTest, TheWagerRunsOnTheQuestTimer) {
	AbstractQuestHandler& handler = install(altgard::_2230AFriendlyWager_questFactory());
	Quester* q = quester(Race::ASMODIANS, 12);
	hold(*q, 2288, QuestStatus::COMPLETE); // the start condition (quest_data.xml, 2230's <finished quest_id="2288"/>)
	Npc& shania = spawnNpc(203621);
	controllers::PlayerController& controller = q->player().getController();
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 1011, 2230))) << ":52-53";
	// the review of 2026-09-29: the switch's default is sendQuestStartDialog (:54-55): the refusal's page 1004
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::QUEST_REFUSE_1));
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 1004, 2230))) << ":54-55";
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::QUEST_ACCEPT_1));
	EXPECT_EQ(statusOf(*q, 2230), QuestStatus::START) << ":46-50";
	EXPECT_TRUE(controller.hasTask(TaskId::QUEST_TIMER)) << ":48 questTimerStart(1800)";
	EXPECT_TRUE(sentTo(*q, q->serializedFor(network::aion::serverpackets::SM_QUEST_ACTION(2230, 1800)))) << ":48 the client's timer: 1,800 s";
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 1003, 2230)));
	q->clearSent();
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 2375, 2230))) << ":59-60";

	// a report with the timer running and too few tusks: page 2716 (:68-75); with ten: REWARD, the timer ended, page 5 (:69-73)
	holdItem(*q, 840003, 182203223, 9);
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 2716, 2230)));
	EXPECT_EQ(statusOf(*q, 2230), QuestStatus::START);
	holdItem(*q, 840004, 182203223, 1);
	q->clearSent();
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_EQ(statusOf(*q, 2230), QuestStatus::REWARD);
	EXPECT_EQ(held(*q, 182203223), 0) << ":69 collectItemCheck(env, true) takes the ten tusks";
	EXPECT_TRUE(sentTo(*q, questUpdate(2230, REWARD, 0))) << ":71 updateQuestStatus";
	EXPECT_FALSE(controller.hasTask(TaskId::QUEST_TIMER)) << "questTimerEnd";
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 5, 2230)));
	q->clearSent();
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::USE_OBJECT)) << ":83-84 REWARD: sendQuestEndDialog";
	EXPECT_FALSE(handler.onQuestTimerEndEvent(envFor(*q, nullptr, 2230))) << ":96 not START";
	EXPECT_FALSE(handler.onLogOutEvent(envFor(*q, nullptr, 2230))) << ":109 not START";
}

TEST_F(AsmodaeHandPortsTest, TheWagerAfterTheTimeRemovesTheTusksAndANewChanceRestartsTheTimer) {
	AbstractQuestHandler& handler = install(altgard::_2230AFriendlyWager_questFactory());
	Quester* q = quester(Race::ASMODIANS, 12);
	Npc& shania = spawnNpc(203621);
	controllers::PlayerController& controller = q->player().getController();
	hold(*q, 2230, QuestStatus::START);
	holdItem(*q, 840005, 182203223, 4);
	EXPECT_FALSE(controller.hasTask(TaskId::QUEST_TIMER));
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_EQ(held(*q, 182203223), 0) << ":77-79 time ended: every tusk removed";
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 3057, 2230))) << ":80";
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::SETPRO1));
	EXPECT_TRUE(controller.hasTask(TaskId::QUEST_TIMER)) << ":62-64 a new chance";
	// the timer's end (the engine's hook): the task is cancelled, the quest stays
	EXPECT_TRUE(handler.onQuestTimerEndEvent(envFor(*q, nullptr, 2230))) << ":96-99";
	EXPECT_FALSE(controller.hasTask(TaskId::QUEST_TIMER));
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::SETPRO1));
	EXPECT_TRUE(controller.hasTask(TaskId::QUEST_TIMER));
	holdItem(*q, 840006, 182203223, 3);
	EXPECT_TRUE(handler.onLogOutEvent(envFor(*q, nullptr, 2230))) << ":109-113";
	EXPECT_EQ(held(*q, 182203223), 0);
	EXPECT_EQ(statusOf(*q, 2230), std::nullopt) << "abandonQuest: never completed, deleted";
}

// --- 2252 Chasing the Legend ------------------------------------------------------------------------------------------------------------

TEST_F(AsmodaeHandPortsTest, TheLegendStartsWithTheBonesAndTheKillSetsTheRewardGroup) {
	AbstractQuestHandler& handler = install(altgard::_2252ChasingtheLegend_questFactory());
	Quester* q = quester(Race::ASMODIANS, 14);
	Npc& sinood = spawnNpc(203646);
	EXPECT_TRUE(dialog(*q, sinood, 2252, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(sinood.getObjectId(), 1011, 2252))) << ":49-50";
	EXPECT_TRUE(dialog(*q, sinood, 2252, DA::QUEST_ACCEPT_1));
	EXPECT_EQ(statusOf(*q, 2252), QuestStatus::START);
	EXPECT_EQ(held(*q, 182203235), 1) << ":52-54 the start gives the bones";
	EXPECT_TRUE(sentTo(*q, dialogWindow(sinood.getObjectId(), 1003, 2252)));
	// START var 0 at Sinood: giveQuestItem answers true for a constant item and count (AbstractQuestHandler.java:626-641), so page 1693; it
	// gives the bones only when none is held (the review of 2026-09-29), else it sends STR_CAN_NOT_GET_LORE_ITEM
	q->clearSent();
	EXPECT_TRUE(dialog(*q, sinood, 2252, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(sinood.getObjectId(), 1693, 2252))) << ":64-68";
	EXPECT_EQ(varOf(*q, 2252), 0);
	EXPECT_EQ(held(*q, 182203235), 1) << "no second bone";
	q->player().getInventory().decreaseByItemId(182203235, 1);
	EXPECT_TRUE(dialog(*q, sinood, 2252, DA::QUEST_SELECT));
	EXPECT_EQ(held(*q, 182203235), 1) << "a new chance: the bones again";

	// the kills (:108-132): the spirit is the high reward (var 1), the drakie the low one (var 2)
	Npc& spirit = npcOf(210634);
	Npc& drakie = npcOf(210635);
	EXPECT_TRUE(handler.onKillEvent(envFor(*q, spirit, 2252)));
	EXPECT_EQ(statusOf(*q, 2252), QuestStatus::REWARD);
	EXPECT_EQ(varOf(*q, 2252), 1);
	EXPECT_FALSE(handler.onKillEvent(envFor(*q, drakie, 2252))) << ":110-112 not START";
	EXPECT_TRUE(dialog(*q, sinood, 2252, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(sinood.getObjectId(), 1352, 2252))) << ":100-101 default";
	EXPECT_TRUE(dialog(*q, sinood, 2252, DA::SELECT_QUEST_REWARD));
	EXPECT_EQ(q->player().getQuestStateList()->getQuestState(2252)->getRewardGroup(), 0) << ":96-98 var - 1";

	Quester* low = quester(Race::ASMODIANS, 14);
	hold(*low, 2252, QuestStatus::START);
	EXPECT_TRUE(handler.onKillEvent(envFor(*low, drakie, 2252)));
	EXPECT_EQ(varOf(*low, 2252), 2);
	EXPECT_EQ(statusOf(*low, 2252), QuestStatus::REWARD);
	EXPECT_TRUE(dialog(*low, sinood, 2252, DA::SELECTED_QUEST_NOREWARD));
	EXPECT_EQ(low->player().getQuestStateList()->getQuestState(2252)->getRewardGroup(), 1);
	Quester* none = quester(Race::ASMODIANS, 14);
	EXPECT_FALSE(handler.onKillEvent(envFor(*none, spirit, 2252))) << ":110-112 no quest";
}

TEST_F(AsmodaeHandPortsTest, TheBonesCallUpOneLegendAtATimeForThreeMinutes) {
	install(altgard::_2252ChasingtheLegend_questFactory());
	Quester* q = quester(Race::ASMODIANS, 14);
	hold(*q, 2252, QuestStatus::START);
	Npc& bones = spawnNpc(700060, 106.0f, 101.0f, 50.0f, int8_t{25});
	EXPECT_FALSE(dialog(*q, bones, 2252, DA::USE_OBJECT)) << ":78-80 without the bones item: nothing";
	EXPECT_TRUE(spawnedOf(210634).empty());
	EXPECT_TRUE(spawnedOf(210635).empty());
	holdItem(*q, 840007, 182203235, 1);
	commons::utils::Rnd::seedCurrentThreadForTests(17);
	EXPECT_FALSE(dialog(*q, bones, 2252, DA::USE_OBJECT)) << "the case falls out of the switch: false (:94)";
	std::vector<Ptr<Npc>> legends = spawnedOf(210634);
	std::vector<Ptr<Npc>> drakies = spawnedOf(210635);
	ASSERT_EQ(legends.size() + drakies.size(), 1u) << ":82-86 one of the two, by Rnd.chance() < 95";
	Npc& legend = legends.empty() ? *drakies.front() : *legends.front();
	EXPECT_FLOAT_EQ(legend.getX(), 106.0f) << ":85 at the bones";
	EXPECT_FLOAT_EQ(legend.getY(), 101.0f);
	EXPECT_FLOAT_EQ(legend.getZ(), 50.0f);
	EXPECT_EQ(legend.getHeading(), 25);
	EXPECT_EQ(held(*q, 182203235), 0) << "checkItemExistence(..., true) removes it (:81; AbstractQuestHandler.java checkItemExistence)";
	EXPECT_FALSE(dialog(*q, bones, 2252, DA::USE_OBJECT)) << ":78-79 a legend is up: false";
	EXPECT_EQ(spawnedOf(210634).size() + spawnedOf(210635).size(), 1u);
	executor->advance(std::chrono::minutes(3));
	EXPECT_FALSE(legend.isSpawned()) << ":83 spawnTime 3 minutes";
}

// --- 24013 Poison in the Waters ---------------------------------------------------------------------------------------------------------

TEST_F(AsmodaeHandPortsTest, PoisonInTheWatersSteps) {
	AbstractQuestHandler& handler = install(altgard::_24013PoisonInTheWaters_questFactory());
	Quester* q = quester(Race::ASMODIANS, 14);
	Npc& atkin = spawnNpc(203631);
	Npc& shania = spawnNpc(203621, 104.0f);
	EXPECT_FALSE(dialog(*q, atkin, 24013, DA::QUEST_SELECT)) << ":42-43 no quest state";
	hold(*q, 24013, QuestStatus::START);
	EXPECT_TRUE(dialog(*q, atkin, 24013, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(atkin.getObjectId(), 1011, 24013))) << ":53-55";
	EXPECT_TRUE(dialog(*q, atkin, 24013, DA::SETPRO1));
	EXPECT_EQ(varOf(*q, 24013), 1) << ":57-58";
	EXPECT_FALSE(dialog(*q, atkin, 24013, DA::QUEST_SELECT)) << ":56, :60 var 1 at Atkin";
	EXPECT_TRUE(dialog(*q, shania, 24013, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 1352, 24013))) << ":63-65";
	EXPECT_TRUE(dialog(*q, shania, 24013, DA::SETPRO2));
	EXPECT_EQ(varOf(*q, 24013), 2) << ":67-68";
	EXPECT_EQ(held(*q, 182215359), 1);

	// the kills (:99-103): 3..6 count up, 7 goes to REWARD
	q->player().getQuestStateList()->getQuestState(24013)->setQuestVarById(0, 3);
	Npc& mob = npcOf(210455);
	for (int32_t var = 4; var <= 7; var++) {
		EXPECT_TRUE(handler.onKillEvent(envFor(*q, mob, 24013)));
		EXPECT_EQ(varOf(*q, 24013), var);
	}
	EXPECT_EQ(statusOf(*q, 24013), QuestStatus::START);
	EXPECT_TRUE(handler.onKillEvent(envFor(*q, npcOf(214032), 24013)));
	EXPECT_EQ(statusOf(*q, 24013), QuestStatus::REWARD) << ":100 at 7: reward";
	q->clearSent();
	EXPECT_TRUE(dialog(*q, atkin, 24013, DA::USE_OBJECT)) << ":71-74 REWARD at Atkin";
	EXPECT_FALSE(dialog(*q, shania, 24013, DA::USE_OBJECT));
}

TEST_F(AsmodaeHandPortsTest, TheSeedInTheZoneCallsTwoSharpeyesAfterThreeSeconds) {
	AbstractQuestHandler& handler = install(altgard::_24013PoisonInTheWaters_questFactory());
	Quester* q = quester(Race::ASMODIANS, 14);
	hold(*q, 24013, QuestStatus::START, 2);
	gameserver::model::gameobjects::Item& seed = holdItem(*q, 840008, 182215359, 1);
	// the zone update of a move (ZoneUpdateService: MapRegion.revalidateZones) 50 m from the zone's axis, then back at its centre
	q->player().getPosition()->setXYZH(150.0f, 100.0f, 50.0f, int8_t{0});
	q->player().getPosition()->getMapRegion()->revalidateZones(q->player());
	EXPECT_EQ(handler.onItemUseEvent(envFor(*q, nullptr, 24013), seed), HandlerResult::UNKNOWN) << ":82, :95 outside the zone";
	q->player().getPosition()->setXYZH(100.0f, 100.0f, 50.0f, int8_t{0});
	q->player().getPosition()->getMapRegion()->revalidateZones(q->player());
	ASSERT_TRUE(q->player().isInsideZone(world::zone::ZoneName::get("DF1A_ITEMUSEAREA_Q2016_220030000")));
	EXPECT_EQ(handler.onItemUseEvent(envFor(*q, nullptr, 24013), seed), HandlerResult::SUCCESS) << ":93 useQuestItem(2, 3)";
	EXPECT_TRUE(spawnedOf(210457).empty());
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_TRUE(spawnedOf(210457).empty());
	executor->advance(std::chrono::milliseconds(1));
	std::vector<Ptr<Npc>> sharpeyes = spawnedOf(210457);
	ASSERT_EQ(sharpeyes.size(), 2u) << ":85-91 the two Feral Black Claw Sharpeyes";
	for (const Ptr<Npc>& npc : sharpeyes)
		spawned.emplace_back(*npc);
	std::vector<float> ys{sharpeyes[0]->getY(), sharpeyes[1]->getY()};
	std::sort(ys.begin(), ys.end());
	EXPECT_FLOAT_EQ(sharpeyes[0]->getX(), 113.0f) << "x + 13";
	EXPECT_FLOAT_EQ(sharpeyes[1]->getX(), 113.0f);
	EXPECT_EQ(ys, (std::vector<float>{97.0f, 103.0f})) << "y - 3 (right) and y + 3 (left)";
	for (const Ptr<Npc>& npc : sharpeyes) {
		EXPECT_FLOAT_EQ(npc->getZ(), 50.0f) << "the player's z (:88-89), both";
		EXPECT_EQ(npc->getHeading(), 60) << "(byte) 60, both";
	}
	EXPECT_EQ(varOf(*q, 24013), 3) << "useQuestItem's own 3 s task: 2 -> 3";
	EXPECT_EQ(statusOf(*q, 24013), QuestStatus::START) << "useQuestItem(env, item, 2, 3, false): no reward, the kills 3..7 follow";
	EXPECT_EQ(held(*q, 182215359), 0);
}

TEST_F(AsmodaeHandPortsTest, PoisonInTheWatersStartsAfterSuthransOrders) {
	install(altgard::_24013PoisonInTheWaters_questFactory());
	Quester* q = quester(Race::ASMODIANS, 14);
	QuestEngine::getInstance().onLevelChanged(q->player());
	EXPECT_EQ(statusOf(*q, 24013), std::nullopt) << ":112 defaultOnLevelChangedEvent(player, 24010): 24010 not finished";
	hold(*q, 24010, QuestStatus::COMPLETE);
	QuestEngine::getInstance().onLevelChanged(q->player());
	EXPECT_EQ(statusOf(*q, 24013), QuestStatus::START);
	Quester* r = quester(Race::ASMODIANS, 14);
	hold(*r, 24010, QuestStatus::COMPLETE);
	QuestEngine::getInstance().onQuestCompleted(r->player(), 24010);
	EXPECT_EQ(statusOf(*r, 24013), QuestStatus::START) << ":107 defaultOnQuestCompletedEvent(env, 24010)";
}

// --- 2900 No Escaping Destiny -----------------------------------------------------------------------------------------------------------

TEST_F(AsmodaeHandPortsTest, DestinyDialogsOfTheStepsWithoutATeleport) {
	AbstractQuestHandler& handler = install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	Npc& urd = spawnNpc(790003);
	Npc& verdandi = spawnNpc(790002, 103.0f);
	Npc& skuld = spawnNpc(203546, 104.0f);
	Npc& skuld2 = spawnNpc(204264, 105.0f);
	Npc& aud = spawnNpc(204061, 106.0f);
	EXPECT_FALSE(dialog(*q, urd, 2900, DA::QUEST_SELECT)) << ":49-51 no quest state";
	Ref<QuestState> qs = hold(*q, 2900, QuestStatus::START, 2);
	EXPECT_TRUE(dialog(*q, urd, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(urd.getObjectId(), 1693, 2900))) << ":91-94";
	EXPECT_TRUE(dialog(*q, urd, 2900, DA::SETPRO3));
	EXPECT_EQ(varOf(*q, 2900), 3) << ":96-97";
	EXPECT_FALSE(dialog(*q, urd, 2900, DA::QUEST_SELECT)) << ":95 var 3 at Urd";
	EXPECT_TRUE(dialog(*q, verdandi, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(verdandi.getObjectId(), 2034, 2900))) << ":102-104";
	EXPECT_TRUE(dialog(*q, verdandi, 2900, DA::SETPRO4));
	EXPECT_EQ(varOf(*q, 2900), 4) << ":107-108";
	EXPECT_TRUE(dialog(*q, skuld, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(skuld.getObjectId(), 2375, 2900))) << ":113-115";
	qs->setQuestVar(9);
	EXPECT_TRUE(dialog(*q, skuld, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(skuld.getObjectId(), 3739, 2900))) << ":116-117";

	// Skuld inside the Space of Destiny (204264): the pages of the vars 95..99 (:136-144)
	for (auto [var, page] : std::vector<std::pair<int32_t, int32_t>>{{95, 2716}, {96, 3057}, {99, 3057}, {97, 3398}}) {
		qs->setQuestVar(var);
		q->clearSent();
		EXPECT_TRUE(dialog(*q, skuld2, 2900, DA::QUEST_SELECT)) << var;
		EXPECT_TRUE(sentTo(*q, dialogWindow(skuld2.getObjectId(), page, 2900))) << var;
	}
	qs->setQuestVar(98);
	EXPECT_FALSE(dialog(*q, skuld2, 2900, DA::QUEST_SELECT)) << ":144";
	// SETPRO6 at 95: movie 156 and the window closed (:145-149); the movie's end sets 96 (:179-182)
	qs->setQuestVar(95);
	q->clearSent();
	EXPECT_TRUE(dialog(*q, skuld2, 2900, DA::SETPRO6));
	EXPECT_TRUE(sentTo(*q, playMovie(false, skuld2.getObjectId(), 2900, 156)));
	EXPECT_TRUE(sentTo(*q, dialogWindow(skuld2.getObjectId(), 0, 0))) << "closeDialogWindow";
	handler.onMovieEndEvent(envFor(*q, nullptr, 2900), 155);
	EXPECT_EQ(varOf(*q, 2900), 95) << "another movie";
	handler.onMovieEndEvent(envFor(*q, nullptr, 2900), 156);
	EXPECT_EQ(varOf(*q, 2900), 96);
	// SELECT7_1: the stigma stone of the class (a Gladiator: 140000003, :251-254) and 96 -> 99, page 3058 (:151-154)
	EXPECT_TRUE(dialog(*q, skuld2, 2900, DA::SELECT7_1));
	EXPECT_EQ(held(*q, 140000003), 1);
	EXPECT_EQ(varOf(*q, 2900), 99);
	EXPECT_TRUE(sentTo(*q, dialogWindow(skuld2.getObjectId(), 3058, 2900)));
	// SETPRO7 at 99: the stigma window (DialogPage.STIGMA, id 1; :155-158)
	q->clearSent();
	EXPECT_TRUE(dialog(*q, skuld2, 2900, DA::SETPRO7));
	EXPECT_TRUE(sentTo(*q, dialogWindow(skuld2.getObjectId(), 1, 0)));
	// the stone equipped: 99 -> 97 and the window closed (:184-188)
	EXPECT_TRUE(handler.onEquipItemEvent(envFor(*q, nullptr, 2900), 140000003));
	EXPECT_EQ(varOf(*q, 2900), 97);
	// SETPRO8 at 97: 98 and the guardian (204263) at (257.5, 245, 125) for five minutes (:161-165)
	EXPECT_TRUE(dialog(*q, skuld2, 2900, DA::SETPRO8));
	EXPECT_EQ(varOf(*q, 2900), 98);
	std::vector<Ptr<Npc>> guardians = spawnedOf(204263);
	ASSERT_EQ(guardians.size(), 1u);
	spawned.emplace_back(*guardians.front());
	EXPECT_FLOAT_EQ(guardians.front()->getX(), 257.5f);
	EXPECT_FLOAT_EQ(guardians.front()->getY(), 245.0f);
	executor->advance(std::chrono::minutes(5));
	EXPECT_FALSE(guardians.front()->isSpawned());
	// REWARD: only Aud (:170-173)
	qs->setStatus(QuestStatus::REWARD);
	EXPECT_FALSE(dialog(*q, skuld2, 2900, DA::USE_OBJECT));
	EXPECT_TRUE(dialog(*q, aud, 2900, DA::USE_OBJECT));
}

TEST_F(AsmodaeHandPortsTest, DestinyDeathAndReturnTakeTheStigmaBack) {
	AbstractQuestHandler& handler = install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::CHANTER);
	Ref<QuestState> qs = hold(*q, 2900, QuestStatus::START, 97);
	holdItem(*q, 840009, 140000001, 1); // a Chanter's stone (:243-246)
	EXPECT_TRUE(handler.onDieEvent(envFor(*q, nullptr, 2900))) << ":209-215";
	EXPECT_EQ(held(*q, 140000001), 0) << "removeStigma: removeQuestItem(stigmaId, 1) (:263-270)";
	EXPECT_EQ(varOf(*q, 2900), 4);
	EXPECT_FALSE(handler.onDieEvent(envFor(*q, nullptr, 2900))) << "var 4";

	// the enter world outside the Space of Destiny (the fixture's Poeta map): 95..99 back to 4, 9 keeps its step (:221-238)
	qs->setQuestVar(99);
	holdItem(*q, 840010, 140000001, 1);
	EXPECT_TRUE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900)));
	EXPECT_EQ(varOf(*q, 2900), 4);
	EXPECT_EQ(held(*q, 140000001), 0);
	qs->setQuestVar(9);
	holdItem(*q, 840011, 140000001, 1);
	EXPECT_TRUE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900)));
	EXPECT_EQ(varOf(*q, 2900), 9);
	EXPECT_EQ(held(*q, 140000001), 0);
	qs->setQuestVar(3);
	EXPECT_FALSE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900)));

	// a starting class has no stone: Java throws (:258-259), and so does the port
	Quester* warrior = quester(Race::ASMODIANS, 20, PlayerClass::WARRIOR);
	hold(*warrior, 2900, QuestStatus::START, 96);
	try {
		handler.onDieEvent(envFor(*warrior, nullptr, 2900));
		ADD_FAILURE() << "no exception";
	} catch (const runtime::UnsupportedOperationException& e) {
		EXPECT_EQ(std::string(e.what()), "Unhandled player class WARRIOR");
	}
	EXPECT_EQ(varOf(*warrior, 2900), 96);
}

TEST_F(AsmodaeHandPortsTest, DestinyStartsAtTheLevelOfTheMission) {
	install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* low = quester(Race::ASMODIANS, 17, PlayerClass::GLADIATOR);
	QuestEngine::getInstance().onLevelChanged(low->player());
	EXPECT_EQ(statusOf(*low, 2900), std::nullopt) << "a mission starts 2 levels early at most (AbstractQuestHandler.defaultOnLevelChangedEvent)";
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	QuestEngine::getInstance().onLevelChanged(q->player());
	EXPECT_EQ(statusOf(*q, 2900), QuestStatus::START) << ":272-275";
}

TEST_F(AsmodaeHandPortsTest, DestinyTeleportsAfterTheSteps) {
	// the steps before each teleport of Java (:66-68, :84-86, :129-131, :197-199); the teleports themselves go through TeleportService
	AbstractQuestHandler& handler = install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	Npc& heimdall = spawnNpc(204182);
	Npc& munin = spawnNpc(203550, 103.0f);
	Npc& skuld = spawnNpc(203546, 104.0f);
	Ref<QuestState> qs = hold(*q, 2900, QuestStatus::START, 0);
	EXPECT_TRUE(dialog(*q, heimdall, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(heimdall.getObjectId(), 1011, 2900))) << ":60-63";
	dialog(*q, heimdall, 2900, DA::SETPRO1);
	EXPECT_EQ(varOf(*q, 2900), 1) << ":66 defaultCloseDialog(0, 1)";
	EXPECT_TRUE(dialog(*q, munin, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(munin.getObjectId(), 1352, 2900))) << ":75-76";
	EXPECT_TRUE(dialog(*q, munin, 2900, DA::SETPRO2));
	EXPECT_EQ(varOf(*q, 2900), 2);
	qs->setQuestVar(9);
	dialog(*q, skuld, 2900, DA::SETPRO9);
	EXPECT_EQ(varOf(*q, 2900), 10) << ":129";
	EXPECT_TRUE(dialog(*q, munin, 2900, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(munin.getObjectId(), 4080, 2900))) << ":77-78";
	dialog(*q, munin, 2900, DA::SETPRO10);
	EXPECT_EQ(statusOf(*q, 2900), QuestStatus::REWARD) << ":84 defaultCloseDialog(10, 10, true, false)";
	Quester* killer = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	hold(*killer, 2900, QuestStatus::START, 98);
	Npc& guardian = npcOf(204263);
	try {
		handler.onKillEvent(envFor(*killer, guardian, 2900));
	} catch (const std::exception&) {
		// the teleport of a player standing in the unit fixture's map: the step is taken before it (:197)
	}
	EXPECT_EQ(varOf(*killer, 2900), 9) << ":196-197";
}

// --- the generated paths that kill their target (tests/quest_handlers_golden, knownNotReproducible) --------------------------------------

TEST_F(AsmodaeHandPortsTest, TheCartOfAnOminousCropDiesAtItsUse) {
	// _24012AnOminousCrop.java:61-65: USE_OBJECT at the MuMu Cart (700096) with var 2..4: useQuestObject(var, var + 1, false, true)
	install(altgard::_24012AnOminousCrop_questFactory());
	Quester* q = quester(Race::ASMODIANS, 12);
	hold(*q, 24012, QuestStatus::START, 2);
	Npc& cart = spawnNpc(700096);
	EXPECT_TRUE(dialog(*q, cart, 24012, DA::USE_OBJECT));
	EXPECT_EQ(varOf(*q, 24012), 3);
	EXPECT_TRUE(cart.isDead()) << "useQuestObject's die (AbstractQuestHandler.java useQuestObject)";
	Npc& another = spawnNpc(700096, 104.0f);
	q->player().getQuestStateList()->getQuestState(24012)->setQuestVarById(0, 5);
	EXPECT_FALSE(dialog(*q, another, 24012, DA::USE_OBJECT)) << "var 5: :63 var < 5";
	EXPECT_FALSE(another.isDead());
}

TEST_F(AsmodaeHandPortsTest, TheBonesOfAMythicalMonsterDieWithTheMovie) {
	// _2223AMythicalMonster.java:66-70: USE_OBJECT at var 1 with the item: useQuestObject(1, 1, false, 0, 0, 0, 182203217, 1, 67, true)
	install(altgard::_2223AMythicalMonster_questFactory());
	Quester* q = quester(Race::ASMODIANS, 12);
	hold(*q, 2223, QuestStatus::START, 1);
	holdItem(*q, 840012, 182203217, 1);
	Npc& bones = spawnNpc(700134);
	q->clearSent();
	dialog(*q, bones, 2223, DA::USE_OBJECT);
	EXPECT_EQ(held(*q, 182203217), 0) << "the item removed";
	EXPECT_TRUE(sentTo(*q, playMovie(false, bones.getObjectId(), 2223, 67))) << "movie 67";
	EXPECT_TRUE(bones.isDead());
	EXPECT_EQ(varOf(*q, 2223), 1) << "step 1 -> 1";
}

// --- the review of 2026-09-29 (docs/deviations/Q10.md, "Review"): what the cases above left unobserved ---------------------------------------

TEST_F(AsmodaeHandPortsTest, TheWagerTimerEndsAfterHalfAnHour) {
	// _2230AFriendlyWager.java:22 questDurationTime 1800, :62-64 the new chance: the task fires after 1,800 s, not before; its end
	// (QuestEngine.onQuestTimerEnd) reaches onQuestTimerEndEvent, which cancels the task (:96-99)
	install(altgard::_2230AFriendlyWager_questFactory());
	Quester* q = quester(Race::ASMODIANS, 12);
	Npc& shania = spawnNpc(203621);
	controllers::PlayerController& controller = q->player().getController();
	hold(*q, 2230, QuestStatus::START);
	q->clearSent();
	EXPECT_TRUE(dialog(*q, shania, 2230, DA::SETPRO1));
	EXPECT_TRUE(sentTo(*q, q->serializedFor(network::aion::serverpackets::SM_QUEST_ACTION(2230, 1800))));
	EXPECT_TRUE(sentTo(*q, dialogWindow(shania.getObjectId(), 10, 0))) << ":65 sendQuestSelectionDialog";
	executor->advance(std::chrono::seconds(1799));
	EXPECT_TRUE(controller.hasTask(TaskId::QUEST_TIMER));
	executor->advance(std::chrono::seconds(1));
	EXPECT_FALSE(controller.hasTask(TaskId::QUEST_TIMER)) << "the timer's end";
	EXPECT_EQ(statusOf(*q, 2230), QuestStatus::START) << "only the task goes";
}

TEST_F(AsmodaeHandPortsTest, TheBonesCallUpTheSpiritNinetyFiveTimesInAHundredAndTheDrakieElse) {
	// _2252ChasingtheLegend.java:84 `Rnd.chance() < chance ? questKillNpc1Id : questKillNpc2Id` (95): a seed whose first draw is below 95 calls
	// up the spirit (210634), one at 95 or above the drakie (210635); the legend shouts 1100630 to the players within 50 m 500 ms later (:87)
	auto seedWhere = [](bool high) {
		for (uint64_t seed = 1; seed < 100000; seed++) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			if ((commons::utils::Rnd::chance() >= 95.0f) == high)
				return seed;
		}
		return uint64_t{0};
	};
	install(altgard::_2252ChasingtheLegend_questFactory());
	const std::pair<bool, int32_t> rolls[] = {{false, 210634}, {true, 210635}};
	float x = 106.0f;
	for (const auto& [high, npcId] : rolls) {
		SCOPED_TRACE(npcId);
		uint64_t seed = seedWhere(high);
		ASSERT_NE(seed, 0u);
		Quester* q = quester(Race::ASMODIANS, 14);
		// the quester in the World's Poeta instance, where the npcs spawn (the fixture's questers stand in a map instance of their own), and in
		// its region's objects, as a spawn puts it: the handler's "a legend is up" reads the quester's instance, and the legend's known list update
		// finds the quester 6 m away. Restored at the end of the roll whatever the checks say
		runtime::Ptr<world::MapRegion> region = poeta().getRegion(100.0f, 100.0f, 50.0f);
		Ref<world::WorldPosition> fixturePosition(*q->player().getPosition());
		q->player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, region));
		q->player().getPosition()->setIsSpawned(true);
		region->add(q->player());
		struct Restore {
			world::MapRegion& region;
			Player& player;
			Ref<world::WorldPosition> position;
			~Restore() {
				region.remove(player);
				player.setPosition(position);
			}
		} restore{*region, q->player(), fixturePosition};
		hold(*q, 2252, QuestStatus::START);
		holdItem(*q, high ? 840021 : 840020, 182203235, 1);
		Npc& bones = spawnNpc(700060, x, 101.0f, 50.0f, int8_t{25});
		x += 2.0f;
		q->clearSent();
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		EXPECT_FALSE(dialog(*q, bones, 2252, DA::USE_OBJECT));
		std::vector<Ptr<Npc>> legends = spawnedOf(npcId);
		ASSERT_EQ(legends.size(), 1u);
		EXPECT_TRUE(spawnedOf(high ? 210634 : 210635).empty());
		Npc& legend = *legends.front();
		EXPECT_FLOAT_EQ(legend.getZ(), 50.0f);
		legend.getKnownList().update();
		std::vector<uint8_t> shout = q->serializedFor(network::aion::serverpackets::SM_SYSTEM_MESSAGE(gameserver::model::ChatType::NPC,
			Ptr<VisibleObject>(legend), 1100630, std::vector<std::string>()));
		executor->advance(std::chrono::milliseconds(499));
		EXPECT_FALSE(sentTo(*q, shout));
		executor->advance(std::chrono::milliseconds(1));
		EXPECT_TRUE(sentTo(*q, shout)) << ":87 broadcastMessage(questMob, 1100630, 500)";
		// a legend is up in the quester's instance (:78-79): false before the item is looked at, even with new bones
		holdItem(*q, high ? 840023 : 840022, 182203235, 1);
		EXPECT_FALSE(dialog(*q, bones, 2252, DA::USE_OBJECT));
		EXPECT_EQ(held(*q, 182203235), 1) << "the bones stay";
		EXPECT_EQ(spawnedOf(210634).size() + spawnedOf(210635).size(), 1u);
		executor->advance(std::chrono::minutes(3)); // the legend leaves (:83): the next roll may call one up
		EXPECT_FALSE(legend.isSpawned());
	}
}

TEST_F(AsmodaeHandPortsTest, DestinyGivesTheStoneOfEveryDaevaClass) {
	// _2900NoEscapingDestiny.java:151-154 and getStoneId :240-261: SELECT7_1 at var 96 gives the class's stone and steps to 99
	install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Npc& skuld2 = spawnNpc(204264);
	const std::pair<PlayerClass, int32_t> stones[] = {{PlayerClass::CHANTER, 140000001}, {PlayerClass::CLERIC, 140000001},
		{PlayerClass::BARD, 140000001}, {PlayerClass::RIDER, 140000002}, {PlayerClass::GUNNER, 140000002}, {PlayerClass::RANGER, 140000002},
		{PlayerClass::GLADIATOR, 140000003}, {PlayerClass::ASSASSIN, 140000003}, {PlayerClass::TEMPLAR, 140000003},
		{PlayerClass::SORCERER, 140000004}, {PlayerClass::SPIRIT_MASTER, 140000004}};
	for (const auto& [playerClass, stone] : stones) {
		SCOPED_TRACE(std::string(xml::enumName(playerClass)));
		Quester* q = quester(Race::ASMODIANS, 20, playerClass);
		hold(*q, 2900, QuestStatus::START, 96);
		EXPECT_TRUE(dialog(*q, skuld2, 2900, DA::SELECT7_1));
		for (int32_t other : {140000001, 140000002, 140000003, 140000004})
			EXPECT_EQ(held(*q, other), other == stone ? 1 : 0) << other;
		EXPECT_EQ(varOf(*q, 2900), 99);
	}
}

TEST_F(AsmodaeHandPortsTest, DestinySpaceOfDestinyStartsAtSkuldsFifthStep) {
	// _2900NoEscapingDestiny.java:120-127: SETPRO5 at var 4 steps to 95 first, then asks for a new Space of Destiny instance (320070000) and
	// teleports; the unit world holds no such map, so the instance step throws after the step (QuestEngine.onDialog's catch). At var 3: false
	install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	Npc& skuld = spawnNpc(203546);
	hold(*q, 2900, QuestStatus::START, 3);
	EXPECT_FALSE(dialog(*q, skuld, 2900, DA::SETPRO5)) << ":127 var 3";
	EXPECT_EQ(varOf(*q, 2900), 3);
	q->player().getQuestStateList()->getQuestState(2900)->setQuestVar(4);
	q->clearSent();
	dialog(*q, skuld, 2900, DA::SETPRO5);
	EXPECT_EQ(varOf(*q, 2900), 95) << ":122 changeQuestStep(env, 4, 95)";
	EXPECT_TRUE(sentTo(*q, questUpdate(2900, START, 95)));
}

TEST_F(AsmodaeHandPortsTest, DestinyDeathAndReturnBoundsAndTheSpaceOfDestiny) {
	// onDieEvent :211 and onEnterWorldEvent :226-227: 95..99 both ends, and inside the Space of Destiny (320070000) the enter world does nothing
	AbstractQuestHandler& handler = install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* q = quester(Race::ASMODIANS, 20, PlayerClass::GLADIATOR);
	Ref<QuestState> qs = hold(*q, 2900, QuestStatus::START, 99);
	holdItem(*q, 840030, 140000003, 1);
	EXPECT_TRUE(handler.onDieEvent(envFor(*q, nullptr, 2900))) << "var 99";
	EXPECT_EQ(varOf(*q, 2900), 4);
	EXPECT_EQ(held(*q, 140000003), 0);
	qs->setQuestVar(94);
	EXPECT_FALSE(handler.onDieEvent(envFor(*q, nullptr, 2900))) << "var 94";
	qs->setQuestVar(95);
	holdItem(*q, 840031, 140000003, 1);
	EXPECT_TRUE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900))) << "var 95 outside the Space of Destiny";
	EXPECT_EQ(varOf(*q, 2900), 4);
	EXPECT_EQ(held(*q, 140000003), 0);
	qs->setQuestVar(94);
	EXPECT_FALSE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900))) << "var 94";
	qs->setQuestVar(10);
	EXPECT_FALSE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900))) << "var 10";

	Ref<world::WorldPosition> poetaPosition = onMap(*q, 320070000);
	ASSERT_EQ(q->player().getWorldId(), 320070000);
	for (int32_t var : {95, 97, 99, 9}) {
		qs->setQuestVar(var);
		holdItem(*q, 840032 + var, 140000003, 1);
		EXPECT_FALSE(handler.onEnterWorldEvent(envFor(*q, nullptr, 2900))) << "inside, var " << var;
		EXPECT_EQ(varOf(*q, 2900), var);
		EXPECT_EQ(held(*q, 140000003), 1) << "the stone stays";
		q->player().getInventory().decreaseByItemId(140000003, 1);
	}
	q->player().setPosition(poetaPosition);
}

// (removeStigma's unequip loop, :266-268, has no case: Equipment.unEquipItem of a stigma reaches StigmaService.removeStigmaSkills, and equipping
// one StigmaService.getPossibleStigmaCount and addStigmaSkills, all three AION_UNPORTED; docs/deviations/Q10.md, "Landed with an unported
// engine body on the path")

TEST_F(AsmodaeHandPortsTest, DestinyIsLockedTwoLevelsBeforeTheMissionsLevel) {
	// quest_data.xml: 2900 minlevel_permitted 20, a MISSION; AbstractQuestHandler.defaultOnLevelChangedEvent (AbstractQuestHandler.java:988-1030)
	// checks the start conditions 2 levels early and adds a mission LOCKED below its level: level 18 LOCKED, 20 START (DestinyStartsAt...)
	install(pandaemonium::_2900NoEscapingDestiny_questFactory());
	Quester* q = quester(Race::ASMODIANS, 18, PlayerClass::GLADIATOR);
	QuestEngine::getInstance().onLevelChanged(q->player());
	EXPECT_EQ(statusOf(*q, 2900), QuestStatus::LOCKED) << "level 18";
}

// --- generated handlers whose hooks the golden harness leaves unobserved (the oracle refuses them, or their effect is invisible there) -------

TEST_F(AsmodaeHandPortsTest, SuthransOrdersStartAtTheEnterWorldInAltgard) {
	// _24010SuthransOrders.java:36-41: at an enter world in Altgard (220030000) without the quest, startQuest; the hook gs.scenario.travel's T3
	// reaches (docs/deviations/Q10.md, "Gate impact")
	install(altgard::_24010SuthransOrders_questFactory());
	Quester* q = quester(Race::ASMODIANS, 10);
	QuestEngine::getInstance().onEnterWorld(q->player());
	EXPECT_EQ(statusOf(*q, 24010), std::nullopt) << "in Poeta: nothing";
	Ref<world::WorldPosition> poetaPosition = onMap(*q, 220030000);
	q->clearSent();
	QuestEngine::getInstance().onEnterWorld(q->player());
	EXPECT_EQ(statusOf(*q, 24010), QuestStatus::START) << "in Altgard: started";
	q->clearSent();
	QuestEngine::getInstance().onEnterWorld(q->player());
	EXPECT_TRUE(q->sent().empty()) << "hasQuest: once only";
	Quester* done = quester(Race::ASMODIANS, 10);
	hold(*done, 24010, QuestStatus::COMPLETE);
	Ref<world::WorldPosition> donePosition = onMap(*done, 220030000);
	QuestEngine::getInstance().onEnterWorld(done->player());
	EXPECT_EQ(statusOf(*done, 24010), QuestStatus::COMPLETE);
	done->player().setPosition(donePosition);
	q->player().setPosition(poetaPosition);
}

TEST_F(AsmodaeHandPortsTest, AStrangeNewThreatGoesBackAStepAtADeath) {
	// _24016AStrangeNewThread.java onDieEvent: START with var >= 2: var 1 and the update; below 2 nothing
	AbstractQuestHandler& handler = install(altgard::_24016AStrangeNewThread_questFactory());
	Quester* q = quester(Race::ASMODIANS, 20);
	Ref<QuestState> qs = hold(*q, 24016, QuestStatus::START, 1);
	EXPECT_FALSE(handler.onDieEvent(envFor(*q, nullptr, 24016))) << "var 1";
	EXPECT_EQ(varOf(*q, 24016), 1);
	for (int32_t var : {2, 5}) {
		qs->setQuestVar(var);
		q->clearSent();
		EXPECT_TRUE(handler.onDieEvent(envFor(*q, nullptr, 24016))) << var;
		EXPECT_EQ(varOf(*q, 24016), 1) << var;
		EXPECT_TRUE(sentTo(*q, questUpdate(24016, START, 1))) << var;
	}
	qs->setStatus(QuestStatus::REWARD);
	qs->setQuestVar(3);
	EXPECT_FALSE(handler.onDieEvent(envFor(*q, nullptr, 24016))) << "not START";
}

TEST_F(AsmodaeHandPortsTest, AHeartfeltConfessionSteps) {
	// _2925AHeartfeltConfession.java (every hook refused by the oracle, getEquipment): the start at 204261 and the five steps
	install(pandaemonium::_2925AHeartfeltConfession_questFactory());
	Quester* q = quester(Race::ASMODIANS, 25);
	hold(*q, 2923, QuestStatus::COMPLETE); // quest_data.xml: 2925's <finished quest_id="2923"/>
	Npc& npc204261 = spawnNpc(204261);
	Npc& npc204235 = spawnNpc(204235, 103.0f);
	Npc& npc204127 = spawnNpc(204127, 104.0f);
	Npc& npc204193 = spawnNpc(204193, 105.0f);
	EXPECT_TRUE(dialog(*q, npc204261, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204261.getObjectId(), 4762, 2925)));
	EXPECT_TRUE(dialog(*q, npc204261, 2925, DA::QUEST_ACCEPT_1));
	EXPECT_EQ(statusOf(*q, 2925), QuestStatus::START);
	// var 0 at 204235: page 1097 without the equipped 110100288, 1011 with it
	EXPECT_TRUE(dialog(*q, npc204235, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204235.getObjectId(), 1097, 2925)));
	EXPECT_TRUE(dialog(*q, npc204235, 2925, DA::SETPRO1));
	EXPECT_EQ(varOf(*q, 2925), 1) << "defaultCloseDialog(env, 0, 1)";
	EXPECT_TRUE(dialog(*q, npc204261, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204261.getObjectId(), 1352, 2925)));
	holdItem(*q, 840050, 110100288, 1);
	EXPECT_TRUE(dialog(*q, npc204261, 2925, DA::SETPRO2));
	EXPECT_EQ(held(*q, 110100288), 0) << "removeQuestItem(env, 110100288, 1)";
	EXPECT_EQ(varOf(*q, 2925), 2) << "defaultCloseDialog(env, 1, 2)";
	EXPECT_TRUE(dialog(*q, npc204127, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204127.getObjectId(), 1693, 2925)));
	EXPECT_TRUE(dialog(*q, npc204127, 2925, DA::SETPRO3));
	EXPECT_EQ(varOf(*q, 2925), 3) << "defaultCloseDialog(env, 2, 3)";
	EXPECT_TRUE(dialog(*q, npc204193, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204193.getObjectId(), 2034, 2925)));
	EXPECT_TRUE(dialog(*q, npc204193, 2925, DA::SETPRO4));
	EXPECT_EQ(varOf(*q, 2925), 4) << "defaultCloseDialog(env, 3, 4)";
	EXPECT_TRUE(dialog(*q, npc204235, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204235.getObjectId(), 2375, 2925)));
	EXPECT_TRUE(dialog(*q, npc204235, 2925, DA::SETPRO5));
	EXPECT_EQ(varOf(*q, 2925), 5) << "defaultCloseDialog(env, 4, 5)";
	EXPECT_TRUE(dialog(*q, npc204261, 2925, DA::QUEST_SELECT));
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204261.getObjectId(), 2716, 2925)));
	EXPECT_TRUE(dialog(*q, npc204261, 2925, DA::SELECT6_1));
	EXPECT_EQ(statusOf(*q, 2925), QuestStatus::REWARD) << "changeQuestStep(env, 5, 5, true)";
	EXPECT_TRUE(sentTo(*q, dialogWindow(npc204261.getObjectId(), 10002, 2925)));
}

TEST_F(AsmodaeHandPortsTest, ShugoPotionLeavesWithItsPollenAtTheTimersEndAndTheLogout) {
	// _2263ShugoPotion.java onQuestTimerEndEvent and onLogOutEvent: in START every Malodor Pollen (182203242) removed and the quest abandoned;
	// the golden trace holds no pollen there (given inventory 0), so removeQuestItem(..., 0) did nothing in both runs
	AbstractQuestHandler& handler = install(altgard::_2263ShugoPotion_questFactory());
	for (bool logout : {false, true}) {
		SCOPED_TRACE(logout ? "logout" : "timer end");
		Quester* q = quester(Race::ASMODIANS, 14);
		hold(*q, 2263, QuestStatus::START);
		holdItem(*q, logout ? 840061 : 840060, 182203242, 2);
		EXPECT_TRUE(logout ? handler.onLogOutEvent(envFor(*q, nullptr, 2263)) : handler.onQuestTimerEndEvent(envFor(*q, nullptr, 2263)));
		EXPECT_EQ(held(*q, 182203242), 0);
		EXPECT_EQ(statusOf(*q, 2263), std::nullopt) << "abandonQuest: never completed, deleted";
		Quester* rewarded = quester(Race::ASMODIANS, 14);
		hold(*rewarded, 2263, QuestStatus::REWARD);
		holdItem(*rewarded, logout ? 840063 : 840062, 182203242, 3);
		EXPECT_FALSE(logout ? handler.onLogOutEvent(envFor(*rewarded, nullptr, 2263))
												: handler.onQuestTimerEndEvent(envFor(*rewarded, nullptr, 2263)));
		EXPECT_EQ(held(*rewarded, 182203242), 3);
	}
	// the start with the five-minute timer (:43-46)
	Quester* q = quester(Race::ASMODIANS, 14);
	Npc& mabrunerk = spawnNpc(798036);
	EXPECT_TRUE(dialog(*q, mabrunerk, 2263, DA::QUEST_ACCEPT_1));
	EXPECT_TRUE(sentTo(*q, q->serializedFor(network::aion::serverpackets::SM_QUEST_ACTION(2263, 300))));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::asmodae
