// P5-02b, M5b-2 stage 1 part 2, work items K-02 and K-07 (m5b2-plan.md §5): the rules of EffectController (EffectController.java:39-773) and
// PlayerEffectController (PlayerEffectController.java:36-154) that EffectControllerTest.cpp leaves open - the limits of addEffect (extra effects,
// NOSHOW toggles and auras, chants, cooldown ids, archer buffs), the dispel filters, the removal helpers, the stand-up rule of setAbnormal, the
// broadcasts (SM_ABNORMAL_EFFECT to the players who know the owner, SM_ABNORMAL_STATE and SM_SKILL_ACTIVATION to the player itself) and the
// ABYSSXFORM_LOGOUT arm of addSavedEffect.
//
// The effects are real Effects between spawned creatures of a real map instance whose skills carry ProbeEffect templates (EffectTestSupport.h);
// the packets are read back from a real AionConnection whose IO never started (tests/cm_ak/InWorldPacketRunSupport.h, as the cast lane's
// CastTestSupport.h does). Expectations are derived by hand from the Java methods cited per case.

#include "../../cm_ak/InWorldPacketRunSupport.h"

#include "EffectTestSupport.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_ACTIVATION.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/TransformEffect.h"
#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"
#include "aion/gameserver/skillengine/model/TransformType.h"

namespace aion::gameserver::skillengine::effecttest {
namespace {

namespace cp = network::aion::clientpackets::testing;
using controllers::effect::EffectController;
using controllers::effect::PlayerEffectController;
using effect::AbnormalState;
using model::DispelCategoryType;
using model::Effect;

class EffectControllerRulesTest : public EffectWorldTest {};

/** One probe at position 1 with a duration of 60 s and the given effect id and basic level (0: no id, never conflicts by id) */
std::vector<std::unique_ptr<effect::EffectTemplate>> longProbe(std::string name, Journal* journal, int32_t effectId = 0, int32_t basicLvl = 0) {
	auto p = probe(std::move(name), journal, 1);
	p->effectid = effectId;
	p->basicLvl = basicLvl;
	p->duration2 = 60000;
	return probeList(std::move(p));
}

/** Java Skill.endCast's `new Effect(...); effect.initialize()` and applyEffect's addToEffectedController for one effected creature */
Ref<Effect> addTo(Creature& caster, Creature& target, const model::SkillTemplate* skill) {
	Ref<Effect> effect = Effect::create(caster, Ptr<Creature>(target), skill, 1);
	effect->initialize();
	effect->addToEffectedController();
	return effect;
}

/** The same with a forced duration (Java `new Effect(effector, effected, template, level, duration, null)`) */
Ref<Effect> addFor(Creature& caster, Creature& target, const model::SkillTemplate* skill, int32_t duration) {
	Ref<Effect> effect = Effect::create(caster, Ptr<Creature>(target), skill, 1, duration, nullptr);
	effect->initialize();
	effect->addToEffectedController();
	return effect;
}

/** A NOSHOW toggle: skillsubtype CHANT makes it an aura (EffectController.java:213-219) */
std::string toggleXml(int32_t skillId, std::string_view stack, std::string_view subType) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="toggle" nameId="1" stack=")" + std::string(stack)
		+ R"(" lvl="1" skilltype="MAGICAL" skillsubtype=")" + std::string(subType) + R"(" tslot="NOSHOW" activation="TOGGLE" duration="0"/>)";
}

/** Whether a captured packet is a P: its header is [H encodeServerPacketOpcode(opcode)][C 0x44][H ~encoded] (AionServerPacket::writeOP) */
template <class P>
bool isPacket(const std::vector<uint8_t>& bytes) {
	if (bytes.size() < 2)
		return false;
	int32_t encoded = static_cast<int32_t>(static_cast<uint16_t>(bytes[0] | bytes[1] << 8));
	return encoded == (network::Crypt::encodeServerPacketOpcode(network::aion::opcodeOf<P>) & 0xFFFF);
}

/** The captured packets of type P since the connection was last cleared, in order */
template <class P>
std::vector<std::vector<uint8_t>> packetsOf(cp::RecordingAionConnection& connection) {
	std::vector<std::vector<uint8_t>> result;
	for (const std::vector<uint8_t>& bytes : connection.sentBytes())
		if (isPacket<P>(bytes))
			result.push_back(bytes);
	return result;
}

