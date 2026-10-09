// M5j stage 3 CP1 (m5j-plan.md §18.4, item N-01): the small root AIs - AggressiveNoLootNpcAI, BookAI, BubblegutAI, FirecrackerAI,
// NeutralGuardAI, NoDmgNoActionAI, NoInteractionAI, OneDmgAI, OneDmgNoActionAI, ShifterAI, AggressiveBossSummonNpcAI, UseSkillAndDieAI,
// BombAI, PlatinumFountainAI, SpeakerAI (P5-05). Npcs of one template row (npc_templates.xml's 798100, its ai attribute aside: the test
// installs the AI under test) beside the item fixture's player in a Poeta map instance.
//
// Java: data/handlers/ai/*.java of the classes above.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/AIData.bind.h"
#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/handlers/ai/AggressiveBossSummonNpcAI.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/handlers/ai/BombAI.h"
#include "aion/gameserver/handlers/ai/BookAI.h"
#include "aion/gameserver/handlers/ai/BubblegutAI.h"
#include "aion/gameserver/handlers/ai/FirecrackerAI.h"
#include "aion/gameserver/handlers/ai/NeutralGuardAI.h"
#include "aion/gameserver/handlers/ai/NoDmgNoActionAI.h"
#include "aion/gameserver/handlers/ai/NoInteractionAI.h"
#include "aion/gameserver/handlers/ai/OneDmgAI.h"
#include "aion/gameserver/handlers/ai/OneDmgNoActionAI.h"
#include "aion/gameserver/handlers/ai/PlatinumFountainAI.h"
#include "aion/gameserver/handlers/ai/ShifterAI.h"
#include "aion/gameserver/handlers/ai/SpeakerAI.h"
#include "aion/gameserver/handlers/ai/UseSkillAndDieAI.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;

using gameserver::ai::AIState;
using gameserver::ai::poll::AIQuestion;
using model::CreatureType;
using model::gameobjects::Npc;
using model::stats::container::StatEnum;

constexpr int32_t NPC = 798100;

/** npc_templates.xml:462252-462258 (the template the cases share; the AI under test is installed by the test) */
constexpr const char* NPC_XML =
	R"(<npc_templates><npc_template npc_id="798100" level="15" name="zephyr deliveryman" name_id="350579" height="1.16875" group_drop="NONE" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="deliveryman" srange="20" sangle="240" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" )"
	R"(group_run_fight="4.23" /></stats><bound_radius front="0.595" side="0.3774" upper="1.16875" /><talk_info distance="5" )"
	R"(can_talk_invisible="false" /></npc_template></npc_templates>)";

/** item_templates.xml: the gold, platinum and rusted medals of PlatinumFountainAI (their <inventory id="1"/>, the special cube, left out) */
constexpr std::string_view MEDAL_ROWS = R"xml(
	<item_template id="186000030" name="Gold Medal" level="50" cName="medal_01" mask="12414" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="5000" desc="739361"/>
	<item_template id="186000096" name="Platinum Medal" level="60" cName="medal_03" mask="12364" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="15000" desc="752849"/>
	<item_template id="182005205" name="Rusted Medal" level="40" cName="junk_bronze_medal_01" mask="12414" max_stack_count="1000" quality="JUNK" price="500" desc="741640"/>
</item_templates>)xml";

constexpr int32_t GOLD_MEDAL = 186000030;
constexpr int32_t PLATINUM_MEDAL = 186000096;
constexpr int32_t RUSTED_MEDAL = 182005205;

class NpcSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	NpcSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

// the probes: each AI with its protected hooks callable
struct BookProbe final : roots::BookAI {
	using BookAI::BookAI;
	using BookAI::handleDialogStart;
};
struct BubblegutProbe final : roots::BubblegutAI {
	using BubblegutAI::BubblegutAI;
	using BubblegutAI::handleSpawned;
};
struct NeutralGuardProbe final : roots::NeutralGuardAI {
	using NeutralGuardAI::NeutralGuardAI;
	using NeutralGuardAI::handleBackHome;
};
struct NoInteractionProbe final : roots::NoInteractionAI {
	using NoInteractionAI::NoInteractionAI;
	using NoInteractionAI::handleBeforeSpawned;
};
struct ShifterProbe final : roots::ShifterAI {
	using ShifterAI::ShifterAI;
	using ShifterAI::handleUseItemFinish;
};
struct BossSummonProbe final : roots::AggressiveBossSummonNpcAI {
	using AggressiveBossSummonNpcAI::AggressiveBossSummonNpcAI;
	using AggressiveBossSummonNpcAI::handleDied;
};
struct BombProbe final : roots::BombAI {
	using BombAI::BombAI;
	using BombAI::handleSpawned;
	using BombAI::handleDied;
};
struct FountainProbe final : roots::PlatinumFountainAI {
	using PlatinumFountainAI::PlatinumFountainAI;
	using PlatinumFountainAI::handleDialogStart;
	using PlatinumFountainAI::handleUseItemFinish;
};

class StageThreeAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // the empty AI registry of this executable: the test installs the AI
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_XML));
		// Npc.getType's relation-based arm reads the tribe relations (NeutralGuardAI.creatureNeedsHelp)
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, gameserver::ai::testing::AI_TRIBE_RELATIONS_XML));
		// the fixture's item rows and the three medals
		std::string itemsXml(ITEM_TEMPLATES_XML);
		itemsXml.replace(itemsXml.rfind("</item_templates>"), std::string_view("</item_templates>").size(), MEDAL_ROWS);
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext, itemsXml));
		player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		world::World::getInstance().storeObject(player());
		clearSent();
	}

	void TearDown() override {
		for (const runtime::Ref<Npc>& npc : npcs)
			npc->getController().cancelAllTasks();
		player().setTarget(nullptr);
		mapInstance->removeObject(player());
		world::World::getInstance().removeObject(player());
		for (const runtime::Ref<Npc>& npc : npcs)
			world::World::getInstance().removeObject(*npc);
		npcs.clear();
		groups.clear();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** an npc of the shared template at (x, 100, 50) with the AI `A` installed and idle */
	template <class A>
	Npc& npc(float x = 102.0f, int32_t npcId = NPC) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<NpcSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> created = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		created->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*created));
		created->setEffectController(std::make_unique<controllers::effect::EffectController>(*created));
		created->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		created->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<A>(*created);
		ai->setStateIfNot(AIState::IDLE);
		created->replaceAi(std::move(ai));
		groups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	template <class A>
	static A& aiOf(Npc& npc) {
		return dynamic_cast<A&>(npc.getAi());
	}

	std::vector<uint8_t> message(std::string_view text) {
		return serializedFor(serverpackets::SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW));
	}

	size_t count(const std::vector<uint8_t>& packet) {
		size_t n = 0;
		for (const std::vector<uint8_t>& bytes : sent())
			n += bytes == packet ? 1 : 0;
		return n;
	}

	xml::LoadContext context;
	xml::LoadContext itemContext;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups;
	std::vector<runtime::Ref<Npc>> npcs;
};

