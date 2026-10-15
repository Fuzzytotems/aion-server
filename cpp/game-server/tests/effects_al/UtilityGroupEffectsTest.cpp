// The utility effects of P5-03 / P5-04 (M5b-2's utility group, ported 2026-10-03 on the owner's request): SearchEffect, SkillLauncherEffect,
// the four dispel variants DispelBuffEffect, DispelDebuffMentalEffect, DispelNpcBuffEffect and DispelNpcDebuffEffect (AbstractDispelEffect with
// their category and slot), AbstractAbsoluteStatEffect (the absstatbuff / absstatdebuff of absolute_stats.xml), and the four empty Java bodies
// UtilityEffect, SupportEventEffect, DummyEffect and ActivateEnslaveEffect. Each case binds the data template that uses the effect
// (skill_templates.xml, cut to its effect) and runs it on the DaevaEffectTest fixture: a case that reaches an AION_UNPORTED site fails in
// TearDown, so a green case ran through ported code only.

#include "DaevaEffectsTestSupport.h"

#include <cstdint>
#include <string>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/AbsoluteStatsData.bind.h"
#include "aion/gameserver/dataholders/AbsoluteStatsData.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::state::CreatureSeeState;
using model::Effect;

std::string skillXml(int32_t skillId, std::string_view name, std::string_view subtype, std::string_view tslot, std::string_view effectXml,
	std::string_view extra = "") {
	return templateXml(R"(skill_id=")" + std::to_string(skillId) + R"(" name=")" + std::string(name) + R"(" nameId="1" stack="TEST_UTILITY_)" +
			std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype=")" + std::string(subtype) + R"(" tslot=")" + std::string(tslot) +
			R"(" activation="ACTIVE" cooldown="0" duration="0")" + std::string(extra),
		effectXml);
}

/** a long buff or debuff of the given slot and dispel category (level 1, count 1), the target of the dispels */
std::string dispellable(int32_t skillId, std::string_view tslot, std::string_view category) {
	return skillXml(skillId, std::string("dispellable ") + std::string(category), tslot == "BUFF" ? "BUFF" : "DEBUFF", tslot,
		R"(<statup duration2="600000" e="1" noresist="true"><change stat="PHYSICAL_DEFENSE" func="ADD" value="1" /></statup>)",
		R"( dispel_category=")" + std::string(category) + R"(" req_dispel_level="1" req_dispel_count="1")");
}

// ---- the effects of the data (skill_templates.xml), each on its own template -----------------------------------------------------------------

/** 1099 Hunter's Eye (SearchEffect) */
const std::string SEARCH_XML =
	skillXml(1099, "Hunter's Eye", "BUFF", "BUFF", R"(<search state="SEARCH1" duration2="60000" effectid="166" e="1" noresist="true" />)");
/** 3550 Magic Implosion's launcher of skill 8776 (SkillLauncherEffect), and a stand-in for 8776 */
const std::string SKILL_LAUNCHER_XML = skillXml(3550, "Magic Implosion", "NONE", "NONE", R"(<skilllauncher skill_id="8776" value="1" e="1" />)");
const std::string LAUNCHED_XML = dispellable(8776, "BUFF", "BUFF");
/** 305 Dispel Magic I, 3938 Splendor of Purification, 10352 Green Cleanse, 18154 Absorb Drana: the four dispels, cut to the dispel */
const std::string DISPEL_BUFF_XML = skillXml(305, "Dispel Magic I", "NONE", "NONE",
	R"(<dispelbuff dispel_level="1" power="10" value="5" e="1" noresist="true" element="EARTH" />)", R"( hostile_type="DIRECT")");
const std::string DISPEL_DEBUFF_MENTAL_XML = skillXml(3938, "Splendor of Purification", "BUFF", "NONE",
	R"(<dispeldebuffmental dispel_level="1" power="30" value="5" e="1" noresist="true" />)");
const std::string DISPEL_NPC_BUFF_XML = skillXml(10352, "Green Cleanse", "NONE", "NONE",
	R"(<dispelnpcbuff dispel_level="5" power="100" value="5" e="1" noresist="true" element="LIGHT" />)", R"( hostile_type="DIRECT")");
const std::string DISPEL_NPC_DEBUFF_XML = skillXml(18154, "Absorb Drana", "NONE", "NONE",
	R"(<dispelnpcdebuff dispel_level="5" power="50" value="5" e="1" noresist="true" element="WATER" />)");
