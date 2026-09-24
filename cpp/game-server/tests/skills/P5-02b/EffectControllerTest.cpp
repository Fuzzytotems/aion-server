// P5-02b, M5b-2 stage 1 part 2, work items K-02, K-03, K-04 and K-07 (m5b2-plan.md §5): EffectController (EffectController.java:39-773) and
// PlayerEffectController (PlayerEffectController.java:36-154) with their stacking, conflict and dispel rules, CumulativeResist
// (CumulativeResist.java) and the effect modifiers (skillengine/effect/modifier/*.java).
//
// The effects are real Effects between spawned creatures of a real map instance whose skills carry ProbeEffect templates (EffectTestSupport.h);
// a probe's journal says which effects the controller started and ended, in which order. Expectations are derived by hand from the Java
// methods cited per case.

#include "EffectTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/effect/CumulativeResistType.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplates.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/FearEffect.h"
#include "aion/gameserver/skillengine/effect/HideEffect.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::effect::CumulativeResistType;
using controllers::effect::EffectController;
using controllers::effect::PlayerEffectController;
using effect::AbnormalState;
using model::Effect;
using model::EffectResult;

class EffectControllerTest : public EffectWorldTest {};

/** One probe at position 1 with the given effect id and basic level (0: no id, never conflicts by id) and a duration of 60 s */
std::vector<std::unique_ptr<effect::EffectTemplate>> idProbe(std::string name, Journal* journal, int32_t effectId, int32_t basicLvl) {
	auto p = probe(std::move(name), journal, 1);
	p->effectid = effectId;
	p->basicLvl = basicLvl;
	p->duration2 = 60000;
	return probeList(std::move(p));
}

/** Java Skill.endCast's `new Effect(...); effect.initialize()` and applyEffect's addToEffectedController for one effected creature */
Ref<Effect> addTo(Creature& caster, Creature& target, const model::SkillTemplate* skill, int32_t level = 1) {
	Ref<Effect> effect = Effect::create(caster, Ptr<Creature>(target), skill, level);
	effect->initialize();
	effect->addToEffectedController();
	return effect;
}

// ---- add, put and clear (EffectController.java:39-114, 310-325) ------------------------------------------------------------------------------

/**
 * addEffect puts an effect under its skill's stack, starts it and broadcasts (EffectController.java:95-100). A second effect of the same stack
 * replaces the map entry without ending the first (put is a plain Map.put, :107-114); when the replaced one ends, clearEffect finds another
 * effect under the stack and leaves it (the identity check, :314-317).
 */
TEST_F(EffectControllerTest, AnEffectReplacesTheEntryOfItsStackAndAReplacedOneDoesNotRemoveItsSuccessor) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(4001);
	Ref<Player> target = makePlayer(4002);
	Journal journal;
	const model::SkillTemplate* first = keep(skillWithProbes(skillXml(9201, "SHARED", R"(tslot="BUFF")"), idProbe("first", &journal, 0, 0)));
	const model::SkillTemplate* second = keep(skillWithProbes(skillXml(9202, "SHARED", R"(tslot="BUFF")"), idProbe("second", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = target->getEffectController();

	Ref<Effect> a = addTo(*caster, *target, first);
	EXPECT_EQ(effects->getAbnormalEffect("SHARED").get(), a.get());
	EXPECT_FALSE(effects->isEmpty());
	EXPECT_TRUE(effects->hasAbnormalEffect(9201));
	EXPECT_EQ(journal, (Journal{"first.calculate", "first.start"}));

	journal.clear();
	Ref<Effect> b = addTo(*caster, *target, second);
	EXPECT_EQ(effects->getAbnormalEffect("SHARED").get(), b.get()) << "the same stack: the map entry is replaced";
	EXPECT_EQ(journal, (Journal{"second.calculate", "second.start"})) << "the replaced effect is not ended";
	EXPECT_FALSE(effects->hasAbnormalEffect(9201));
	EXPECT_EQ(effects->getAbnormalEffects().size(), 1u);

	journal.clear();
	a->endEffect();
	EXPECT_EQ(journal, (Journal{"first.end"}));
	EXPECT_EQ(effects->getAbnormalEffect("SHARED").get(), b.get()) << "the entry belongs to its successor, which stays";
	b->endEffect();
	EXPECT_TRUE(effects->isEmpty());
}

/**
 * Passive effects of one stack (EffectController.java:43-63): an existing one with a higher stack level, or the same stack level and a higher
 * skill level, keeps its place and the new one is dropped unstarted; otherwise the existing one is ended and replaced. Passive effects live in
 * the passive map, not among the abnormal effects.
 */
TEST_F(EffectControllerTest, APassiveEffectReplacesOnlyALowerStackOrSkillLevel) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4011);
	Journal journal;
	auto passiveXml = [](int32_t skillId, int32_t lvl) {
		return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="passive" nameId="1" stack="MASTERY" lvl=")" + std::to_string(lvl)
			+ R"(" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" duration="0"/>)";
	};
	const model::SkillTemplate* stack2 = keep(skillWithProbes(passiveXml(9211, 2), idProbe("stack2", &journal, 0, 0)));
	const model::SkillTemplate* stack1 = keep(skillWithProbes(passiveXml(9212, 1), idProbe("stack1", &journal, 0, 0)));
	const model::SkillTemplate* stack2b = keep(skillWithProbes(passiveXml(9213, 2), idProbe("stack2b", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = player->getEffectController();

	Ref<Effect> existing = addTo(*player, *player, stack2, 3);
	EXPECT_TRUE(effects->isEmpty()) << "no abnormal effect: the passive map holds it";
	ASSERT_EQ(effects->getAllEffects().size(), 1u);
	journal.clear();

	addTo(*player, *player, stack1, 9);
	EXPECT_EQ(journal, (Journal{"stack1.calculate"})) << "a lower stack level is dropped before it starts";
	addTo(*player, *player, stack2b, 2);
	EXPECT_EQ(journal, (Journal{"stack1.calculate", "stack2b.calculate"})) << "same stack level, lower skill level: dropped";
	ASSERT_EQ(effects->getAllEffects().size(), 1u);
	EXPECT_EQ(effects->getAllEffects()[0].get(), existing.get());

	journal.clear();
	Ref<Effect> higher = addTo(*player, *player, stack2b, 3);
	EXPECT_EQ(journal, (Journal{"stack2b.calculate", "stack2.end", "stack2b.start"})) << "same stack and skill level: the existing one ends first";
	ASSERT_EQ(effects->getAllEffects().size(), 1u);
	EXPECT_EQ(effects->getAllEffects()[0].get(), higher.get());
	effects->removeAllEffects(true);
	EXPECT_TRUE(effects->getAllEffects().empty()) << "logout ends the passive effects too";
}

/**
 * searchConflict (EffectController.java:125-158): two effects whose templates share an effect id conflict when their target slots match. The
 * existing one keeps its place when its basic level is higher - the new one is dropped, and marked CONFLICT unless it is a DEBUFF - and is
 * ended when it is not higher. Differing slots only conflict through the conflict effect types (canConflict, :193-199).
 */
TEST_F(EffectControllerTest, EffectsWithOneEffectIdKeepTheHigherBasicLevel) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(4021);
	Ref<Player> target = makePlayer(4022);
	Journal journal;
	const model::SkillTemplate* strong = keep(skillWithProbes(skillXml(9221, "STRONG", R"(tslot="BUFF")"), idProbe("strong", &journal, 700, 5)));
	const model::SkillTemplate* weak = keep(skillWithProbes(skillXml(9222, "WEAK", R"(tslot="BUFF")"), idProbe("weak", &journal, 700, 3)));
	const model::SkillTemplate* equal = keep(skillWithProbes(skillXml(9223, "EQUAL", R"(tslot="BUFF")"), idProbe("equal", &journal, 700, 5)));
	const model::SkillTemplate* otherSlot = keep(skillWithProbes(skillXml(9224, "OTHER_SLOT", R"(tslot="SPEC")"), idProbe("other", &journal, 700, 1)));
	Ptr<PlayerEffectController> effects = target->getEffectController();

	Ref<Effect> kept = addTo(*caster, *target, strong);
	journal.clear();
	Ref<Effect> dropped = Effect::create(*caster, target, weak, 1);
	dropped->addAllEffectToSucess(); // past initialize's own conflict check, as a second cast racing the first would be
	effects->addEffect(*dropped);
	EXPECT_EQ(dropped->getEffectResult(), EffectResult::CONFLICT);
	EXPECT_EQ(journal, Journal{}) << "the weaker effect is neither started nor does it end the stronger one";
	EXPECT_FALSE(effects->hasAbnormalEffect(9222));

	Ref<Effect> otherSlotEffect = addTo(*caster, *target, otherSlot);
	EXPECT_TRUE(effects->hasAbnormalEffect(9224)) << "another target slot without shared conflict types does not conflict";
	journal.clear();

	Ref<Effect> replacing = addTo(*caster, *target, equal);
	EXPECT_EQ(journal, (Journal{"equal.calculate", "strong.end", "equal.start"})) << "an equal basic level ends the existing effect";
	EXPECT_FALSE(effects->hasAbnormalEffect(9221));
	EXPECT_TRUE(effects->hasAbnormalEffect(9223));
	effects->removeAllEffects(true);
}

/** A DEBUFF that loses the conflict is dropped but not marked CONFLICT (EffectController.java:68-69) */
TEST_F(EffectControllerTest, ALosingDebuffIsDroppedWithoutTheConflictResult) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> caster = makeNpc(700301);
	Ref<Player> target = makePlayer(4031);
	Journal journal;
	const model::SkillTemplate* strong = keep(skillWithProbes(skillXml(9231, "STRONG_DEBUFF", R"(tslot="DEBUFF")"), idProbe("strong", &journal, 800, 5)));
	const model::SkillTemplate* weak = keep(skillWithProbes(skillXml(9232, "WEAK_DEBUFF", R"(tslot="DEBUFF")"), idProbe("weak", &journal, 800, 3)));
	Ptr<PlayerEffectController> effects = target->getEffectController();

	addTo(*caster, *target, strong);
	Ref<Effect> weakDebuff = addTo(*caster, *target, weak);
	EXPECT_EQ(weakDebuff->getEffectResult(), EffectResult::NORMAL) << "isConflicting skips a DEBUFF, and so does the CONFLICT mark";
	EXPECT_FALSE(effects->hasAbnormalEffect(9232)) << "but the controller drops it";
	effects->removeAllEffects(true);
}

/** Conflict effect types make differing target slots conflict (canConflict, EffectController.java:196-197: Barricade of Steel vs Holy Shield) */
TEST_F(EffectControllerTest, SharedConflictTypesMakeDifferentSlotsConflict) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(4041);
	Ref<Player> target = makePlayer(4042);
	Journal journal;
	const model::SkillTemplate* shieldBuff = keep(skillWithProbes(skillXml(9241, "SHIELD_BUFF", R"(tslot="BUFF")"),
		idProbe("buff", &journal, 900, 5), {}, {effect::EffectType::SHIELD}));
	const model::SkillTemplate* shieldSpec = keep(skillWithProbes(skillXml(9242, "SHIELD_SPEC", R"(tslot="SPEC")"),
		idProbe("spec", &journal, 900, 3), {}, {effect::EffectType::SHIELD}));
	const model::SkillTemplate* plainSpec = keep(skillWithProbes(skillXml(9243, "PLAIN_SPEC", R"(tslot="SPEC")"), idProbe("plain", &journal, 900, 3)));
	Ptr<PlayerEffectController> effects = target->getEffectController();

	addTo(*caster, *target, shieldBuff);
	Ref<Effect> spec = addTo(*caster, *target, shieldSpec);
	EXPECT_EQ(spec->getEffectResult(), EffectResult::CONFLICT) << "initialize's isConflicting already sees the shared SHIELD type";
	EXPECT_FALSE(effects->hasAbnormalEffect(9242));
	Ref<Effect> plain = addTo(*caster, *target, plainSpec);
	EXPECT_EQ(plain->getEffectResult(), EffectResult::NORMAL);
	EXPECT_TRUE(effects->hasAbnormalEffect(9243)) << "no shared type, another slot: no conflict";
	effects->removeAllEffects(true);
}