/** AggressiveNoLootNpcAI.java:20-25, BombAI.java:48-54: the questions they answer false, the rest their base's */
TEST_F(StageThreeAiTest, NoLootAndBombQuestions) {
	Npc& noLoot = npc<roots::AggressiveNoLootNpcAI>();
	EXPECT_FALSE(noLoot.getAi().ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(noLoot.getAi().ask(AIQuestion::ALLOW_DECAY));
	Npc& plain = npc<roots::AggressiveNpcAI>(103.0f);
	EXPECT_EQ(noLoot.getAi().ask(AIQuestion::REWARD_AP_XP_DP_LOOT), plain.getAi().ask(AIQuestion::REWARD_AP_XP_DP_LOOT)) << "AggressiveNpcAI's";
	EXPECT_EQ(noLoot.getAi().ask(AIQuestion::ALLOW_RESPAWN), plain.getAi().ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_TRUE(plain.getAi().ask(AIQuestion::REWARD_LOOT)) << "the base rewards loot";

	Npc& bomb = npc<BombProbe>(104.0f);
	EXPECT_FALSE(bomb.getAi().ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(bomb.getAi().ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_FALSE(bomb.getAi().ask(AIQuestion::REWARD_LOOT));
	EXPECT_EQ(bomb.getAi().ask(AIQuestion::ALLOW_RESPAWN), plain.getAi().ask(AIQuestion::ALLOW_RESPAWN));
	// BombAI.java:32: AI_DATA.getAiTemplate(npcId).getBombs() of an npc without an ai template is Java's NullPointerException
	xml::LoadContext aiContext;
	dataholders::DataManager::AI_DATA.publish(xml::bindString<dataholders::AIData>(aiContext,
		R"(<ai_templates><ai npcId="281327"><bombs><bomb skillId="16559"/></bombs></ai></ai_templates>)")); // bombs.xml:3-7
	try {
		aiOf<BombProbe>(bomb).handleSpawned();
		ADD_FAILURE() << "no exception";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_NE(std::string(e.what()).find("has no bombs"), std::string::npos) << e.what();
	}
	dataholders::DataManager::AI_DATA.resetForTests();
}

/** BombAI.java:29-46, :73-78: the bomb skill cd + 2 s after the spawn, then the deletion (the skill's duration is 0); a death cancels both */
TEST_F(StageThreeAiTest, BombGoesOffAndLeaves) {
	xml::LoadContext aiContext;
	// the fixture's skill 10034 (no duration) as the bomb skill of the shared template, with a cd of 1 s
	dataholders::DataManager::AI_DATA.publish(xml::bindString<dataholders::AIData>(aiContext,
		R"(<ai_templates><ai npcId="798100"><bombs><bomb skillId="10034" cd="1000"/></bombs></ai></ai_templates>)"));
	Npc& bomb = npc<BombProbe>();
	world::World::getInstance().storeObject(bomb); // AIActions.deleteOwner is World.removeObject
	aiOf<BombProbe>(bomb).handleSpawned();
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_TRUE(world::World::getInstance().isInWorld(bomb.getObjectId())) << "cd + 2000 not yet reached";
	executor->advance(std::chrono::milliseconds(1));
	executor->runReady();
	EXPECT_FALSE(world::World::getInstance().isInWorld(bomb.getObjectId())) << "the skill, then the deletion after 0 ms";

	Npc& dud = npc<BombProbe>(104.0f);
	world::World::getInstance().storeObject(dud);
	aiOf<BombProbe>(dud).handleSpawned();
	aiOf<BombProbe>(dud).handleDied();
	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_TRUE(world::World::getInstance().isInWorld(dud.getObjectId())) << "handleDied cancelled the tasks";
	dataholders::DataManager::AI_DATA.resetForTests();
}

/** BookAI.java:21-24: the dialog is page 1011 */
TEST_F(StageThreeAiTest, BookOpensPage1011) {
	Npc& book = npc<BookProbe>();
	aiOf<BookProbe>(book).handleDialogStart(player());
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_DIALOG_WINDOW(book.getObjectId(), 1011))}));
}

/** BubblegutAI.java:18-22: the spawn casts skill 16447 (NpcController.useSkill renews the last skill time first) */
TEST_F(StageThreeAiTest, BubblegutCastsOnSpawn) {
	Npc& bubblegut = npc<BubblegutProbe>();
	ASSERT_EQ(bubblegut.getGameStats()->getLastSkillTime(), 0);
	aiOf<BubblegutProbe>(bubblegut).handleSpawned();
	EXPECT_NE(bubblegut.getGameStats()->getLastSkillTime(), 0) << "AIActions.useSkill(this, 16447)";
	Npc& firecracker = npc<roots::FirecrackerAI>(103.0f);
	EXPECT_NE(dynamic_cast<roots::GeneralNpcAI*>(&firecracker.getAi()), nullptr) << "FirecrackerAI is a GeneralNpcAI";
}

/**
 * NeutralGuardAI.java:22-35: an attacker near the guard that targets a player makes it aggressive (the attacker's hate then needs the guard's
 * AggroList to be aware of it: a known enemy or hostile tribe, AggroList.isAware - which the same-tribe attacker here is not, so the hate
 * itself is the gate's and the AI smoke's); home, it supports
 */