// ---- the limits of addEffect (EffectController.java:39-101, 201-263) -------------------------------------------------------------------------

/**
 * checkExtraEffect (EffectController.java:201-211, from searchConflict :126-127): a new effect of dispel category EXTRA ends the extra effect the
 * owner already has, unless that one's stack starts with IDSEAL_BOSS_VRITRA_BUFF; an effect of another category ends none.
 */
TEST_F(EffectControllerRulesTest, AnExtraEffectEndsTheExtraEffectBeforeIt) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4401);
	Journal journal;
	auto extra = [&](int32_t skillId, std::string stack, std::string name) {
		return keep(skillWithProbes(skillXml(skillId, stack, R"(tslot="BUFF" dispel_category="EXTRA")"), longProbe(std::move(name), &journal)));
	};
	const model::SkillTemplate* first = extra(9401, "EXTRA_1", "first");
	const model::SkillTemplate* second = extra(9402, "EXTRA_2", "second");
	const model::SkillTemplate* vritra = extra(9403, "IDSEAL_BOSS_VRITRA_BUFF_1", "vritra");
	const model::SkillTemplate* third = extra(9404, "EXTRA_3", "third");
	const model::SkillTemplate* plain = keep(skillWithProbes(skillXml(9405, "NOT_EXTRA", R"(tslot="BUFF")"), longProbe("plain", &journal)));

	addTo(*player, *player, first);
	addTo(*player, *player, plain);
	EXPECT_EQ(journal, (Journal{"first.calculate", "first.start", "plain.calculate", "plain.start"})) << "a plain effect ends no extra one";
	journal.clear();
	addTo(*player, *player, second);
	EXPECT_EQ(journal, (Journal{"second.calculate", "first.end", "second.start"})) << "the second extra effect ended the first";
	journal.clear();
	addTo(*player, *player, vritra);
	EXPECT_EQ(journal, (Journal{"vritra.calculate", "second.end", "vritra.start"}));
	journal.clear();
	addTo(*player, *player, third);
	EXPECT_EQ(journal, (Journal{"third.calculate", "third.start"})) << "the IDSEAL_BOSS_VRITRA_BUFF extra effect is not ended";
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9403));
	player->getEffectController()->removeAllEffects(true);
}

/**
 * The NOSHOW toggles (EffectController.java:76-87): an effector keeps one toggle and three auras (the CHANT subtype), a Ranger or a Rider two of
 * either kind; one more ends the oldest of its kind.
 */
TEST_F(EffectControllerRulesTest, ANoShowToggleEndsTheOldestBeyondTheEffectorsLimit) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = makePlayer(4411, gameserver::model::PlayerClass::MAGE);
	Ref<Player> ranger = makePlayer(4412, gameserver::model::PlayerClass::RANGER);
	Journal journal;
	auto toggle = [&](int32_t skillId, std::string name, std::string_view subType) {
		return keep(skillWithProbes(toggleXml(skillId, "TOGGLE_" + name, subType), longProbe(name, &journal)));
	};

	addTo(*mage, *mage, toggle(9411, "t1", "BUFF"));
	journal.clear();
	addTo(*mage, *mage, toggle(9412, "t2", "BUFF"));
	EXPECT_EQ(journal, (Journal{"t2.calculate", "t1.end", "t2.start"})) << "one toggle";
	journal.clear();
	addTo(*mage, *mage, toggle(9413, "a1", "CHANT"));
	addTo(*mage, *mage, toggle(9414, "a2", "CHANT"));
	addTo(*mage, *mage, toggle(9415, "a3", "CHANT"));
	EXPECT_EQ(journal, (Journal{"a1.calculate", "a1.start", "a2.calculate", "a2.start", "a3.calculate", "a3.start"})) << "three auras";
	journal.clear();
	addTo(*mage, *mage, toggle(9416, "a4", "CHANT"));
	EXPECT_EQ(journal, (Journal{"a4.calculate", "a1.end", "a4.start"})) << "the fourth aura ends the first";

	journal.clear();
	addTo(*ranger, *ranger, toggle(9417, "r1", "BUFF"));
	addTo(*ranger, *ranger, toggle(9418, "r2", "BUFF"));
	EXPECT_EQ(journal, (Journal{"r1.calculate", "r1.start", "r2.calculate", "r2.start"})) << "a Ranger keeps two toggles";
	journal.clear();
	addTo(*ranger, *ranger, toggle(9419, "r3", "BUFF"));
	EXPECT_EQ(journal, (Journal{"r3.calculate", "r1.end", "r3.start"}));
	mage->getEffectController()->removeAllEffects(true);
	ranger->getEffectController()->removeAllEffects(true);
}

