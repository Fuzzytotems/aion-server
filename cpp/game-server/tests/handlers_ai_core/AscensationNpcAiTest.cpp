// The ascension lane (the ascension analysis's "no owner" row; chunk A1 under this lane's file lease on handlers/ai/quests/AscensationNpcAI.*,
// tested here in P5-05's directory as m5d-plan.md's quest-npc-ais lane tests QuestItemNpcAI): AscensationNpcAI, the AI of the four npcs with
// ai="ascensationquestnpc" - the raider 211042 and orissan 211043 of quest 1006's solo instance, the guardian assassin 205040 and brigade
// general hellion 205041 of quest 2008's. Without the handler AIEngine gives them the warn-mode DummyNpcAI, so they neither start nor answer
// a fight; in Java they are aggressive npcs whose every hit deals exactly 1 damage.
//
// Java: data/handlers/ai/quests/AscensationNpcAI.java:13-24. The raider and orissan rows are npc_templates.xml:60615-60623 and :60624-60632,
// verbatim except their <equipment> (QuestNpcAiTestSupport.h); orissan's skill 16526 is skill_templates.xml:119802-119816 verbatim.
//
// LINK WORKAROUND (reported as a manifest request): A1's handler library, aion_gs_handlers_ai_world, is not linked into this executable - a
// handler target's tests link only their own handler library (AionChunks.cmake) - and A1's own test executable would miss this directory's
// library, which defines AscensationNpcAI's superclasses AggressiveNpcAI and GeneralNpcAI. Until the manifest links the two, this file compiles
// the one leased source into this executable itself. It is the only translation unit that does, so the factory and the class are defined once
// here; the server links the A1 library through the registry as usual.
#include "aion/gameserver/handlers/ai/quests/AscensationNpcAI.cpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/quests/AscensationNpcAI.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/effect/DamageEffect.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include "QuestNpcAiTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;
namespace quests = gameserver::handlers::ai::quests;

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using skillengine::model::Effect;

/** orissan's one npc skill (npc_skills.xml:5068-5070), skills/skill_templates.xml:119802-119816 verbatim */
constexpr int32_t POWERFUL_KNOCKDOWN = 16526;
constexpr const char* POWERFUL_KNOCKDOWN_XML =
	R"(<skill_data>)"
	R"(<skill_template skill_id="16526" name="Powerful Knockdown" nameId="284486" cooldownId="3" stack="NFI_KNOCKDOWN_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3500" cancel_rate="25" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="21" e="1" accmod2="0" hoptype="DAMAGE"><subeffect skill_id="8218" /></skillatk></effects>)"
	R"(<motion name="poweratk" /></skill_template>)"
	R"(</skill_data>)";

class AscensationNpcAiTest : public QuestNpcAiWorldTest {
protected:
	/** `swings` swings of the attacker at the target, each carried out on the spot (CreatureController.attackTarget, time 0, skipChecks); the HP each took */
	static std::vector<int32_t> hpTakenBySwings(Npc& attacker, Npc& target, int32_t swings) {
		std::vector<int32_t> taken;
		for (int32_t i = 0; i < swings; i++) {
			const int32_t before = target.getLifeStats()->getCurrentHp();
			attacker.getController().attackTarget(runtime::Ptr<Creature>(target), 0, true);
			taken.push_back(before - target.getLifeStats()->getCurrentHp());
		}
		return taken;
	}
};

TEST_F(AscensationNpcAiTest, TheMarkerDefinesAFactoryThatBuildsAnAggressiveAscensationNpcAi) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> raider = makeWorldNpc(RAIDER, 500, 500, 100);

	// the factory's own answers for an npc and for a player owner are HandlerRegistry.h's createAI, which RootAiHandlersTest pins
	std::unique_ptr<AbstractAI> ai = quests::AscensationNpcAI_aiFactory(*raider);
	ASSERT_TRUE(ai);
	ASSERT_TRUE(dynamic_cast<quests::AscensationNpcAI*>(ai.get()));
	EXPECT_TRUE(dynamic_cast<roots::AggressiveNpcAI*>(ai.get())) << "AscensationNpcAI extends AggressiveNpcAI";
}