TEST_F(StageThreeAiTest, NeutralGuardTurnsOnAnAttackerOfPlayers) {
	Npc& guard = npc<NeutralGuardProbe>();
	Npc& attacker = npc<roots::AggressiveNpcAI>(110.0f);
	Npc& far = npc<roots::AggressiveNpcAI>(130.0f);
	const CreatureType before = guard.getType(player());
	ASSERT_NE(before, CreatureType::AGGRESSIVE);
	aiOf<NeutralGuardProbe>(guard).creatureNeedsHelp(attacker);
	EXPECT_EQ(guard.getType(player()), before) << "the attacker targets nobody";
	attacker.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	far.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	aiOf<NeutralGuardProbe>(guard).creatureNeedsHelp(far);
	EXPECT_EQ(guard.getType(player()), before) << "28 m away: not in range 20";
	aiOf<NeutralGuardProbe>(guard).creatureNeedsHelp(attacker);
	EXPECT_EQ(guard.getType(player()), CreatureType::AGGRESSIVE) << "overrideNpcType(AGGRESSIVE)";
	aiOf<NeutralGuardProbe>(guard).creatureNeedsHelp(attacker);
	EXPECT_EQ(guard.getType(player()), CreatureType::AGGRESSIVE) << "already aggressive towards it: unchanged";
	aiOf<NeutralGuardProbe>(guard).handleBackHome();
	EXPECT_EQ(guard.getType(player()), CreatureType::SUPPORT);
	attacker.setTarget(nullptr);
	far.setTarget(nullptr);
}

/** NoDmgNoActionAI.java, NoInteractionAI.java, OneDmgAI.java, OneDmgNoActionAI.java: damage, thinking, type and stats */
TEST_F(StageThreeAiTest, DamageAndStatModifiers) {
	Npc& noDmg = npc<roots::NoDmgNoActionAI>();
	EXPECT_EQ(noDmg.getAi().modifyDamage(player(), 500.0f, nullptr), 0.0f);
	Npc& noInteraction = npc<NoInteractionProbe>(103.0f);
	EXPECT_EQ(noInteraction.getAi().modifyDamage(player(), 500.0f, nullptr), 0.0f);
	EXPECT_FALSE(noInteraction.getAi().canThink());
	aiOf<NoInteractionProbe>(noInteraction).handleBeforeSpawned();
	EXPECT_EQ(noInteraction.getType(player()), CreatureType::PEACE);

	Npc& oneDmg = npc<roots::OneDmgAI>(104.0f);
	EXPECT_EQ(oneDmg.getAi().modifyDamage(player(), 500.0f, nullptr), 1.0f);
	EXPECT_EQ(oneDmg.getAi().modifyOwnerDamage(500.0f, player(), nullptr), 1.0f);
	Npc& oneDmgPassive = npc<roots::OneDmgNoActionAI>(105.0f);
	EXPECT_EQ(oneDmgPassive.getAi().modifyDamage(player(), 500.0f, nullptr), 1.0f);
	EXPECT_EQ(oneDmgPassive.getAi().modifyOwnerDamage(500.0f, player(), nullptr), 500.0f) << "AbstractAI's: the damage";
	for (Npc* owner : {&oneDmg, &oneDmgPassive}) {
		for (StatEnum stat : {StatEnum::EVASION, StatEnum::MAGICAL_RESIST}) {
			model::stats::calc::AdditionStat value(stat, 300.0f, *owner);
			value.addToBonus(20.0f);
			owner->getAi().modifyOwnerStat(value);
			EXPECT_EQ(value.getBase(), 0);
			EXPECT_EQ(value.getBonus(), 0);
		}
		model::stats::calc::AdditionStat parry(StatEnum::PARRY, 300.0f, *owner);
		owner->getAi().modifyOwnerStat(parry);
		EXPECT_EQ(parry.getBase(), 300) << "only evasion and magical resist";
	}
}

/** ShifterAI.java:20-24: the use ends with the shifter's emote 144 to the user's watchers and the user */
TEST_F(StageThreeAiTest, ShifterEmotesAfterTheUse) {
	Npc& shifter = npc<ShifterProbe>();
	aiOf<ShifterProbe>(shifter).handleUseItemFinish(player());
	EXPECT_EQ(count(serializedFor(serverpackets::SM_EMOTION(shifter, model::EmotionType::EMOTE, 144, 0))), 1u);
}