/** At most four chants (target slot CHANT, EffectController.java:89-94); cooldown id 1 keeps checkEffectCooldownId out of it (:225-226) */
TEST_F(EffectControllerRulesTest, AFifthChantEndsTheFirst) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4421);
	Journal journal;
	for (int32_t i = 1; i <= 4; ++i)
		addTo(*player, *player, keep(skillWithProbes(skillXml(9420 + i, "CHANT_" + std::to_string(i), R"(tslot="CHANT" cooldownId="1")"),
									longProbe("c" + std::to_string(i), &journal))));
	EXPECT_EQ(journal.size(), 8u) << "four chants, none ended";
	journal.clear();
	addTo(*player, *player, keep(skillWithProbes(skillXml(9425, "CHANT_5", R"(tslot="CHANT" cooldownId="1")"), longProbe("c5", &journal))));
	EXPECT_EQ(journal, (Journal{"c5.calculate", "c1.end", "c5.start"}));
	player->getEffectController()->removeAllEffects(true);
}

/**
 * checkEffectCooldownId (EffectController.java:221-263): one effect per slot and cooldown id, but two for the cooldown ids 273 (Erosion &
 * Flamecage) and 353 (Lockdown & Dazing Severe Blow), and a NOSHOW effect is exempt; of the archer buffs (cooldown ids 2020..2030) two may run
 * together, a third ends the first of them.
 */
TEST_F(EffectControllerRulesTest, CooldownIdsLimitTheEffectsPerSlot) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4431);
	Ref<Player> archer = makePlayer(4432, gameserver::model::PlayerClass::RANGER);
	Journal journal;
	auto withCooldown = [&](int32_t skillId, std::string name, std::string_view slot, int32_t cooldownId) {
		return keep(skillWithProbes(skillXml(skillId, "CD_" + name, R"(tslot=")" + std::string(slot) + R"(" cooldownId=")" + std::to_string(cooldownId) + R"(")"),
			longProbe(name, &journal)));
	};

	addTo(*player, *player, withCooldown(9431, "e1", "DEBUFF", 273));
	addTo(*player, *player, withCooldown(9432, "e2", "DEBUFF", 273));
	journal.clear();
	addTo(*player, *player, withCooldown(9433, "e3", "DEBUFF", 273));
	EXPECT_EQ(journal, (Journal{"e3.calculate", "e1.end", "e3.start"})) << "cooldown id 273: the third ends the first";
	addTo(*player, *player, withCooldown(9434, "l1", "DEBUFF", 353));
	addTo(*player, *player, withCooldown(9435, "l2", "DEBUFF", 353));
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9434)) << "cooldown id 353 allows two";
	addTo(*player, *player, withCooldown(9436, "n1", "NOSHOW", 77));
	addTo(*player, *player, withCooldown(9437, "n2", "NOSHOW", 77));
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9436)) << "NOSHOW effects are exempt";
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9437));

	journal.clear();
	addTo(*archer, *archer, withCooldown(9438, "dodging", "BUFF", 2020));
	addTo(*archer, *archer, withCooldown(9439, "focused", "BUFF", 2022));
	EXPECT_EQ(journal, (Journal{"dodging.calculate", "dodging.start", "focused.calculate", "focused.start"})) << "two archer buffs";
	journal.clear();
	addTo(*archer, *archer, withCooldown(9440, "aiming", "BUFF", 2024));
	EXPECT_EQ(journal, (Journal{"aiming.calculate", "dodging.end", "aiming.start"})) << "the third ends the first";
	player->getEffectController()->removeAllEffects(true);
	archer->getEffectController()->removeAllEffects(true);
}

// ---- dispel (EffectController.java:431-617) -------------------------------------------------------------------------------------------------