/** A HideEffect with a higher basic level does not make isConflicting answer true (EffectController.java:180) */
class ProbeHide final : public effect::HideEffect {
public:
	explicit ProbeHide(Journal* probeJournal) : journal(probeJournal) {}
	void applyEffect(model::Effect&) const override { journal->push_back("hide.apply"); }
	void startEffect(model::Effect&) const override { journal->push_back("hide.start"); }
	void endEffect(model::Effect&) const override { journal->push_back("hide.end"); }
	using HideEffect::basicLvl;
	using HideEffect::duration2;
	using HideEffect::effectid;
	using HideEffect::position;
	Journal* const journal;
};

TEST_F(EffectControllerTest, AStrongerHideDoesNotMakeANewEffectConflictAtInitialize) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4051);
	Journal journal;
	auto hide = std::make_unique<ProbeHide>(&journal);
	hide->position = 1;
	hide->effectid = 950;
	hide->basicLvl = 9;
	hide->duration2 = 60000;
	std::vector<std::unique_ptr<effect::EffectTemplate>> hideList;
	hideList.push_back(std::move(hide));
	const model::SkillTemplate* hideSkill = keep(skillWithProbes(skillXml(9251, "HIDE", R"(tslot="BUFF")"), std::move(hideList)));
	const model::SkillTemplate* weaker = keep(skillWithProbes(skillXml(9252, "AFTER_HIDE", R"(tslot="BUFF")"), idProbe("weak", &journal, 950, 1)));
	Ptr<PlayerEffectController> effects = player->getEffectController();

	Ref<Effect> hidden = Effect::create(*player, player, hideSkill, 1);
	hidden->addAllEffectToSucess();
	effects->addEffect(*hidden);
	ASSERT_TRUE(effects->hasAbnormalEffect(9251));
	EXPECT_FALSE(effects->isConflicting(*Effect::create(*player, player, weaker, 1))) << "the stronger existing template is a HideEffect";
	effects->removeAllEffects(true);
}

