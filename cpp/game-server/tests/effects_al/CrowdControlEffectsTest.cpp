// The crowd-control effects of P5-03 / P5-04 (M5b-2's leftovers, ported 2026-10-02 on the owner's request): the buff variants of stun,
// silence, bind and sleep (always a success: their calculate only adds the success effect), PetrificationEffect, DiseaseEffect and
// ConfuseEffect (with its ConfuseTask). Each case binds the data template that uses the effect (skill_templates.xml, cut to its effect; no data
// skill uses <buffsleep>, so its case builds one) and runs it on the DaevaEffectTest fixture: a case that reaches an AION_UNPORTED site fails in
// TearDown, so a green case ran through ported code only.

#include "DaevaEffectsTestSupport.h"

#include <atomic>
#include <cstdint>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using effect::AbnormalState;
using gameserver::model::PlayerClass;
using gameserver::model::stats::container::StatEnum;
using model::Effect;

std::string buffXml(int32_t skillId, std::string_view name, std::string_view effectXml) {
	return templateXml(R"(skill_id=")" + std::to_string(skillId) + R"(" name=")" + std::string(name) + R"(" nameId="1" stack="TEST_CC_)" +
			std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0")",
		effectXml);
}

std::string debuffXml(int32_t skillId, std::string_view name, std::string_view effectXml) {
	return templateXml(R"(skill_id=")" + std::to_string(skillId) + R"(" name=")" + std::string(name) + R"(" nameId="1" stack="TEST_CC_)" +
			std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0")"
			R"( hostile_type="DIRECT")",
		effectXml);
}

// ---- the effects of the data (skill_templates.xml), each on its own template -----------------------------------------------------------------

/** 19580 Barrier of Severance I's stun of itself (BuffStunEffect) */
const std::string BUFF_STUN_XML = buffXml(19580, "Barrier of Severance I", R"(<buffstun duration2="8000" effectid="20200" e="1" basiclvl="200" element="FIRE" />)");
/** 4098 Impervious Veil's silence and bind of itself (BuffSilenceEffect, BuffBindEffect) */
const std::string BUFF_SILENCE_XML = buffXml(4098, "Impervious Veil", R"(<buffsilence duration2="30000" effectid="20202" e="1" element="FIRE" />)");
const std::string BUFF_BIND_XML = buffXml(4099, "Impervious Veil bind", R"(<buffbind duration2="30000" effectid="20203" e="1" element="FIRE" />)");
/** no data skill uses <buffsleep> (BuffSleepEffect): a template like the other buff variants */
const std::string BUFF_SLEEP_XML = buffXml(64101, "buff sleep", R"(<buffsleep duration2="5000" e="1" element="FIRE" />)");
/** 16492 Petrification (PetrificationEffect) */
const std::string PETRIFICATION_XML =
	debuffXml(16492, "Petrification", R"(<petrification value="10" delta="10" duration2="3000" effectid="20004" e="1" noresist="true" element="EARTH" />)");
const std::string PETRIFICATION_RESISTABLE_XML = debuffXml(64102, "Petrification, resistable", R"(<petrification duration2="3000" e="1" element="EARTH" />)");
/** 16557 Disease (DiseaseEffect), as the data has it: no noresist */
const std::string DISEASE_XML = debuffXml(16557, "Disease", R"(<disease duration2="10000" duration1="200" effectid="20009" e="1" element="EARTH" />)");
/** 10506 [Common] Flamethrower's confusion (ConfuseEffect) */
const std::string CONFUSE_XML = debuffXml(10506, "[Common] Flamethrower", R"(<confuse duration2="5000" e="1" noresist="true" element="FIRE" />)");

/** gameserver.geodata.fear.enable for the scope (the confusion's flee task needs it, as the fear's does) */
class FearEnableScope {
public:
	explicit FearEnableScope(bool value) : previous(configs::main::GeoDataConfig::FEAR_ENABLE.load()) {
		configs::main::GeoDataConfig::FEAR_ENABLE.store(value);
	}
	~FearEnableScope() { configs::main::GeoDataConfig::FEAR_ENABLE.store(previous); }
	FearEnableScope(const FearEnableScope&) = delete;
	FearEnableScope& operator=(const FearEnableScope&) = delete;

private:
	const bool previous;
};

using CrowdControlEffectsTest = DaevaEffectTest;

// ---- the buff variants ----------------------------------------------------------------------------------------------------------------------

TEST_F(CrowdControlEffectsTest, TheBuffVariantsAlwaysSucceedAndSetTheirState) {
	EFFECT_TEST_SCOPE;
	struct Case {
		const std::string& xml;
		const char* className;
		AbnormalState state;
		StatEnum resistance;
	};
	const Case cases[] = {
		{BUFF_STUN_XML, "BuffStunEffect", AbnormalState::STUN, StatEnum::STUN_RESISTANCE},
		{BUFF_SILENCE_XML, "BuffSilenceEffect", AbnormalState::SILENCE, StatEnum::SILENCE_RESISTANCE},
		{BUFF_BIND_XML, "BuffBindEffect", AbnormalState::BIND, StatEnum::BIND_RESISTANCE},
		{BUFF_SLEEP_XML, "BuffSleepEffect", AbnormalState::SLEEP, StatEnum::SLEEP_RESISTANCE},
	};
	int32_t id = 8701;
	for (const Case& c : cases) {
		SCOPED_TRACE(c.className);
		const model::SkillTemplate* skill = bindSkill(c.xml);
		ASSERT_EQ(effectOf(*skill, 0).javaClassName(), c.className);
		Ref<Npc> npc = makeMonster(id++);
		addStat(*npc, c.resistance, 1000); // the template has no noresist: the base class's calculate would resist, the buff variant does not roll

		Ref<Effect> effect = cast(*npc, *npc, skill, 1);
		ASSERT_TRUE(effect->isInSuccessEffects(1)) << "calculate: effect.addSuccessEffect(this)";
		EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(c.state));
		effect->endEffect();
		EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(c.state));
	}
}