/**
 * removeByDispelEffect (EffectController.java:450-485): a STUN dispel skips skill 11904 (the avatar skill Remove Shock may not remove); a
 * req_dispel_level equal to the dispel level is dispelled (only `>` skips); and every matching effect costs one of `count`, also one whose power
 * the dispel only weakened (:474-477) - a dispel of count 1 that weakens the first match leaves the second alone.
 */
TEST_F(EffectControllerRulesTest, RemoveByDispelEffectSkipsRemoveShockAndSpendsItsCountOnEveryMatch) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> stunned = makeNpc(700441);
	Ref<Npc> buffed = makeNpc(700442);
	Ref<Npc> counted = makeNpc(700443);
	Journal journal;
	const model::SkillTemplate* avatarStun =
		keep(skillWithProbes(skillXml(11904, "AVATAR_STUN", R"(tslot="DEBUFF")"), longProbe("avatarStun", &journal), {effect::EffectType::STUN}));
	const model::SkillTemplate* stun = keep(skillWithProbes(skillXml(9441, "STUN", R"(tslot="DEBUFF")"), longProbe("stun", &journal), {effect::EffectType::STUN}));
	const model::SkillTemplate* levelled = keep(skillWithProbes(skillXml(9442, "LEVELLED", R"(tslot="BUFF" req_dispel_level="5")"), longProbe("levelled", &journal)));
	const model::SkillTemplate* strong = keep(skillWithProbes(skillXml(9443, "STRONG", R"(tslot="BUFF" req_dispel_count="150")"), longProbe("strong", &journal)));
	const model::SkillTemplate* weak = keep(skillWithProbes(skillXml(9444, "WEAK", R"(tslot="BUFF" req_dispel_count="10")"), longProbe("weak", &journal)));
	addTo(*stunned, *stunned, avatarStun);
	addTo(*stunned, *stunned, stun);
	addTo(*buffed, *buffed, levelled);
	Ref<Effect> strongEffect = addTo(*counted, *counted, strong);
	addTo(*counted, *counted, weak);
	journal.clear();

	stunned->getEffectController()->removeByDispelEffect(effect::EffectType::STUN, std::nullopt, 5, 255, 100);
	EXPECT_EQ(journal, Journal{"stun.end"}) << "11904 is not removed by a STUN dispel";
	EXPECT_TRUE(stunned->getEffectController()->hasAbnormalEffect(11904));
	journal.clear();
	buffed->getEffectController()->removeByDispelEffect(std::nullopt, model::DispelSlotType::BUFF, 5, 5, 100);
	EXPECT_EQ(journal, Journal{"levelled.end"}) << "req_dispel_level 5 at dispel level 5";
	journal.clear();
	counted->getEffectController()->removeByDispelEffect(std::nullopt, model::DispelSlotType::BUFF, 1, 255, 100);
	EXPECT_EQ(journal, Journal{}) << "count 1 went to the first match, which the dispel only weakened";
	EXPECT_EQ(strongEffect->getPower(), 50) << "150 - 100";
	EXPECT_TRUE(counted->getEffectController()->hasAbnormalEffect(9444)) << "the second match was never reached";
	stunned->getEffectController()->removeAllEffects(true);
	counted->getEffectController()->removeAllEffects(true);
}

/**
 * removeEffectByDispelCat and isDispellable (EffectController.java:528-617): an ALL dispel (DispelDebuffEffect) takes the ALL, DEBUFF_MENTAL and
 * DEBUFF_PHYSICAL debuffs of the slot, not an NPC_DEBUFF_PHYSICAL one; a sanctuary effect and an effect of 24 h or more are not dispellable
 * (`>= 86400000`), 1 ms less is, and so is a 24 h effect of one of isRemovableEffect's skills (20941, :627-647).
 */