/** A new effect ends the effect with the same non-zero conflict_id (endConflictedEffect, EffectController.java:116-123) */
TEST_F(EffectControllerTest, AnEffectEndsTheEffectOfItsConflictId) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4061);
	Journal journal;
	const model::SkillTemplate* first = keep(skillWithProbes(skillXml(9261, "C1", R"(tslot="BUFF" conflict_id="12")"), idProbe("first", &journal, 0, 0)));
	const model::SkillTemplate* second = keep(skillWithProbes(skillXml(9262, "C2", R"(tslot="BUFF" conflict_id="12")"), idProbe("second", &journal, 0, 0)));
	const model::SkillTemplate* none = keep(skillWithProbes(skillXml(9263, "C3", R"(tslot="BUFF")"), idProbe("none", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = player->getEffectController();

	addTo(*player, *player, first);
	addTo(*player, *player, none);
	journal.clear();
	addTo(*player, *player, second);
	EXPECT_EQ(journal, (Journal{"second.calculate", "first.end", "second.start"}));
	EXPECT_TRUE(effects->hasAbnormalEffect(9263)) << "conflict id 0 never matches";
	effects->removeAllEffects(true);
}

/** The cooldown id limit: one effect per slot and cooldown id (checkEffectCooldownId, EffectController.java:221-240); cooldown id 1 is exempt */
TEST_F(EffectControllerTest, OneEffectPerTargetSlotAndCooldownId) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4071);
	Journal journal;
	const model::SkillTemplate* first = keep(skillWithProbes(skillXml(9271, "CD1", R"(tslot="BUFF" cooldownId="44")"), idProbe("first", &journal, 0, 0)));
	const model::SkillTemplate* second = keep(skillWithProbes(skillXml(9272, "CD2", R"(tslot="BUFF" cooldownId="44")"), idProbe("second", &journal, 0, 0)));
	const model::SkillTemplate* free1 = keep(skillWithProbes(skillXml(9273, "CD3", R"(tslot="BUFF" cooldownId="1")"), idProbe("free1", &journal, 0, 0)));
	const model::SkillTemplate* free2 = keep(skillWithProbes(skillXml(9274, "CD4", R"(tslot="BUFF" cooldownId="1")"), idProbe("free2", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = player->getEffectController();

	addTo(*player, *player, first);
	addTo(*player, *player, free1);
	journal.clear();
	addTo(*player, *player, second);
	EXPECT_EQ(journal, (Journal{"second.calculate", "first.end", "second.start"}));
	journal.clear();
	addTo(*player, *player, free2);
	EXPECT_EQ(journal, (Journal{"free2.calculate", "free2.start"})) << "cooldown id 1 limits nothing";
	EXPECT_TRUE(effects->hasAbnormalEffect(9273));
	effects->removeAllEffects(true);
}

// ---- lookups, removal and dispel (EffectController.java:283-485, 492-651) ---------------------------------------------------------------------

/** removeEffect finds the effect of a skill id through SKILL_DATA (findBySkillId, EffectController.java:327-338); an unknown id is Java's NPE */
TEST_F(EffectControllerTest, RemoveEffectEndsTheEffectOfASkillIdAndAnUnknownIdThrows) {
	EFFECT_TEST_SCOPE;
	publishSkillData(skillXml(9281, "REMOVE", R"(tslot="BUFF")"));
	Journal journal;
	injectProbes(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9281), idProbe("removed", &journal, 0, 0));
	Ref<Player> player = makePlayer(4081);
	Ptr<PlayerEffectController> effects = player->getEffectController();

	addTo(*player, *player, dataholders::DataManager::SKILL_DATA->getSkillTemplate(9281));
	EXPECT_EQ(effects->findBySkillId(9281)->getSkillId(), 9281);
	effects->removeEffect(9281);
	EXPECT_EQ(journal, (Journal{"removed.calculate", "removed.start", "removed.end"}));
	EXPECT_EQ(effects->findBySkillId(9281), nullptr);
	EXPECT_NO_THROW(effects->removeEffect(9281)) << "nothing left to remove";
	try {
		effects->removeEffect(9999);
		FAIL() << "an unknown skill id must throw";
	} catch (const runtime::NullPointerException& npe) {
		EXPECT_EQ(std::string(npe.what()), "Skill with ID 9999 does not exist");
	}
}

/**
 * removeByDispelSlotType - the M5b-1 AION_PARTIAL, closed - is removeByDispelEffect(null, slot, 255, 100, 100) (EffectController.java:427-429,
 * 450-485): every effect of the slot whose req_dispel_level is at most 100 loses 100 power (req_dispel_count) and ends when that leaves none.
 */
TEST_F(EffectControllerTest, RemoveByDispelSlotTypeEndsTheDispellableEffectsOfTheSlot) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700321);
	Journal journal;
	const model::SkillTemplate* buff =
		keep(skillWithProbes(skillXml(9291, "D_BUFF", R"(tslot="BUFF" req_dispel_level="1" req_dispel_count="10")"), idProbe("buff", &journal, 0, 0)));
	const model::SkillTemplate* strongBuff = keep(skillWithProbes(skillXml(9292, "D_STRONG", R"(tslot="BUFF" req_dispel_level="1" req_dispel_count="150")"),
		idProbe("strong", &journal, 0, 0)));
	const model::SkillTemplate* highLevel = keep(skillWithProbes(skillXml(9293, "D_HIGH", R"(tslot="BUFF" req_dispel_level="101" req_dispel_count="1")"),
		idProbe("high", &journal, 0, 0)));
	const model::SkillTemplate* debuff =
		keep(skillWithProbes(skillXml(9294, "D_DEBUFF", R"(tslot="DEBUFF" req_dispel_level="1" req_dispel_count="1")"), idProbe("debuff", &journal, 0, 0)));
	Ptr<EffectController> effects = npc->getEffectController();

	addTo(*npc, *npc, buff);
	Ref<Effect> strong = addTo(*npc, *npc, strongBuff);
	addTo(*npc, *npc, highLevel);
	addTo(*npc, *npc, debuff);
	journal.clear();
	const uint64_t partialsBefore = runtime::partialHitCount();

	effects->removeByDispelSlotType(model::DispelSlotType::BUFF);
	EXPECT_EQ(journal, (Journal{"buff.end"}));
	EXPECT_EQ(strong->getPower(), 50) << "150 - 100: weakened, not removed";
	EXPECT_TRUE(effects->hasAbnormalEffect(9292));
	EXPECT_TRUE(effects->hasAbnormalEffect(9293)) << "req_dispel_level 101 > 100";
	EXPECT_TRUE(effects->hasAbnormalEffect(9294)) << "another slot";
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore) << "the partial is closed";

	effects->removeByDispelSlotType(model::DispelSlotType::BUFF);
	EXPECT_EQ(journal, (Journal{"buff.end", "strong.end"})) << "the second dispel takes the remaining 50";
	effects->removeAllEffects(true);
}

/**
 * The dispel of DispelBuffCounterAtkEffect (EffectController.java:492-526, 619-624, 762-773): calculateBuffsOrEffectorDebuffsToRemove designates
 * up to `count` dispellable buffs to the dispelling effect (and skips those already designated); resetDesignatedDispelEffect releases them;
 * dispelBuffCounterAtkEffect ends exactly the designated ones.
 */