TEST_F(AscensationNpcAiTest, EveryDamageTheOwnerDealsIsOneAndTheDamageItTakesIsUnchanged) {
	AI_TEST_SCOPE;
	runtime::Ref<Npc> raider = makeWorldNpc(RAIDER, 500, 500, 100);
	runtime::Ref<Player> daeva = makeWorldPlayer(700301, 502, 500, 100);
	quests::AscensationNpcAI& ai = installFrom<quests::AscensationNpcAI>(quests::AscensationNpcAI_aiFactory, *raider);

	// AscensationNpcAI.java:20-23: `return 1;` whatever the damage, the target and the effect (AttackUtil calls it with a null effect for a
	// swing, AttackUtil.java:170, and with the skill's effect for a skill, :336)
	for (float damage : {0.0f, 0.5f, 1.0f, 176.9f, 1000000.0f})
		EXPECT_EQ(ai.modifyOwnerDamage(damage, *daeva, nullptr), 1.0f) << "damage " << damage;
	// modifyDamage is not overridden: what the raider takes is AbstractAI's `return damage` (AbstractAI.java:423-425)
	EXPECT_EQ(ai.modifyDamage(*daeva, 176.9f, nullptr), 176.9f);
}

TEST_F(AscensationNpcAiTest, EverySwingOfARaiderTakesExactlyOneHitPoint) {
	AI_TEST_SCOPE;
	// CreatureController.attackTarget -> AttackUtil.calculatePhysAttackResult -> modifyDamageByNpcAi, which replaces every result's damage by
	// the attacker AI's modifyOwnerDamage (AttackUtil.java:164-176) - whatever the attack status, so a dodged swing takes 1 HP too. An npc has
	// one attack result per swing (calculateAdditionalHitCount adds hits for a Player's weapon only), so a swing takes exactly 1 HP.
	runtime::Ref<Npc> raider = makeWorldNpc(RAIDER, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(SHADOW_COURT, 501, 500, 100);
	know(*raider, *target); // AggroList.isAware: the target's hate for its attacker needs a known creature
	installFrom<quests::AscensationNpcAI>(quests::AscensationNpcAI_aiFactory, *raider);
	// the target needs a real NpcAI: NpcController::onAttack casts its own AI to NpcAI without a guard (NpcController.cpp:341, plan D15)
	installLeaf<roots::GeneralNpcAI>(*target);

	constexpr int32_t SWINGS = 20;
	commons::utils::Rnd::seedCurrentThreadForTests(20260928);
	EXPECT_EQ(hpTakenBySwings(*raider, *target, SWINGS), std::vector<int32_t>(SWINGS, 1));

	// the control: the same template, the same swings and the same random sequence with the superclass as its AI
	runtime::Ref<Npc> plainRaider = makeWorldNpc(RAIDER, 520, 500, 100);
	runtime::Ref<Npc> plainTarget = makeWorldNpc(SHADOW_COURT, 521, 500, 100);
	know(*plainRaider, *plainTarget);
	installLeaf<roots::AggressiveNpcAI>(*plainRaider);
	installLeaf<roots::GeneralNpcAI>(*plainTarget);
	commons::utils::Rnd::seedCurrentThreadForTests(20260928);
	std::vector<int32_t> plain = hpTakenBySwings(*plainRaider, *plainTarget, SWINGS);
	int32_t plainTotal = 0;
	for (int32_t hp : plain)
		plainTotal += hp;
	EXPECT_GT(plainTotal, SWINGS) << "an AggressiveNpcAI raider hits harder than 1 per swing";
}

/**
 * The skill side of the same rule: AttackUtil.calculateSkillResult hands the skill's Effect to the effector AI's modifyOwnerDamage
 * (AttackUtil.java:336, AttackUtil.cpp:486), and AscensationNpcAI answers 1 for it as for a swing. Orissan (211043) is the one of the four npcs
 * with a skill, 16526 Powerful Knockdown (a <skillatk> of 21 % of the attack). DamageEffect.calculateDamage (DamageEffect.java:49-52) runs the
 * skill's damage through AttackUtil.calculateSkillResult into the Effect's reserved HP of position 1, which applyEffect deals; the npc target has
 * no PvE ratios and no shield, so the 1 arrives unchanged.
 */
TEST_F(AscensationNpcAiTest, OrissansSkillDealsOneAsWell) {
	AI_TEST_SCOPE;
	auto skills = xml::bindString<dataholders::SkillData>(contexts.emplace_back(), POWERFUL_KNOCKDOWN_XML);
	const skillengine::model::SkillTemplate* knockdown = skills->getSkillTemplate(POWERFUL_KNOCKDOWN);
	ASSERT_NE(knockdown, nullptr);
	const auto* damage = dynamic_cast<const skillengine::effect::DamageEffect*>(knockdown->getEffects()->getEffects().at(0).get());
	ASSERT_NE(damage, nullptr) << "<skillatk> is a SkillAttackInstantEffect, a DamageEffect";

	runtime::Ref<Npc> orissan = makeWorldNpc(ORISSAN, 500, 500, 100);
	runtime::Ref<Npc> target = makeWorldNpc(SHADOW_COURT, 502, 500, 100);
	quests::AscensationNpcAI& ai = installFrom<quests::AscensationNpcAI>(quests::AscensationNpcAI_aiFactory, *orissan);
	installLeaf<roots::GeneralNpcAI>(*target);

	// Java `new Effect(effector, effected, template, level)`, the skill's effect the call carries
	runtime::Ref<Effect> effect = Effect::create(*orissan, runtime::Ptr<Creature>(*target), knockdown, 1);
	for (float value : {0.0f, 0.5f, 176.9f, 1000000.0f})
		EXPECT_EQ(ai.modifyOwnerDamage(value, *target, runtime::Ptr<Effect>(*effect)), 1.0f) << "damage " << value << " with a skill's effect";

	// the skill path itself, with the same random draws for orissan and for a control orissan whose AI is the superclass
	commons::utils::Rnd::seedCurrentThreadForTests(20260928);
	damage->calculateDamage(*effect);
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 1) << "orissan's knockdown reserves 1 HP";

	runtime::Ref<Npc> plainOrissan = makeWorldNpc(ORISSAN, 520, 500, 100);
	runtime::Ref<Npc> plainTarget = makeWorldNpc(SHADOW_COURT, 522, 500, 100);
	installLeaf<roots::AggressiveNpcAI>(*plainOrissan);
	installLeaf<roots::GeneralNpcAI>(*plainTarget);
	runtime::Ref<Effect> plainEffect = Effect::create(*plainOrissan, runtime::Ptr<Creature>(*plainTarget), knockdown, 1);
	commons::utils::Rnd::seedCurrentThreadForTests(20260928);
	damage->calculateDamage(*plainEffect);
	EXPECT_GT(plainEffect->getReserveds(1)->getValue(), 1) << "an AggressiveNpcAI orissan's knockdown hits harder than 1";
}