/** 20558 Test: Stats Absolute Value Correction Buff 01 (AbsoluteStatToPCBuffEffect), on a stats set of the case */
const std::string ABS_STAT_XML = skillXml(20558, "Test: Stats Absolute Value Correction Buff 01", "BUFF", "BUFF",
	R"(<absstatbuff statsetid="901" duration2="30000" effectid="10188461" e="1" noresist="true" element="WATER" />)");
const std::string ABS_STAT_UNKNOWN_XML = skillXml(64401, "absolute stats of no set", "BUFF", "BUFF",
	R"(<absstatbuff statsetid="902" duration2="30000" e="1" noresist="true" />)");
/** the four empty Java bodies, as one template: utility, supportevent, dummy, activateenslave */
const std::string EMPTY_EFFECTS_XML = skillXml(64402, "empty effects", "NONE", "NONE",
	R"(<utility e="1" noresist="true" /><supportevent e="2" noresist="true" /><dummy e="3" /><activateenslave e="4" noresist="true" />)");

class UtilityGroupEffectsTest : public DaevaEffectTest {
protected:
	void TearDown() override {
		DaevaEffectTest::TearDown();
		dataholders::DataManager::ABSOLUTE_STATS_DATA.resetForTests();
	}

	xml::LoadContext absoluteStatsContext;
};

// ---- SearchEffect / SkillLauncherEffect -----------------------------------------------------------------------------------------------------

TEST_F(UtilityGroupEffectsTest, HuntersEyeSeesSearch1UntilItEnds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(SEARCH_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "SearchEffect");
	Ref<Player> ranger = daeva(9101, PlayerClass::RANGER);
	ASSERT_FALSE(ranger->isInSeeState(CreatureSeeState::SEARCH1));

	Ref<Effect> effect = cast(*ranger, *ranger, skill, 1);
	EXPECT_TRUE(ranger->getEffectController()->hasAbnormalEffect(1099)) << "applyEffect: addToEffectedController";
	EXPECT_TRUE(ranger->isInSeeState(CreatureSeeState::SEARCH1)) << "startEffect";
	effect->endEffect();
	EXPECT_FALSE(ranger->isInSeeState(CreatureSeeState::SEARCH1)) << "endEffect";
}

TEST_F(UtilityGroupEffectsTest, MagicImplosionAppliesItsSkillToTheEffected) {
	EFFECT_TEST_SCOPE;
	bindSkill(LAUNCHED_XML);
	const model::SkillTemplate* skill = bindSkill(SKILL_LAUNCHER_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "SkillLauncherEffect");
	Ref<Player> caster = daeva(9102, PlayerClass::SORCERER);
	Ref<Npc> monster = makeMonster(702102, 505, 500);

	Ref<Effect> effect = calculated(*caster, *monster, skill, 1);
	EXPECT_TRUE(effect->isInSuccessEffects(1)) << "calculate: addSuccessEffect, no resist roll";
	effect->applyEffect();
	EXPECT_TRUE(monster->getEffectController()->hasAbnormalEffect(8776)) << "SkillEngine.applyEffect(skillId, effector, effected)";
	EXPECT_FALSE(caster->getEffectController()->hasAbnormalEffect(8776));
}

// ---- the dispels ----------------------------------------------------------------------------------------------------------------------------

/**
 * Each dispel variant hands its category and slot to AbstractDispelEffect (EffectController.removeEffectByDispelCat): on a monster holding a buff
 * of each buff category and a debuff of each debuff category, it removes only the effects of its own category in its own slot.
 */
TEST_F(UtilityGroupEffectsTest, EachDispelRemovesOnlyItsCategoryInItsSlot) {
	EFFECT_TEST_SCOPE;
	struct Case {
		const std::string& xml;
		const char* className;
		int32_t removed;
	};
	const model::SkillTemplate* buff = bindSkill(dispellable(64411, "BUFF", "BUFF"));
	const model::SkillTemplate* npcBuff = bindSkill(dispellable(64412, "BUFF", "NPC_BUFF"));
	const model::SkillTemplate* mental = bindSkill(dispellable(64413, "DEBUFF", "DEBUFF_MENTAL"));
	const model::SkillTemplate* npcPhysical = bindSkill(dispellable(64414, "DEBUFF", "NPC_DEBUFF_PHYSICAL"));
	const model::SkillTemplate* physical = bindSkill(dispellable(64415, "DEBUFF", "DEBUFF_PHYSICAL"));
	const Case cases[] = {
		{DISPEL_BUFF_XML, "DispelBuffEffect", 64411},
		{DISPEL_NPC_BUFF_XML, "DispelNpcBuffEffect", 64412},
		{DISPEL_DEBUFF_MENTAL_XML, "DispelDebuffMentalEffect", 64413},
		{DISPEL_NPC_DEBUFF_XML, "DispelNpcDebuffEffect", 64414},
	};
	Ref<Npc> caster = makeMonster(702110, 510, 500);
	int32_t id = 702111;
	for (const Case& c : cases) {
		SCOPED_TRACE(c.className);
		const model::SkillTemplate* dispel = bindSkill(c.xml);
		ASSERT_EQ(effectOf(*dispel, 0).javaClassName(), c.className);
		Ref<Npc> target = makeMonster(id++, 505, 500);
		for (const model::SkillTemplate* held : {buff, npcBuff, mental, npcPhysical, physical})
			forced(*target, *target, held, 1);
		for (int32_t skillId : {64411, 64412, 64413, 64414, 64415})
			ASSERT_TRUE(target->getEffectController()->hasAbnormalEffect(skillId)) << skillId;

		forced(*caster, *target, dispel, 1);
		for (int32_t skillId : {64411, 64412, 64413, 64414, 64415})
			EXPECT_EQ(target->getEffectController()->hasAbnormalEffect(skillId), skillId != c.removed) << skillId;
	}
}