TEST_F(EffectControllerTest, TheCounterAttackDispelDesignatesEndsAndReleasesBuffs) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(4101);
	Ref<Player> target = makePlayer(4102);
	Journal journal;
	const model::SkillTemplate* buff1 = keep(skillWithProbes(skillXml(9301, "B1", R"(tslot="BUFF" dispel_category="BUFF" req_dispel_level="1")"),
		idProbe("b1", &journal, 0, 0)));
	const model::SkillTemplate* buff2 = keep(skillWithProbes(skillXml(9302, "B2", R"(tslot="BUFF" dispel_category="BUFF" req_dispel_level="1")"),
		idProbe("b2", &journal, 0, 0)));
	const model::SkillTemplate* dispel = keep(skillWithProbes(skillXml(9303, "DISPEL", R"(tslot="DEBUFF")"), idProbe("dispel", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = target->getEffectController();
	Ref<Effect> b1 = addTo(*target, *target, buff1);
	Ref<Effect> b2 = addTo(*target, *target, buff2);
	Ref<Effect> dispeller = Effect::create(*caster, target, dispel, 1);
	journal.clear();

	EXPECT_EQ(effects->calculateBuffsOrEffectorDebuffsToRemove(*dispeller, 1, 10, 100), 1) << "count 1";
	EXPECT_EQ(b1->getDesignatedDispelEffect().get(), dispeller.get()) << "the first in insertion order";
	EXPECT_EQ(b2->getDesignatedDispelEffect(), nullptr);
	EXPECT_EQ(effects->calculateBuffsOrEffectorDebuffsToRemove(*dispeller, 1, 10, 100), 1);
	EXPECT_EQ(b2->getDesignatedDispelEffect().get(), dispeller.get()) << "an already designated buff is not dispellable (isDispellable)";

	effects->resetDesignatedDispelEffect(*dispeller);
	EXPECT_EQ(b1->getDesignatedDispelEffect(), nullptr);
	EXPECT_EQ(b2->getDesignatedDispelEffect(), nullptr);

	EXPECT_EQ(effects->calculateBuffsOrEffectorDebuffsToRemove(*dispeller, 5, 0, 100), 0) << "req_dispel_level 1 > dispel level 0";
	EXPECT_EQ(effects->calculateBuffsOrEffectorDebuffsToRemove(*dispeller, 5, 10, 100), 2);
	effects->dispelBuffCounterAtkEffect(*dispeller);
	EXPECT_EQ(journal, (Journal{"b1.end", "b2.end"}));
	EXPECT_EQ(b1->getDesignatedDispelEffect(), nullptr) << "an ended effect lets go of its dispeller (the C++ breaker of Effect.endEffect)";
	EXPECT_TRUE(effects->isEmpty());
}

/**
 * removeEffectByDispelCat (EffectController.java:528-598): a DEBUFF_MENTAL dispel takes ALL and DEBUFF_MENTAL debuffs of the target slot up to
 * the dispel level, not DEBUFF_PHYSICAL ones; count limits how many are removed.
 */
TEST_F(EffectControllerTest, TheDispelCategoryDecidesWhichDebuffsAreRemoved) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> caster = makeNpc(700341);
	Ref<Player> target = makePlayer(4111);
	Journal journal;
	const model::SkillTemplate* mental = keep(skillWithProbes(skillXml(9311, "MENTAL", R"(tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="2")"),
		idProbe("mental", &journal, 0, 0)));
	const model::SkillTemplate* physical = keep(skillWithProbes(
		skillXml(9312, "PHYSICAL", R"(tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1")"), idProbe("physical", &journal, 0, 0)));
	const model::SkillTemplate* all = keep(skillWithProbes(skillXml(9313, "ALL", R"(tslot="DEBUFF" dispel_category="ALL" req_dispel_level="1")"),
		idProbe("all", &journal, 0, 0)));
	const model::SkillTemplate* tooHigh = keep(skillWithProbes(skillXml(9314, "HIGH", R"(tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="9")"),
		idProbe("high", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = target->getEffectController();
	addTo(*caster, *target, mental);
	addTo(*caster, *target, physical);
	addTo(*caster, *target, all);
	addTo(*caster, *target, tooHigh);
	journal.clear();

	effects->removeEffectByDispelCat(model::DispelCategoryType::DEBUFF_MENTAL, model::SkillTargetSlot::DEBUFF, 1, 5, 100);
	EXPECT_EQ(journal, (Journal{"mental.end"})) << "count 1: the first matching one";
	effects->removeEffectByDispelCat(model::DispelCategoryType::DEBUFF_MENTAL, model::SkillTargetSlot::DEBUFF, 5, 5, 100);
	EXPECT_EQ(journal, (Journal{"mental.end", "all.end"})) << "ALL matches a mental dispel, PHYSICAL does not, level 9 > 5";
	EXPECT_TRUE(effects->hasAbnormalEffect(9312));
	EXPECT_TRUE(effects->hasAbnormalEffect(9314));
	effects->removeAllEffects(true);
}

// ---- abnormal states and shields (EffectController.java:292-302, 706-752) ------------------------------------------------------------------

/**
 * unsetAbnormal keeps a state that a second effect of the map still carries (the `++count == 2` early return, EffectController.java:720-734):
 * the effect that asks to unset it is normally still in the map (a template's endEffect runs before clearEffect).
 */
TEST_F(EffectControllerTest, AnAbnormalStateStaysWhileASecondEffectCarriesIt) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> caster = makeNpc(700351);
	Ref<Player> target = makePlayer(4121);
	Journal journal;
	auto rootProbe = [&](std::string name) {
		auto p = probe(std::move(name), &journal, 1);
		p->duration2 = 60000;
		p->onStart = [](Effect& effect) {
			effect.setAbnormal(AbnormalState::ROOT);
			effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::ROOT);
		};
		p->onEnd = [](Effect& effect) { effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::ROOT); };
		return probeList(std::move(p));
	};
	const model::SkillTemplate* root1 = keep(skillWithProbes(skillXml(9321, "ROOT1", R"(tslot="DEBUFF")"), rootProbe("root1")));
	const model::SkillTemplate* root2 = keep(skillWithProbes(skillXml(9322, "ROOT2", R"(tslot="DEBUFF")"), rootProbe("root2")));
	Ptr<PlayerEffectController> effects = target->getEffectController();

	Ref<Effect> first = addTo(*caster, *target, root1);
	Ref<Effect> second = addTo(*caster, *target, root2);
	EXPECT_TRUE(effects->isAbnormalSet(AbnormalState::ROOT));
	EXPECT_TRUE(effects->isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE)) << "ROOT is one of the can't-move states";
	first->endEffect();
	EXPECT_TRUE(effects->isAbnormalSet(AbnormalState::ROOT)) << "the second root still carries it";
	second->endEffect();
	EXPECT_FALSE(effects->isAbnormalSet(AbnormalState::ROOT));
	EXPECT_TRUE(effects->isAbnormalSet(AbnormalState::NONE));
}

/** isUnderNormalShield: an abnormal effect whose shield defense has the NORMAL bit (2) (EffectController.java:300-302, m5b-plan.md D14) */
TEST_F(EffectControllerTest, ANormalShieldBitMakesTheOwnerShielded) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4131);
	Journal journal;
	const model::SkillTemplate* shield = keep(skillWithProbes(skillXml(9331, "SHIELD", R"(tslot="BUFF")"), idProbe("shield", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = player->getEffectController();
	EXPECT_FALSE(effects->isUnderNormalShield()) << "no effect";

	Ref<Effect> effect = addTo(*player, *player, shield);
	EXPECT_FALSE(effects->isUnderNormalShield()) << "an effect without the NORMAL bit";
	effect->setShieldDefense(1 | 8); // REFLECTOR and PROTECT
	EXPECT_FALSE(effects->isUnderNormalShield());
	effect->setShieldDefense(2);
	EXPECT_TRUE(effects->isUnderNormalShield());
	effect->endEffect();
	EXPECT_FALSE(effects->isUnderNormalShield());
}

/**
 * Death (removeAllEffects(false)) ends the abnormal effects that can be removed on death and keeps the noremoveatdie ones and the passive ones;
 * logout (removeAllEffects(true)) ends every one (EffectController.java:656-671). keepBuffsOnDie keeps everything but the debuffs of a player
 * (PlayerEffectController.java:132-139).
 */
TEST_F(EffectControllerTest, DeathEndsTheRemovableEffectsAndLogoutEndsEveryOne) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4141);
	Journal journal;
	const model::SkillTemplate* plain = keep(skillWithProbes(skillXml(9341, "PLAIN", R"(tslot="BUFF")"), idProbe("plain", &journal, 0, 0)));
	const model::SkillTemplate* kept = keep(skillWithProbes(skillXml(9342, "KEPT", R"(tslot="BUFF" noremoveatdie="true")"), idProbe("kept", &journal, 0, 0)));
	const model::SkillTemplate* debuff = keep(skillWithProbes(skillXml(9343, "DEBUFF", R"(tslot="DEBUFF")"), idProbe("debuff", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = player->getEffectController();
	addTo(*player, *player, plain);
	addTo(*player, *player, kept);
	journal.clear();

	effects->removeAllEffects();
	EXPECT_EQ(journal, (Journal{"plain.end"}));
	EXPECT_TRUE(effects->hasAbnormalEffect(9342));

	addTo(*player, *player, plain);
	addTo(*player, *player, debuff);
	effects->setKeepBuffsOnDie(true);
	journal.clear();
	effects->removeAllEffects();
	EXPECT_EQ(journal, (Journal{"debuff.end"})) << "keepBuffsOnDie: only the debuff goes";

	journal.clear();
	effects->removeAllEffects(true);
	EXPECT_EQ(journal, (Journal{"kept.end", "plain.end"})) << "logout ends every effect, in the map's insertion order";
	EXPECT_TRUE(effects->isEmpty());
}

// ---- PlayerEffectController (PlayerEffectController.java:36-154) -----------------------------------------------------------------------------

/** A DEBUFF from a player who is no enemy is refused unless it is forced (checkDuelCondition, PlayerEffectController.java:37-42, 91-96) */
TEST_F(EffectControllerTest, APlayerRefusesADebuffOfAFriendlyPlayerUnlessItIsForced) {
	EFFECT_TEST_SCOPE;
	Ref<Player> friendly = makePlayer(4151);
	Ref<Player> target = makePlayer(4152);
	Ref<Npc> npc = makeNpc(700361);
	Journal journal;
	const model::SkillTemplate* debuff = keep(skillWithProbes(skillXml(9351, "DUEL", R"(tslot="DEBUFF")"), idProbe("debuff", &journal, 0, 0)));
	const model::SkillTemplate* buff = keep(skillWithProbes(skillXml(9352, "FRIENDLY_BUFF", R"(tslot="BUFF")"), idProbe("buff", &journal, 0, 0)));
	Ptr<PlayerEffectController> effects = target->getEffectController();
	ASSERT_FALSE(target->isEnemy(*friendly)) << "two Elyos outside a duel";

	addTo(*friendly, *target, debuff);
	EXPECT_FALSE(effects->hasAbnormalEffect(9351)) << "a friendly player's debuff";
	addTo(*friendly, *target, buff);
	EXPECT_TRUE(effects->hasAbnormalEffect(9352)) << "a friendly player's buff";
	Ref<Effect> forced = Effect::create(*friendly, target, debuff, 1, std::nullopt, model::Effect::ForceType::DEFAULT);
	forced->initialize();
	forced->addToEffectedController();
	EXPECT_TRUE(effects->hasAbnormalEffect(9351)) << "a forced effect is added all the same";
	forced->endEffect();
	addTo(*npc, *target, debuff);
	EXPECT_TRUE(effects->hasAbnormalEffect(9351)) << "an npc's debuff";
	effects->removeAllEffects(true);
}

/**
 * addSavedEffect - the M5a AION_PARTIAL, closed - restores a stored effect: forced to the stored remaining time, every template successful
 * (addAllEffectToSucess), started and put under its stack (PlayerEffectController.java:97-118). A row without remaining time is dropped.
 */
TEST_F(EffectControllerTest, ASavedEffectIsRestoredWithItsRemainingTime) {
	EFFECT_TEST_SCOPE;
	publishSkillData(skillXml(9361, "SAVED", R"(tslot="BUFF")"));
	Journal journal;
	auto savedProbe = probe("saved", &journal, 1, ProbeEffect::Calculate::FAIL);
	savedProbe->duration2 = 5000; // the template's own duration is not used
	injectProbes(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9361), probeList(std::move(savedProbe)));
	Ref<Player> player = makePlayer(4161);
	Ptr<PlayerEffectController> effects = player->getEffectController();
	const uint64_t partialsBefore = runtime::partialHitCount();

	effects->addSavedEffect(9361, 3, 0, 0, nullptr, nullptr);
	EXPECT_TRUE(effects->isEmpty()) << "no remaining time: dropped";
	effects->addSavedEffect(9361, 3, 42000, 0, nullptr, nullptr);
	Ptr<Effect> restored = effects->getAbnormalEffect("SAVED");
	ASSERT_TRUE(restored);
	EXPECT_EQ(restored->getSkillLevel(), 3);
	EXPECT_EQ(restored->getDuration(), 42000);
	EXPECT_EQ(restored->getRemainingTimeToDisplay(), 42000);
	EXPECT_TRUE(restored->isInSuccessEffects(1)) << "addAllEffectToSucess: calculate is never asked";
	EXPECT_EQ(journal, (Journal{"saved.start"}));
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore) << "the partial is closed";

	advance(42000);
	EXPECT_EQ(journal, (Journal{"saved.start", "saved.end"})) << "the restored effect ends with its remaining time";
	EXPECT_TRUE(effects->isEmpty());
}

/**
 * CumulativeResist over PlayerEffectController (PlayerEffectController.java:141-154, CumulativeResist.java): each proc of one type raises the
 * level (up to 5), the duration shrinks by the multiplier of the level BEFORE the proc (1, 1, 0.9, 0.85, 0.8, 0), the resistance follows the
 * level (0, 0, 0, 200, 400, 1000), and the level resets once the clock passes the expiration (duration + the FEAR offset of 2000).
 */
TEST_F(EffectControllerTest, CumulativeResistShortensRepeatedCrowdControlUntilItExpires) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4171);
	Ptr<PlayerEffectController> effects = player->getEffectController();
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::FEAR), 0) << "no entry yet";

	const int64_t durations[] = {10000, 10000, 9000, 8500, 8000, 0};
	const int32_t resistances[] = {0, 0, 200, 400, 1000, 1000};
	for (int i = 0; i < 6; ++i) {
		EXPECT_EQ(effects->calculateAndApplyCumulativeResistDuration(CumulativeResistType::FEAR, 10000), durations[i]) << "proc " << i + 1;
		EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::FEAR), resistances[i]) << "after proc " << i + 1;
	}
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::SLEEP), 0) << "every type counts on its own";

	advance(12000);
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::FEAR), 1000) << "the expiration itself is not past it";
	advance(1);
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::FEAR), 0) << "10000 + 2000 ms after the last proc it resets";
	EXPECT_EQ(effects->calculateAndApplyCumulativeResistDuration(CumulativeResistType::FEAR, 10000), 10000);

	effects->calculateAndApplyCumulativeResistDuration(CumulativeResistType::SLEEP, 4000);
	advance(4000);
	effects->calculateAndApplyCumulativeResistDuration(CumulativeResistType::SLEEP, 4000);
	advance(4001);
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::SLEEP), 0) << "SLEEP has no offset: 4000 ms";

	for (int i = 0; i < 3; ++i)
		effects->calculateAndApplyCumulativeResistDuration(CumulativeResistType::PARALYZE, 1000);
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::PARALYZE), 200);
	effects->removeAllEffects();
	EXPECT_EQ(effects->getCumulativeResistance(CumulativeResistType::PARALYZE), 0) << "death clears the cumulative resistances";
}