TEST_F(AscensationNpcAiTest, ARaiderStartsAFightWithTheCharacterItSees) {
	AI_TEST_SCOPE;
	// tribe AGGRESSIVESINGLEMONSTER is aggressive to PC (tribe_relations.xml:29-31), srange 20 and no sangle (360): AggressiveNpcAI's rule
	// aggroes the level-1 character 5 m away (the level difference 1 - 9 is below 10, CreatureEventHandler.validateAggro)
	runtime::Ref<Npc> raider = makeWorldNpc(RAIDER, 500, 500, 100);
	runtime::Ref<Player> character = makeWorldPlayer(700401, 505, 500, 100);
	know(*raider, *character);
	quests::AscensationNpcAI& ai = installFrom<quests::AscensationNpcAI>(quests::AscensationNpcAI_aiFactory, *raider);

	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *character);
	EXPECT_FALSE(raider->getAggroList().isHating(*character)) << "the AggroNotifier runs 500 ms later (AggroEventHandler.java:23)";
	executor->advance(std::chrono::milliseconds(500));
	EXPECT_TRUE(raider->getAggroList().isHating(*character));
	EXPECT_EQ(raider->getAggroList().getHate(*character), 1);
	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	EXPECT_TRUE(raider->isTargeting(character->getObjectId()));
}

} // namespace
} // namespace aion::gameserver::ai::testing