// ---- petrification and disease ------------------------------------------------------------------------------------------------------------------

TEST_F(CrowdControlEffectsTest, PetrificationStopsTheMonsterUntilItEnds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* petrification = bindSkill(PETRIFICATION_XML);
	ASSERT_EQ(effectOf(*petrification, 0).javaClassName(), "PetrificationEffect");
	Ref<Player> caster = daeva(8711, PlayerClass::SORCERER);
	Ref<Npc> npc = makeMonster(701711, 510, 500);
	npc->getMoveController()->moveToPoint(520, 500, 100);
	ASSERT_TRUE(npc->getMoveController()->isInMove());

	Ref<Effect> effect = cast(*caster, *npc, petrification, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "PetrificationEffect.startEffect: abortMove";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::PETRIFICATION));
	effect->endEffect();
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::PETRIFICATION));
}

TEST_F(CrowdControlEffectsTest, PetrificationAndDiseaseAreResistedByTheirStats) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* petrification = bindSkill(PETRIFICATION_RESISTABLE_XML);
	const model::SkillTemplate* disease = bindSkill(DISEASE_XML);
	ASSERT_EQ(effectOf(*disease, 0).javaClassName(), "DiseaseEffect");
	Ref<Player> caster = daeva(8712, PlayerClass::SORCERER);
	Ref<Npc> stoneResistant = makeMonster(701712, 505, 500);
	addStat(*stoneResistant, StatEnum::PERIFICATION_RESISTANCE, 1000);
	Ref<Npc> diseaseResistant = makeMonster(701713, 505, 505);
	addStat(*diseaseResistant, StatEnum::DISEASE_RESISTANCE, 1000);

	EXPECT_FALSE(calculated(*caster, *stoneResistant, petrification)->isInSuccessEffects(1)) << "PERIFICATION_RESISTANCE";
	EXPECT_TRUE(calculated(*caster, *diseaseResistant, petrification)->isInSuccessEffects(1));
	EXPECT_FALSE(calculated(*caster, *diseaseResistant, disease)->isInSuccessEffects(1)) << "DISEASE_RESISTANCE";
	EXPECT_TRUE(calculated(*caster, *stoneResistant, disease)->isInSuccessEffects(1));
}

TEST_F(CrowdControlEffectsTest, ADiseaseSetsItsStateUntilItEnds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* disease = bindSkill(DISEASE_XML);
	Ref<Player> caster = daeva(8713, PlayerClass::SORCERER);
	Ref<Npc> npc = makeMonster(701714, 510, 500);

	Ref<Effect> effect = forced(*caster, *npc, disease);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::DISEASE));
	effect->endEffect();
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::DISEASE));
}

// ---- confusion ------------------------------------------------------------------------------------------------------------------------------------

TEST_F(CrowdControlEffectsTest, AConfusedMonsterWandersUntilTheConfusionEnds) {
	EFFECT_TEST_SCOPE;
	FearEnableScope fearEnable(true);
	const model::SkillTemplate* confuse = bindSkill(CONFUSE_XML);
	ASSERT_EQ(effectOf(*confuse, 0).javaClassName(), "ConfuseEffect");
	Ref<Player> caster = daeva(8714, PlayerClass::SORCERER);
	Ref<Npc> npc = makeMonster(701715, 510, 500);

	Ref<Effect> effect = cast(*caster, *npc, confuse, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::CONFUSE));
	EXPECT_TRUE(npc->getEffectController()->isConfused());
	EXPECT_TRUE(npc->getAi().isInState(ai::AIState::CONFUSE)) << "ConfuseEffect.startEffect: setStateIfNot(CONFUSE)";
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "abortMove; the ConfuseTask has not run yet";

	advance(1);
	EXPECT_TRUE(npc->getMoveController()->isInMove()) << "the ConfuseTask's first run: moveToPoint";

	advance(5000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::CONFUSE)) << "ConfuseEffect.endEffect";
	EXPECT_FALSE(npc->getAi().isInState(ai::AIState::CONFUSE)) << "setStateIfNot(IDLE), then the ATTACK event";
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "abortMove";
}

TEST_F(CrowdControlEffectsTest, WithoutFearEnableAConfusedMonsterStaysPut) {
	EFFECT_TEST_SCOPE;
	FearEnableScope fearDisabled(false);
	const model::SkillTemplate* confuse = bindSkill(CONFUSE_XML);
	Ref<Player> caster = daeva(8715, PlayerClass::SORCERER);
	Ref<Npc> npc = makeMonster(701716, 510, 500);

	Ref<Effect> effect = cast(*caster, *npc, confuse, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	advance(2000);
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "ConfuseEffect.startEffect schedules no ConfuseTask";
	effect->endEffect();
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