/** A FearEffect (or a subclass) of a player on a player takes the cumulative multiplier into its duration unless it is noresist (Effect.java:912-924) */
class ProbeFear final : public effect::FearEffect {
public:
	void applyEffect(model::Effect&) const override {}
	void calculate(model::Effect&) const override {}
	void startEffect(model::Effect&) const override {}
	void endEffect(model::Effect&) const override {}
	using FearEffect::duration2;
	using FearEffect::noResist;
	using FearEffect::position;
};

TEST_F(EffectControllerTest, AFearOnAPlayerTakesTheCumulativeMultiplierIntoItsDuration) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(4181);
	Ref<Player> target = makePlayer(4182);
	Ref<Npc> npc = makeNpc(700381);
	auto fearSkill = [&](int32_t skillId, bool noResist) {
		auto fear = std::make_unique<ProbeFear>();
		fear->position = 1;
		fear->duration2 = 10000;
		fear->noResist = noResist;
		std::vector<std::unique_ptr<effect::EffectTemplate>> list;
		list.push_back(std::move(fear));
		return keep(skillWithProbes(skillXml(skillId, "FEAR" + std::to_string(skillId), R"(tslot="DEBUFF")"), std::move(list)));
	};
	const model::SkillTemplate* fear = fearSkill(9381, false);
	const model::SkillTemplate* unresistable = fearSkill(9382, true);
	auto durationOf = [&](Creature& effector, const model::SkillTemplate* skill) {
		Ref<Effect> effect = Effect::create(effector, target, skill, 1);
		effect->addAllEffectToSucess();
		effect->startEffect();
		int32_t value = effect->getDuration();
		effect->endEffect();
		return value;
	};

	EXPECT_EQ(durationOf(*caster, fear), 10000) << "level 0 -> 1";
	EXPECT_EQ(durationOf(*caster, fear), 10000) << "level 1 -> 2";
	EXPECT_EQ(durationOf(*caster, fear), 9000) << "level 2: 0.9";
	EXPECT_EQ(durationOf(*caster, unresistable), 10000) << "noresist takes no multiplier (and raises no level)";
	EXPECT_EQ(durationOf(*caster, fear), 8500) << "level 3: 0.85";
	EXPECT_EQ(durationOf(*npc, fear), 10000) << "an npc effector does not count";
}