TEST_F(EffectControllerRulesTest, TheDispelCategoriesAndTheEffectsNoDispelTakes) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700451);
	Journal journal;
	auto debuff = [&](int32_t skillId, std::string name, std::string_view category) {
		return keep(skillWithProbes(skillXml(skillId, "CAT_" + name, R"(tslot="DEBUFF" req_dispel_level="1" dispel_category=")" + std::string(category) + R"(")"),
			longProbe(name, &journal)));
	};
	auto buff = [&](int32_t skillId, std::string name, std::set<effect::EffectType> types = {}) {
		return keep(skillWithProbes(skillXml(skillId, "DISPEL_" + name, R"(tslot="BUFF" req_dispel_level="1" dispel_category="BUFF")"),
			longProbe(name, &journal), std::move(types)));
	};
	addTo(*npc, *npc, debuff(9451, "all", "ALL"));
	addTo(*npc, *npc, debuff(9452, "mental", "DEBUFF_MENTAL"));
	addTo(*npc, *npc, debuff(9453, "physical", "DEBUFF_PHYSICAL"));
	addTo(*npc, *npc, debuff(9454, "npcPhysical", "NPC_DEBUFF_PHYSICAL"));
	addTo(*npc, *npc, buff(9455, "sanctuary", {effect::EffectType::SANCTUARY}));
	addFor(*npc, *npc, buff(9456, "day"), 86400000);
	addFor(*npc, *npc, buff(9457, "almost"), 86399999);
	addFor(*npc, *npc, buff(20941, "removable"), 86400000);
	Ptr<EffectController> effects = npc->getEffectController();
	journal.clear();

	effects->removeEffectByDispelCat(DispelCategoryType::ALL, model::SkillTargetSlot::DEBUFF, 10, 10, 100);
	EXPECT_EQ(journal, (Journal{"all.end", "mental.end", "physical.end"})) << "not the NPC_DEBUFF_PHYSICAL one";
	journal.clear();
	effects->removeEffectByDispelCat(DispelCategoryType::BUFF, model::SkillTargetSlot::BUFF, 10, 10, 100);
	EXPECT_EQ(journal, (Journal{"almost.end", "removable.end"})) << "neither the sanctuary nor the 24 h effect";
	EXPECT_TRUE(effects->hasAbnormalEffect(9455));
	EXPECT_TRUE(effects->hasAbnormalEffect(9456));
	effects->removeAllEffects(true);
}

/**
 * calculateBuffsOrEffectorDebuffsToRemove (EffectController.java:492-526) designates a DEBUFF-slot effect only when the dispelling effect's
 * effector cast it (:507-509); a debuff of another effector is left undesignated.
 */
TEST_F(EffectControllerRulesTest, TheCounterAttackDispelDesignatesOnlyTheDebuffsOfItsEffector) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> target = makeNpc(700461);
	Ref<Npc> other = makeNpc(700462);
	Ref<Player> dispeller = makePlayer(4461);
	Journal journal;
	const model::SkillTemplate* debuff = keep(skillWithProbes(skillXml(9461, "MINE", R"(tslot="DEBUFF" dispel_category="ALL")"), longProbe("mine", &journal)));
	const model::SkillTemplate* foreign = keep(skillWithProbes(skillXml(9462, "THEIRS", R"(tslot="DEBUFF" dispel_category="ALL")"), longProbe("theirs", &journal)));
	const model::SkillTemplate* dispel = keep(skillWithProbes(skillXml(9463, "COUNTER", R"(tslot="DEBUFF")"), longProbe("dispel", &journal)));
	Ref<Effect> theirs = addTo(*other, *target, foreign);
	Ref<Effect> mine = addTo(*dispeller, *target, debuff);
	Ref<Effect> dispelling = Effect::create(*dispeller, target, dispel, 1);

	EXPECT_EQ(target->getEffectController()->calculateBuffsOrEffectorDebuffsToRemove(*dispelling, 5, 10, 100), 1);
	EXPECT_EQ(mine->getDesignatedDispelEffect().get(), dispelling.get());
	EXPECT_EQ(theirs->getDesignatedDispelEffect(), nullptr) << "another effector's debuff";
	target->getEffectController()->resetDesignatedDispelEffect(*dispelling);
	target->getEffectController()->removeAllEffects(true);
}

// ---- removal helpers, the slot filter and setAbnormal (EffectController.java:340-367, 431-444, 695-718) --------------------------------------

/** A TransformEffect of the given transform type whose behaviour hooks only record (the real bodies are part 3's) */
class ProbeTransform final : public effect::TransformEffect {
public:
	ProbeTransform(std::string probeName, Journal* probeJournal, model::TransformType transformType)
		: name(std::move(probeName)), journal(probeJournal) {
		type = transformType;
		position = 1;
		duration2 = 60000;
	}
	std::string_view javaClassName() const override { return "ProbeTransform"; }
	void calculate(model::Effect& effect) const override { effect.addSuccessEffect(this); }
	void applyEffect(model::Effect&) const override {}
	void startEffect(model::Effect&) const override {}
	void endEffect(model::Effect&) const override { journal->push_back(name + ".end"); }
	const std::string name;
	Journal* const journal;
};