// ---- AbstractAbsoluteStatEffect -------------------------------------------------------------------------------------------------------------

TEST_F(UtilityGroupEffectsTest, AnAbsoluteStatBuffSetsTheStatsOfItsSet) {
	EFFECT_TEST_SCOPE;
	dataholders::DataManager::ABSOLUTE_STATS_DATA.publish(xml::bindString<dataholders::AbsoluteStatsData>(absoluteStatsContext,
		R"(<absolute_stats><stats_set id="901"><modifiers><abs name="MAXHP" value="5000"/><abs name="MAXMP" value="4000"/></modifiers>)"
		R"(</stats_set></absolute_stats>)"));
	const model::SkillTemplate* skill = bindSkill(ABS_STAT_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "AbsoluteStatToPCBuffEffect");
	Ref<Player> player = daeva(9103, PlayerClass::GLADIATOR);
	ASSERT_NE(player->getGameStats()->getMaxHp()->getCurrent(), 5000);

	Ref<Effect> effect = forced(*player, *player, skill, 1);
	EXPECT_EQ(player->getGameStats()->getMaxHp()->getCurrent(), 5000) << "getModifiers: the set's abs functions";
	EXPECT_EQ(player->getGameStats()->getMaxMp()->getCurrent(), 4000);
	effect->endEffect();
	EXPECT_NE(player->getGameStats()->getMaxHp()->getCurrent(), 5000);
}

TEST_F(UtilityGroupEffectsTest, AnAbsoluteStatBuffOfAnUnknownSetThrows) {
	EFFECT_TEST_SCOPE;
	dataholders::DataManager::ABSOLUTE_STATS_DATA.publish(xml::bindString<dataholders::AbsoluteStatsData>(absoluteStatsContext,
		R"(<absolute_stats><stats_set id="901"><modifiers><abs name="MAXHP" value="5000"/></modifiers></stats_set></absolute_stats>)"));
	const model::SkillTemplate* skill = bindSkill(ABS_STAT_UNKNOWN_XML);
	Ref<Player> player = daeva(9104, PlayerClass::GLADIATOR);
	const auto& absolute = dynamic_cast<const effect::AbstractAbsoluteStatEffect&>(effectOf(*skill, 0));
	EXPECT_EQ(absolute.getModifiersSet(), nullptr);
	Ref<Effect> effect = calculated(*player, *player, skill, 1);
	// BufEffect.startEffect -> getModifiers: getModifiersSet() is null; Effect.applyEffect wraps it ("Error applying effect of skill ...")
	try {
		effect->applyEffect();
		ADD_FAILURE() << "no exception";
	} catch (const std::exception& e) {
		EXPECT_NE(causeChain(e).find("no absolute stats set 902"), std::string::npos) << causeChain(e);
	}
}

// ---- the empty Java bodies ------------------------------------------------------------------------------------------------------------------

TEST_F(UtilityGroupEffectsTest, TheEmptyEffectsDoNothing) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(EMPTY_EFFECTS_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "UtilityEffect");
	ASSERT_EQ(effectOf(*skill, 1).javaClassName(), "SupportEventEffect");
	ASSERT_EQ(effectOf(*skill, 2).javaClassName(), "DummyEffect");
	ASSERT_EQ(effectOf(*skill, 3).javaClassName(), "ActivateEnslaveEffect");
	Ref<Player> player = daeva(9105, PlayerClass::GLADIATOR);
	forced(*player, *player, skill, 1);
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(64402)) << "their applyEffect is empty: no addToEffectedController";
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