// ---- modifiers (skillengine/effect/modifier/*.java) -----------------------------------------------------------------------------------------

/**
 * The five action modifiers: analyze is value + skillLevel * delta, where the target class/race ones answer 0 for another class/race; check is
 * the condition EffectTemplate.getActionModifiers asks (behind, in front, an abnormal state, the class, the race).
 */
TEST_F(EffectControllerTest, TheActionModifiersAnalyzeTheirValueAndCheckTheirCondition) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(4191, gameserver::model::PlayerClass::WARRIOR, gameserver::model::Race::ELYOS, 495, 500);
	Ref<Player> mage = makePlayer(4192, gameserver::model::PlayerClass::MAGE, gameserver::model::Race::ASMODIANS, 500, 500);
	Ref<Npc> asmodianNpc = makeNpc(700391, "ASMODIANS", 500, 505);
	Ref<Npc> elyosNpc = makeNpc(700392, "ELYOS", 500, 505);
	xml::LoadContext context;
	const model::SkillTemplate* skill = keep(xml::bindString<model::SkillTemplate>(context,
		R"(<skill_template skill_id="9391" name="modified" nameId="1" stack="MODIFIED" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK")"
		R"( tslot="NONE" activation="ACTIVE" duration="0"><effects><skillatk value="10" e="1"><modifiers>)"
		R"(<backdamage value="10" delta="2"/><frontdamage value="5" delta="1"/><abnormaldamage value="7" delta="3" state="ROOT"/>)"
		R"(<targetclass value="9" delta="1" class="MAGE"/><targetrace value="4" delta="2" race="ASMODIANS"/>)"
		R"(</modifiers></skillatk></effects></skill_template>)"));
	const auto& modifiers = skill->getEffectTemplate(1)->getModifiers()->getActionModifiers();
	ASSERT_EQ(modifiers.size(), 5u);
	const effect::modifier::ActionModifier& back = *modifiers[0];
	const effect::modifier::ActionModifier& front = *modifiers[1];
	const effect::modifier::ActionModifier& abnormal = *modifiers[2];
	const effect::modifier::ActionModifier& targetClass = *modifiers[3];
	const effect::modifier::ActionModifier& targetRace = *modifiers[4];

	Ref<Effect> onMage = Effect::create(*caster, mage, skill, 3);
	EXPECT_EQ(back.analyze(*onMage), 16) << "10 + 3 * 2";
	EXPECT_EQ(front.analyze(*onMage), 8) << "5 + 3 * 1";
	EXPECT_EQ(abnormal.analyze(*onMage), 16) << "7 + 3 * 3";
	EXPECT_EQ(targetClass.analyze(*onMage), 12) << "9 + 3 * 1 on a MAGE";
	EXPECT_EQ(targetRace.analyze(*onMage), 10) << "4 + 3 * 2 on an Asmodian";
	EXPECT_TRUE(back.check(*onMage)) << "the caster at x 495 stands behind the mage at x 500 facing heading 0 (+x)";
	EXPECT_FALSE(front.check(*onMage));
	EXPECT_TRUE(targetClass.check(*onMage));
	EXPECT_TRUE(targetRace.check(*onMage));
	EXPECT_FALSE(abnormal.check(*onMage));
	mage->getEffectController()->setAbnormal(AbnormalState::ROOT);
	EXPECT_TRUE(abnormal.check(*onMage));

	Ref<Effect> onCaster = Effect::create(*mage, caster, skill, 3);
	EXPECT_EQ(targetClass.analyze(*onCaster), 0) << "a WARRIOR";
	EXPECT_FALSE(targetClass.check(*onCaster));
	EXPECT_EQ(targetRace.analyze(*onCaster), 0) << "an Elyos";
	EXPECT_FALSE(targetRace.check(*onCaster));
	EXPECT_TRUE(front.check(*onCaster)) << "the mage at x 500 stands in front of the caster facing +x";

	Ref<Effect> onAsmodianNpc = Effect::create(*caster, asmodianNpc, skill, 1);
	Ref<Effect> onElyosNpc = Effect::create(*caster, elyosNpc, skill, 1);
	EXPECT_EQ(targetRace.analyze(*onAsmodianNpc), 6) << "4 + 1 * 2 on an npc of the race";
	EXPECT_TRUE(targetRace.check(*onAsmodianNpc));
	EXPECT_EQ(targetRace.analyze(*onElyosNpc), 0);
	EXPECT_FALSE(targetRace.check(*onElyosNpc));
	EXPECT_EQ(targetClass.analyze(*onAsmodianNpc), 0) << "an npc has no class";
	EXPECT_FALSE(targetClass.check(*onAsmodianNpc));
}