/**
 * removeByEffectId ends the first effect of the effect id whose req_dispel_level is at most the dispel level (EffectController.java:431-444);
 * removeHideEffects ends the HIDE effects and nothing else (:340-342); removeTransformEffects spares the AVATAR transforms (:359-367); and
 * getAbnormalEffectsToTargetSlot answers the effects of one slot (:695-697).
 */
TEST_F(EffectControllerRulesTest, TheRemovalHelpersAndTheSlotFilterPickTheirEffects) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700471);
	Journal journal;
	Ptr<EffectController> effects = npc->getEffectController();
	const model::SkillTemplate* byId =
		keep(skillWithProbes(skillXml(9471, "BY_ID", R"(tslot="BUFF" req_dispel_level="5" req_dispel_count="1")"), longProbe("byId", &journal, 1234, 1)));
	addTo(*npc, *npc, byId);
	journal.clear();
	EXPECT_FALSE(effects->removeByEffectId(1234, 4, 100)) << "req_dispel_level 5 > 4";
	EXPECT_TRUE(effects->removeByEffectId(1234, 5, 100)) << "req_dispel_level 5 <= 5";
	EXPECT_EQ(journal, Journal{"byId.end"});

	const model::SkillTemplate* hide = keep(skillWithProbes(skillXml(9472, "HIDDEN", R"(tslot="BUFF")"), longProbe("hide", &journal), {effect::EffectType::HIDE}));
	const model::SkillTemplate* stun = keep(skillWithProbes(skillXml(9473, "STUNNED", R"(tslot="DEBUFF")"), longProbe("stun", &journal), {effect::EffectType::STUN}));
	addTo(*npc, *npc, hide);
	addTo(*npc, *npc, stun);
	journal.clear();
	effects->removeHideEffects();
	EXPECT_EQ(journal, Journal{"hide.end"});
	effects->removeAllEffects(true);

	auto transform = [&](int32_t skillId, std::string name, model::TransformType transformType) {
		std::vector<std::unique_ptr<effect::EffectTemplate>> list;
		list.push_back(std::make_unique<ProbeTransform>(name, &journal, transformType));
		return keep(skillWithProbes(skillXml(skillId, "TRANSFORM_" + name, R"(tslot="BUFF")"), std::move(list)));
	};
	addTo(*npc, *npc, transform(9474, "avatar", model::TransformType::AVATAR));
	addTo(*npc, *npc, transform(9475, "form", model::TransformType::FORM1));
	journal.clear();
	effects->removeTransformEffects();
	EXPECT_EQ(journal, Journal{"form.end"}) << "an AVATAR transform stays";
	effects->removeAllEffects(true);

	Ref<Effect> buffEffect = addTo(*npc, *npc, keep(skillWithProbes(skillXml(9476, "SLOT_BUFF", R"(tslot="BUFF")"), longProbe("buff", &journal))));
	addTo(*npc, *npc, keep(skillWithProbes(skillXml(9477, "SLOT_DEBUFF", R"(tslot="DEBUFF")"), longProbe("debuff", &journal))));
	std::vector<Ptr<Effect>> buffs = effects->getAbnormalEffectsToTargetSlot(getId(model::SkillTargetSlot::BUFF));
	ASSERT_EQ(buffs.size(), 1u);
	EXPECT_EQ(buffs[0].get(), buffEffect.get());
	effects->removeAllEffects(true);
}

/** setAbnormal makes a resting player stand up for the states of AUTOMATICALLY_STANDUP (EffectController.java:710-717): STUN does, ROOT does not */
TEST_F(EffectControllerRulesTest, AStunMakesARestingPlayerStandUp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4481);
	using gameserver::model::gameobjects::state::CreatureState;
	player->setState(CreatureState::RESTING);
	ASSERT_TRUE(player->isInState(CreatureState::RESTING));
	player->getEffectController()->setAbnormal(AbnormalState::ROOT);
	EXPECT_TRUE(player->isInState(CreatureState::RESTING)) << "ROOT is not one of AUTOMATICALLY_STANDUP";
	player->getEffectController()->setAbnormal(AbnormalState::STUN);
	EXPECT_FALSE(player->isInState(CreatureState::RESTING)) << "STUN is";
}