/** AggressiveBossSummonNpcAI.java:20-41: without a fighting creator the summon leaves after an attack; it leaves when it died or the fight ended */
TEST_F(StageThreeAiTest, BossSummonLeaves) {
	for (int i = 0; i < 3; i++) {
		Npc& summon = npc<BossSummonProbe>(102.0f + i);
		world::World::getInstance().storeObject(summon); // delete() is World.removeObject
		ASSERT_TRUE(world::World::getInstance().isInWorld(summon.getObjectId()));
		if (i == 0)
			aiOf<BossSummonProbe>(summon).handleAttackComplete();
		else if (i == 1)
			aiOf<BossSummonProbe>(summon).handleFinishAttack();
		else
			aiOf<BossSummonProbe>(summon).handleDied();
		EXPECT_FALSE(world::World::getInstance().isInWorld(summon.getObjectId())) << "case " << i << ": getController().delete()";
	}
}

/** UseSkillAndDieAI.java:32-37, :55-58: an npc without npc skills warns and leaves; it takes damage while it can die */
TEST_F(StageThreeAiTest, UseSkillAndDieWithoutSkills) {
	Npc& npcWithoutSkills = npc<roots::UseSkillAndDieAI>();
	world::World::getInstance().storeObject(npcWithoutSkills); // delete() is World.removeObject
	EXPECT_EQ(npcWithoutSkills.getAi().modifyDamage(player(), 40.0f, nullptr), 40.0f) << "canDie starts true";
	network::test::LogCapture capture({"ai.UseSkillAndDieAI"});
	aiOf<roots::UseSkillAndDieAI>(npcWithoutSkills).handleSpawned();
	EXPECT_EQ(capture.count("has no skill list"), 1) << capture.dump();
	EXPECT_FALSE(world::World::getInstance().isInWorld(npcWithoutSkills.getObjectId()));
}

/** PlatinumFountainAI.java:22-42: no gold medal, no use; each throw takes one medal and gives a platinum (10 %) or a rusted one */
TEST_F(StageThreeAiTest, PlatinumFountain) {
	Npc& fountain = npc<FountainProbe>();
	aiOf<FountainProbe>(fountain).handleDialogStart(player());
	EXPECT_EQ(count(message("Du hast leider keine Goldmedaillen bei dir, die du in den Brunnen werfen könntest.")), 1u);
	stored(9901, GOLD_MEDAL, 40);
	clearSent();
	aiOf<FountainProbe>(fountain).handleDialogStart(player());
	EXPECT_EQ(count(message("Du forderst dein Glück heraus und wirfst eine Goldmedaille in den Brunnen!")), 1u);
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	for (int i = 0; i < 40; i++)
		aiOf<FountainProbe>(fountain).handleUseItemFinish(player());
	const int64_t platinum = player().getInventory().getItemCountByItemId(PLATINUM_MEDAL);
	const int64_t rusted = player().getInventory().getItemCountByItemId(RUSTED_MEDAL);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(GOLD_MEDAL), 0);
	EXPECT_EQ(platinum + rusted, 40);
	EXPECT_GT(platinum, 0) << "Rnd.chance() < 10 in 40 throws";
	EXPECT_LT(platinum, 15);
	EXPECT_EQ(count(message("Du hattest Glück! Eine Medaille aus reinem Platin springt dir entgegen!")), static_cast<size_t>(platinum));
	aiOf<FountainProbe>(fountain).handleUseItemFinish(player());
	EXPECT_EQ(platinum + rusted, player().getInventory().getItemCountByItemId(PLATINUM_MEDAL) + player().getInventory().getItemCountByItemId(RUSTED_MEDAL))
		<< "no medal: nothing";
}

/** SpeakerAI.java:20-28: only on Inggison and Gelkmaros, only the IDLE pattern shouts, never without a pattern */
TEST_F(StageThreeAiTest, SpeakerOnlyOnItsMaps) {
	using model::templates::npcshout::ShoutEventType;
	Npc& speaker = npc<roots::SpeakerAI>();
	EXPECT_FALSE(speaker.getAi().onPatternShout(ShoutEventType::IDLE, "5", 0)) << "Poeta";
	// on Inggison (no map instance: only getWorldId is read before the siege service)
	speaker.setPosition(world::WorldPosition::create(210050000, 100.0f, 100.0f, 50.0f, int8_t{0}, nullptr));
	EXPECT_FALSE(speaker.getAi().onPatternShout(ShoutEventType::ATTACKED, "5", 0)) << "not IDLE";
	EXPECT_FALSE(speaker.getAi().onPatternShout(ShoutEventType::IDLE, "", 0)) << "no pattern";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