// ---- NpcSkillTemplateEntry (NpcSkillTemplateEntry.java:98-162) ---------------------------------------------------------------------------------

/** The npc skill templates of one npc, bound from an <npc_skill_templates> document the test keeps */
class NpcSkillTemplatesHolder {
public:
	explicit NpcSkillTemplatesHolder(const std::string& skillsXml)
		: data(xml::bindString<dataholders::NpcSkillData>(context, R"(<npc_skill_templates><npc_skills npc_ids="700900">)" + skillsXml
				+ R"(</npc_skills></npc_skill_templates>)")) {}
	Ref<gameserver::model::skill::NpcSkillTemplateEntry> entry(size_t index) const {
		return gameserver::model::skill::NpcSkillTemplateEntry::create(data->getNpcSkillList(700900)->getNpcSkills().at(index));
	}

private:
	xml::LoadContext context;
	std::unique_ptr<dataholders::NpcSkillData> data;
};

/**
 * conditionReady (NpcSkillTemplateEntry.java:98-129): no <cond> or NONE is ready; the target conditions read the npc's current target - its
 * kind, class, abnormal states (the effect controller's bits), flying state and range - and are false for no target, except the two arms that
 * dereference it unchecked (TARGET_IS_GATE, TARGET_IS_IN_RANGE), which throw Java's NullPointerException.
 */
TEST_F(EffectControllerTest, AnNpcSkillConditionReadsTheCurrentTarget) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700401, {}, 500, 500);
	Ref<Player> mage = makePlayer(4201, gameserver::model::PlayerClass::MAGE, gameserver::model::Race::ELYOS, 505, 500);
	Ref<Player> warrior = makePlayer(4202, gameserver::model::PlayerClass::WARRIOR, gameserver::model::Race::ELYOS, 530, 500);
	Ref<Npc> otherNpc = makeNpc(700402, {}, 503, 500);
	const NpcSkillTemplatesHolder holder(R"(<npc_skill id="1" lv="1"/>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="NONE"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_PLAYER"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_NPC"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_MAGICAL_CLASS"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_PHYSICAL_CLASS"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_STUNNED"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_IN_ANY_STUN"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_FLYING"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_IN_RANGE" range="10"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_GATE"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="NPC_IS_ALIVE" npc_id="700999"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_SLEEPING"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_POISONED"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_BLEEDING"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_AETHERS_HOLD"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><cond cond_type="TARGET_IS_IN_STUMBLE"/></npc_skill>)");
	enum : size_t {
		NO_COND, NONE, PLAYER, NPC, MAGICAL, PHYSICAL, STUNNED, ANY_STUN, FLYING, IN_RANGE, GATE, NPC_ALIVE,
		SLEEPING, POISONED, BLEEDING, AETHERS_HOLD, IN_STUMBLE
	};

	EXPECT_TRUE(holder.entry(NO_COND)->conditionReady(*npc)) << "no <cond>";
	EXPECT_TRUE(holder.entry(NONE)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(PLAYER)->conditionReady(*npc)) << "no target";
	EXPECT_FALSE(holder.entry(STUNNED)->conditionReady(*npc)) << "no target";
	EXPECT_FALSE(holder.entry(SLEEPING)->conditionReady(*npc)) << "no target";
	EXPECT_THROW(holder.entry(IN_RANGE)->conditionReady(*npc), runtime::NullPointerException) << "Java dereferences the missing target";
	EXPECT_THROW(holder.entry(GATE)->conditionReady(*npc), runtime::NullPointerException);

	npc->setTarget(mage);
	EXPECT_TRUE(holder.entry(PLAYER)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(NPC)->conditionReady(*npc));
	EXPECT_TRUE(holder.entry(MAGICAL)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(PHYSICAL)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(STUNNED)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(FLYING)->conditionReady(*npc));
	EXPECT_TRUE(holder.entry(IN_RANGE)->conditionReady(*npc)) << "5 m away, range 10";
	EXPECT_FALSE(holder.entry(GATE)->conditionReady(*npc)) << "a player has no npc template";
	EXPECT_FALSE(holder.entry(NPC_ALIVE)->conditionReady(*npc)) << "no npc 700999 in the instance";
	mage->getEffectController()->setAbnormal(AbnormalState::STUMBLE);
	EXPECT_FALSE(holder.entry(STUNNED)->conditionReady(*npc)) << "STUMBLE is no STUN";
	EXPECT_TRUE(holder.entry(ANY_STUN)->conditionReady(*npc)) << "but one of ANY_STUN";
	EXPECT_TRUE(holder.entry(IN_STUMBLE)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(AETHERS_HOLD)->conditionReady(*npc)) << "STUMBLE is no OPENAERIAL";
	mage->getEffectController()->setAbnormal(AbnormalState::STUN);
	EXPECT_TRUE(holder.entry(STUNNED)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(SLEEPING)->conditionReady(*npc)) << "STUN is no SLEEP";
	mage->getEffectController()->setAbnormal(AbnormalState::BLEED);
	EXPECT_TRUE(holder.entry(BLEEDING)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(POISONED)->conditionReady(*npc)) << "BLEED is no POISON";
	mage->getEffectController()->setAbnormal(AbnormalState::POISON);
	EXPECT_TRUE(holder.entry(POISONED)->conditionReady(*npc));
	mage->getEffectController()->setAbnormal(AbnormalState::SLEEP);
	EXPECT_TRUE(holder.entry(SLEEPING)->conditionReady(*npc));
	mage->getEffectController()->setAbnormal(AbnormalState::OPENAERIAL);
	EXPECT_TRUE(holder.entry(AETHERS_HOLD)->conditionReady(*npc));

	npc->setTarget(warrior);
	EXPECT_TRUE(holder.entry(PHYSICAL)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(MAGICAL)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(IN_RANGE)->conditionReady(*npc)) << "30 m away, range 10";
	npc->setTarget(otherNpc);
	EXPECT_TRUE(holder.entry(NPC)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(PLAYER)->conditionReady(*npc));
	EXPECT_FALSE(holder.entry(GATE)->conditionReady(*npc)) << "an npc that is no DOOR";
	EXPECT_FALSE(holder.entry(MAGICAL)->conditionReady(*npc)) << "an npc has no class";

	// NPC_IS_ALIVE asks the npcs of the id in the npc's map instance for a living one (NpcSkillTemplateEntry.java:128)
	Ref<Npc> watched = makeNpc(700999, {}, 520, 500);
	mapInstance->addObject(*watched);
	EXPECT_TRUE(holder.entry(NPC_ALIVE)->conditionReady(*npc)) << "npc 700999 lives in the instance";
	watched->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(watched->isDead());
	EXPECT_FALSE(holder.entry(NPC_ALIVE)->conditionReady(*npc)) << "a dead one does not count";
	mapInstance->removeObject(*watched);
}

/**
 * The carved-signet conditions (NpcSkillTemplateEntry.java:123-127, 133-148): the npc skill's SignetBurstEffect names a signet stack, and the
 * condition holds while the target carries an effect of that stack whose skill level is above the condition's level (0 for
 * TARGET_HAS_CARVED_SIGNET, 1 for LEVEL_II, 2 for LEVEL_III); a dead target carries none.
 */