// ---- the broadcasts (EffectController.java:95-100, 155-156, 304-325, 660-671; PlayerEffectController.java:37-89) -------------------------------

/**
 * SM_ABNORMAL_EFFECT as a player who knows the npc receives it: addEffect announces the new effect; searchConflict ends a predecessor of the same
 * slot silently (endEffect(false), :156 - the successor's announcement covers the slot) and one of another slot with its own announcement; an
 * effect that ends announces its removal (clearEffect); death ends the effects silently and then announces every slot once.
 */
TEST_F(EffectControllerRulesTest, TheOwnersObserversAreToldWhatChanged) {
	EFFECT_TEST_SCOPE;
	publishTribeRelations(); // the observer's SM_NPC_INFO asks the npc's relation to it (Npc.getType): a tribe of the data
	Ref<Npc> npc = makeNpc(700491, {}, 505, 500, 100, "MONSTER");
	Ref<Player> observer = makePlayer(4491);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *observer));
	cp::TestClient client;
	client.enterWorld(*observer, *accounts.back());
	Journal journal;
	const model::SkillTemplate* first = keep(skillWithProbes(skillXml(9491, "TOLD_1", R"(tslot="BUFF")"), longProbe("first", &journal, 1100, 3), {},
		{effect::EffectType::SHIELD}));
	const model::SkillTemplate* sameSlot = keep(skillWithProbes(skillXml(9492, "TOLD_2", R"(tslot="BUFF")"), longProbe("sameSlot", &journal, 1100, 3), {},
		{effect::EffectType::SHIELD}));
	const model::SkillTemplate* otherSlot = keep(skillWithProbes(skillXml(9493, "TOLD_3", R"(tslot="SPEC")"), longProbe("otherSlot", &journal, 1100, 3),
		{}, {effect::EffectType::SHIELD}));
	const model::SkillTemplate* plain = keep(skillWithProbes(skillXml(9494, "TOLD_4", R"(tslot="DEBUFF")"), longProbe("plain", &journal)));
	(*client).clearSent();
	auto told = [&client] {
		size_t count = packetsOf<network::aion::serverpackets::SM_ABNORMAL_EFFECT>(*client).size();
		(*client).clearSent();
		return count;
	};

	addTo(*npc, *npc, first);
	EXPECT_EQ(told(), 1u) << "addEffect announced the new effect";
	journal.clear();
	addTo(*npc, *npc, sameSlot);
	EXPECT_EQ(journal, (Journal{"sameSlot.calculate", "first.end", "sameSlot.start"}));
	EXPECT_EQ(told(), 1u) << "the predecessor of the same slot ended without an announcement";
	Ref<Effect> last = addTo(*npc, *npc, otherSlot);
	EXPECT_EQ(told(), 2u) << "the predecessor of another slot announced its end, the successor itself";
	last->endEffect();
	EXPECT_EQ(told(), 1u) << "an ended effect is announced";
	addTo(*npc, *npc, plain);
	addTo(*npc, *npc, first);
	told();
	npc->getEffectController()->removeAllEffects();
	EXPECT_EQ(told(), 1u) << "death: silent ends, then one announcement of all slots";
	observer->setClientConnection(nullptr);
}

/**
 * The player itself: PlayerEffectController sends SM_ABNORMAL_STATE after an effect was added and after one ended with a broadcast, but not for a
 * passive effect (updatePlayerIconsAndGroup's filter, PlayerEffectController.java:72); a toggle is shown activated as it starts and deactivated as
 * it ends (SM_SKILL_ACTIVATION, Effect.java:670-672, 734-736).
 */
TEST_F(EffectControllerRulesTest, APlayerIsToldItsIconsAndTogglesButNotItsPassives) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(4501);
	cp::TestClient client;
	client.enterWorld(*player, *accounts.back());
	Journal journal;
	const model::SkillTemplate* buff = keep(skillWithProbes(skillXml(9501, "ICON", R"(tslot="BUFF")"), longProbe("buff", &journal)));
	const model::SkillTemplate* passive = keep(skillWithProbes(R"(<skill_template skill_id="9502" name="passive" nameId="1" stack="NO_ICON" lvl="1")"
															   R"( skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" duration="0"/>)",
		longProbe("passive", &journal)));
	const model::SkillTemplate* toggle = keep(skillWithProbes(toggleXml(9503, "SHOWN_TOGGLE", "BUFF"), longProbe("toggle", &journal)));
	using network::aion::serverpackets::SM_ABNORMAL_STATE;
	using network::aion::serverpackets::SM_SKILL_ACTIVATION;
	(*client).clearSent();

	Ref<Effect> buffEffect = addTo(*player, *player, buff);
	EXPECT_EQ(packetsOf<SM_ABNORMAL_STATE>(*client).size(), 1u) << "addEffect: the icons";
	(*client).clearSent();
	buffEffect->endEffect();
	EXPECT_EQ(packetsOf<SM_ABNORMAL_STATE>(*client).size(), 1u) << "an effect ended with a broadcast: the icons";
	(*client).clearSent();
	Ref<Effect> passiveEffect = addTo(*player, *player, passive);
	ASSERT_EQ(player->getEffectController()->getAllEffects().size(), 1u);
	passiveEffect->endEffect();
	EXPECT_EQ(packetsOf<SM_ABNORMAL_STATE>(*client).size(), 0u) << "a passive effect shows no icon, added or ended";

	Ref<Effect> toggleEffect = addTo(*player, *player, toggle);
	EXPECT_EQ(packetsOf<SM_SKILL_ACTIVATION>(*client), cp::exactly({cp::serialized(SM_SKILL_ACTIVATION(9503, true), client.con())}));
	(*client).clearSent();
	toggleEffect->endEffect();
	EXPECT_EQ(packetsOf<SM_SKILL_ACTIVATION>(*client), cp::exactly({cp::serialized(SM_SKILL_ACTIVATION(9503, false), client.con())}));
	player->setClientConnection(nullptr);
}

// ---- addSavedEffect with ABYSSXFORM_LOGOUT (PlayerEffectController.java:98-120) ---------------------------------------------------------------

/**
 * With CustomConfig.ABYSSXFORM_LOGOUT a deity avatar's saved effect lasts until its stored end time instead of the row's remaining time, and is
 * dropped once that end time has come (`currentTimeMillis() >= endTime`, PlayerEffectController.java:105-111). The end time is wall-clock time
 * (docs/deviations/P5-02b.md), so the boundary is tried a hundred times: nearly every attempt reads the same millisecond as its end time.
 */
TEST_F(EffectControllerRulesTest, ADeityAvatarsSavedEffectLastsUntilItsStoredEndTime) {
	EFFECT_TEST_SCOPE;
	publishSkillData(skillXml(9511, "AVATAR", R"(tslot="BUFF" avatar="true")"));
	Journal journal;
	injectProbes(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9511), probeList(probe("avatar", &journal, 1, ProbeEffect::Calculate::FAIL)));
	Ref<Player> player = makePlayer(4511);
	Ptr<PlayerEffectController> effects = player->getEffectController();
	struct ConfigGuard {
		const bool saved = configs::main::CustomConfig::ABYSSXFORM_LOGOUT.load();
		ConfigGuard() { configs::main::CustomConfig::ABYSSXFORM_LOGOUT.store(true); }
		~ConfigGuard() { configs::main::CustomConfig::ABYSSXFORM_LOGOUT.store(saved); }
	} abyssXformLogout;

	for (int attempt = 0; attempt < 100; ++attempt)
		effects->addSavedEffect(9511, 1, 5000, commons::utils::currentTimeMillis(), nullptr, nullptr);
	EXPECT_TRUE(effects->isEmpty()) << "an end time that has come drops the row, whatever remaining time it has";
	effects->addSavedEffect(9511, 1, 5000, commons::utils::currentTimeMillis() + 30000, nullptr, nullptr);
	Ptr<Effect> restored = effects->getAbnormalEffect("AVATAR");
	ASSERT_TRUE(restored);
	EXPECT_GT(restored->getDuration(), 5000) << "the time left to the end time, not the row's remaining time";
	EXPECT_LE(restored->getDuration(), 30000);
	effects->removeAllEffects(true);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