TEST_F(EffectControllerTest, ACarvedSignetConditionNeedsASignetAboveItsLevel) {
	EFFECT_TEST_SCOPE;
	publishSkillData(R"(<skill_template skill_id="9401" name="burst" nameId="1" stack="BURST" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
					 R"( tslot="NONE" activation="ACTIVE" duration="0"><effects><signetburst signetlvl="5" signet="SIGNET_TEST" value="1" e="1"/>)"
					 R"(</effects></skill_template>)");
	Ref<Npc> npc = makeNpc(700421, {}, 500, 500);
	Ref<Npc> target = makeNpc(700422, {}, 505, 500);
	Journal journal;
	const model::SkillTemplate* signet = keep(skillWithProbes(skillXml(9402, "SIGNET_TEST", R"(tslot="DEBUFF")"), idProbe("signet", &journal, 0, 0)));
	const NpcSkillTemplatesHolder holder(R"(<npc_skill id="9401" lv="1"><cond cond_type="TARGET_HAS_CARVED_SIGNET"/></npc_skill>)"
										 R"(<npc_skill id="9401" lv="1"><cond cond_type="TARGET_HAS_CARVED_SIGNET_LEVEL_II"/></npc_skill>)"
										 R"(<npc_skill id="9401" lv="1"><cond cond_type="TARGET_HAS_CARVED_SIGNET_LEVEL_III"/></npc_skill>)");
	enum : size_t { SIGNET_I, SIGNET_II, SIGNET_III };
	npc->setTarget(target);

	EXPECT_FALSE(holder.entry(SIGNET_I)->conditionReady(*npc)) << "no signet on the target";
	Ref<Effect> first = addTo(*npc, *target, signet, 1);
	ASSERT_EQ(target->getEffectController()->getAbnormalEffect("SIGNET_TEST").get(), first.get());
	EXPECT_TRUE(holder.entry(SIGNET_I)->conditionReady(*npc)) << "skill level 1 > 0";
	EXPECT_FALSE(holder.entry(SIGNET_II)->conditionReady(*npc)) << "skill level 1 is not above 1";
	Ref<Effect> second = addTo(*npc, *target, signet, 2);
	EXPECT_TRUE(holder.entry(SIGNET_II)->conditionReady(*npc)) << "skill level 2 > 1";
	EXPECT_FALSE(holder.entry(SIGNET_III)->conditionReady(*npc)) << "skill level 2 is not above 2";
	target->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(target->isDead());
	EXPECT_FALSE(holder.entry(SIGNET_I)->conditionReady(*npc)) << "a dead target";
	target->getEffectController()->removeAllEffects(true);
}

/**
 * HELP_FRIEND (NpcSkillTemplateEntry.java:97-109): the npc makes a known, visible, living creature it supports (the same tribe,
 * TribeRelationService.isSupport) its target when that one's HP percentage is at most hp_below and it is within range; otherwise the condition
 * fails and the target stays.
 */
TEST_F(EffectControllerTest, HelpFriendTargetsAFriendAtOrBelowTheHpThreshold) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700431, {}, 500, 500);
	Ref<Npc> friendNpc = makeNpc(700432, {}, 505, 500);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *friendNpc));
	const NpcSkillTemplatesHolder holder(R"(<npc_skill id="1" lv="1"><cond cond_type="HELP_FRIEND" hp_below="50" range="10"/></npc_skill>)");

	EXPECT_FALSE(holder.entry(0)->conditionReady(*npc)) << "the friend has all its HP";
	EXPECT_EQ(npc->getTarget(), nullptr);
	friendNpc->getLifeStats()->setCurrentHp(1262);
	ASSERT_EQ(friendNpc->getLifeStats()->getHpPercentage(), 50) << "(int) (100f * 1262 / 2522)";
	EXPECT_TRUE(holder.entry(0)->conditionReady(*npc)) << "50 % is at most hp_below 50";
	EXPECT_EQ(npc->getTarget().get(), friendNpc.get()) << "the friend became the target";
}

/**
 * spawnNpc's count (NpcSkillTemplateEntry.java:164-165) is Rnd.get(min_count, max_count) only for a max_count above 1, min_count otherwise; the
 * delayed spawn asks again whether the npc lives when it runs (:158-161). With min_count 0 no npc is ever spawned, so the random draw is what
 * shows whether spawnNpc counted: none for max_count 1, one for max_count 2, none for a delayed spawn whose npc died meanwhile.
 */
TEST_F(EffectControllerTest, AnNpcSkillSpawnDrawsItsCountOnlyForAMaxCountAboveOneAndALivingNpc) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700441);
	const NpcSkillTemplatesHolder holder(R"(<npc_skill id="1" lv="1"><spawn_npc npc_id="700999" min_count="0" max_count="1"/></npc_skill>)"
										 R"(<npc_skill id="1" lv="1"><spawn_npc npc_id="700999" min_count="0" max_count="2" delay="3000"/></npc_skill>)");
	// a seed whose first draw makes every count 0 even where the port would draw wrongly (Rnd.get(0, 1) and Rnd.get(0, 2) both answer 0)
	uint64_t seed = 20260923;
	for (;; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		const int32_t upToOne = Rnd::get(0, 1);
		Rnd::seedCurrentThreadForTests(seed);
		if (upToOne == 0 && Rnd::get(0, 2) == 0)
			break;
	}
	Rnd::seedCurrentThreadForTests(seed);
	const int32_t undrawn = Rnd::nextInt(1000000);
	auto drewAny = [&] { return Rnd::nextInt(1000000) != undrawn; };

	Rnd::seedCurrentThreadForTests(seed);
	holder.entry(0)->fireOnEndCastEvents(*npc);
	EXPECT_FALSE(drewAny()) << "max_count 1: the count is min_count, no draw";

	holder.entry(1)->fireOnEndCastEvents(*npc);
	Rnd::seedCurrentThreadForTests(seed);
	advance(3000);
	EXPECT_TRUE(drewAny()) << "max_count 2: the delayed spawn drew its count";

	holder.entry(1)->fireOnEndCastEvents(*npc);
	npc->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(npc->isDead());
	Rnd::seedCurrentThreadForTests(seed);
	advance(3000);
	EXPECT_FALSE(drewAny()) << "the npc died before the delay was over: no spawn, no draw";
}

/**
 * fireOnEndCastEvents (NpcSkillTemplateEntry.java:150-162): nothing without a <spawn_npc>, nothing for a dead npc; a delayed spawn is a task
 * that checks again when it runs and spawns nothing for an npc that died meanwhile.
 */
TEST_F(EffectControllerTest, AnNpcSkillSpawnWaitsForItsDelayAndSkipsADeadNpc) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700411);
	const NpcSkillTemplatesHolder holder(R"(<npc_skill id="1" lv="1"/>)"
										 R"(<npc_skill id="1" lv="1"><spawn_npc npc_id="700999" delay="3000"/></npc_skill>)");
	const size_t pendingBefore = executor->pendingTaskCount();

	holder.entry(0)->fireOnEndCastEvents(*npc);
	EXPECT_EQ(executor->pendingTaskCount(), pendingBefore) << "no <spawn_npc>";
	holder.entry(1)->fireOnEndCastEvents(*npc);
	EXPECT_EQ(executor->pendingTaskCount(), pendingBefore + 1) << "the delayed spawn is scheduled";

	npc->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(npc->isDead());
	holder.entry(1)->fireOnEndCastEvents(*npc);
	EXPECT_EQ(executor->pendingTaskCount(), pendingBefore + 1) << "a dead npc schedules nothing";
	advance(3000);
	EXPECT_EQ(executor->pendingTaskCount(), pendingBefore) << "the task ran and, the npc being dead, spawned nothing";
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
