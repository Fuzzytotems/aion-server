// P5-03, M5b-2 stage 1 part 3, items F-02 and F-05 (m5b2-plan.md §5): the buff classes of the A-L half - BufEffect (through the data-only
// StatboostEffect, `<statboost>`, which overrides nothing), ArmorMasteryEffect, AlwaysDodgeEffect, AlwaysResistEffect and HideEffect - each on a
// real Effect of a template bound through the real binder, driven through Effect.initialize (calculate), Effect.applyEffect
// (applyEffect -> EffectController.addEffect -> startEffect) and the end task or an observer's Effect.endEffect.
//
// Every enter-world passive goes through BufEffect (m5b2-plan.md D2: the level-1 Warrior's 37/39 WeaponMastery, 40/41/42/103 ArmorMastery, 43
// ShieldMastery, 140 Statboost), and so do the statup and hide post-spawn skills of D7; this file pins what the body of each class does to the
// effected creature's stats, observers, abnormal states and visual state, and what the client is told (SM_ABNORMAL_STATE to the effected player,
// SM_ABNORMAL_EFFECT to the players who see an effected npc, SM_PLAYER_STATE for a hide).
//
// Real templates: 140 Boost Physical Attack I (skill_templates.xml:1872-1882), 40 Basic Clothing Proficiency (:768-778), 3195 Focused Evasion
// (:53955-53967), 3222 Stealth (:54396-54414, without its randomtime so its duration is exact). The crafted templates keep the data's attribute
// spelling and choose stats that no player stat function reads (PlayerStatFunctions.java:25-39), so a stat's value is the arithmetic of the class.

#include "EffectClassTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualStateInfo.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackStatus;
using effect::AbnormalState;
using gameserver::model::gameobjects::state::CreatureVisualState;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_PLAYER_STATE;

class BuffEffectsTest : public EffectClassTest {
protected:
	/** The player's skill list holds exactly these skills (Equipment.checkAvailableEquipSkills asks it before an armor piece is equipped) */
	void learn(Player& player, std::initializer_list<int32_t> skillIds) {
		std::vector<Ptr<gameserver::model::skill::PlayerSkillEntry>> entries;
		for (int32_t skillId : skillIds) {
			learned.push_back(gameserver::model::skill::PlayerSkillEntry::create(skillId, 1, 0,
				gameserver::model::gameobjects::Persistable_PersistentState::NOACTION));
			entries.emplace_back(*learned.back());
		}
		player.setSkillList(gameserver::model::skill::PlayerSkillList::create(entries));
	}

	/** An item template bound through the real binder (immortal static data, like the holder keeps it) */
	static const gameserver::model::templates::item::ItemTemplate* itemTemplate(const std::string& xmlText) {
		xml::LoadContext context;
		return xml::bindString<gameserver::model::templates::item::ItemTemplate>(context, xmlText).release();
	}

	/** Java PlayerService's equipped row: Equipment.onLoadHandler of an item equipped in `slot` */
	Ref<gameserver::model::gameobjects::Item> equip(Player& player, int32_t objectId, const gameserver::model::templates::item::ItemTemplate* item,
		gameserver::model::items::ItemSlot slot) {
		Ref<gameserver::model::gameobjects::Item> equipped =
			gameserver::model::gameobjects::Item::create(objectId, item, 1, true, gameserver::model::items::getSlotIdMask(slot));
		player.getEquipment().onLoadHandler(*equipped);
		return equipped;
	}

	std::vector<Ref<gameserver::model::skill::PlayerSkillEntry>> learned;
};

/** The SkillTargetSlot ordinal SM_ABNORMAL_STATE writes (SkillTargetSlot.java: BUFF is the first constant) */
constexpr int32_t BUFF_ORDINAL = 0;

// ---- BufEffect ----------------------------------------------------------------------------------------------------------------------------------

/**
 * A <statboost> buff with the three change functions of BufEffect.getModifiers (BufEffect.java:59-90), each `value + delta * skillLvl` at level
 * 2: ADD 7 + 3*2 as a bonus (StatAddFunction), PERCENT 10 + 5*2 as a bonus rate (StatRateFunction: base 1000 * 20 / 100), REPLACE 40 - 2*2 as the
 * base (StatSetFunction, bonus false); three more changes, one per func, carry a <conditions> whose weapon the player does not hold, so their
 * functions exist but never validate (withConditions on every arm: 25 conditioned PERCENT changes of statboost and statup are in the data).
 * startEffect adds them as the effect's functions (CreatureGameStats.addEffect), endEffect removes them (Effect.endEffects ->
 * CreatureGameStats.endEffect, because the template has changes). The effected player is told the icon both times.
 */
constexpr const char* BUF_SKILL_XML =
	R"(<skill_template skill_id="9701" name="buf test" nameId="1" stack="EFFECTS_AL_BUF" lvl="3" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" activation="ACTIVE" duration="0"><properties first_target="ME" target_type="ONLYONE"/><effects>)"
	R"(<statboost duration2="10000" effectid="970101" e="1" noresist="true">)"
	R"(<change stat="BOOST_SPELL_ATTACK" func="ADD" value="7" delta="3"/>)"
	R"(<change stat="BOOST_HATE" func="PERCENT" value="10" delta="5"/>)"
	R"(<change stat="MAGICAL_CRITICAL_RESIST" func="REPLACE" value="40" delta="-2"/>)"
	R"(<change stat="HEAL_SKILL_DEBOOST" func="ADD" value="1000"><conditions><weapon weapon="BOW"/></conditions></change>)"
	R"(<change stat="HEAL_SKILL_BOOST" func="PERCENT" value="50"><conditions><weapon weapon="BOW"/></conditions></change>)"
	R"(<change stat="CONCENTRATION" func="REPLACE" value="77"><conditions><weapon weapon="BOW"/></conditions></change>)"
	R"(</statboost></effects></skill_template>)";

TEST_F(BuffEffectsTest, BufEffectAddsEachChangeAsAFunctionOfTheEffectForItsDuration) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5001);
	cp::RecordingAionConnection& client = connect(*player, *accounts.back());
	const model::SkillTemplate* skill = bindSkill(BUF_SKILL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "StatboostEffect") << "data-only: BufEffect's bodies answer";
	ASSERT_EQ(statOf(*player, StatEnum::BOOST_SPELL_ATTACK, 0), 0);
	ASSERT_EQ(statOf(*player, StatEnum::BOOST_HATE, 1000), 1000);
	ASSERT_EQ(statOf(*player, StatEnum::MAGICAL_CRITICAL_RESIST, 500), 500);
	ASSERT_EQ(statOf(*player, StatEnum::HEAL_SKILL_BOOST, 1000), 1000);
	ASSERT_EQ(statOf(*player, StatEnum::CONCENTRATION, 500), 500);

	Ref<Effect> effect = cast(*player, *player, skill, 2);
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9701)) << "applyEffect: effect.addToEffectedController()";
	EXPECT_EQ(effect->getDuration(), 10000) << "duration2, no duration1";
	EXPECT_EQ(statOf(*player, StatEnum::BOOST_SPELL_ATTACK, 0), 13) << "ADD: 7 + 3 * 2 as a bonus";
	EXPECT_EQ(statBaseOf(*player, StatEnum::BOOST_SPELL_ATTACK, 0), 0) << "ADD: new StatAddFunction(stat, value, true), not the base";
	EXPECT_EQ(statBonusOf(*player, StatEnum::BOOST_SPELL_ATTACK, 0), 13);
	EXPECT_EQ(statOf(*player, StatEnum::BOOST_HATE, 1000), 1200) << "PERCENT: 1000 * (10 + 5 * 2) / 100 as a bonus";
	EXPECT_EQ(statBaseOf(*player, StatEnum::BOOST_HATE, 1000), 1000) << "PERCENT: new StatRateFunction(stat, value, true), not the base";
	EXPECT_EQ(statBonusOf(*player, StatEnum::BOOST_HATE, 1000), 200);
	EXPECT_EQ(statOf(*player, StatEnum::MAGICAL_CRITICAL_RESIST, 500), 36) << "REPLACE: the base set to 40 - 2 * 2";
	EXPECT_EQ(statOf(*player, StatEnum::HEAL_SKILL_DEBOOST, 0), 0) << "the conditioned change: its weapon condition fails for a player without a bow";
	EXPECT_EQ(statOf(*player, StatEnum::HEAL_SKILL_BOOST, 1000), 1000) << "the conditioned PERCENT change fails the same way";
	EXPECT_EQ(statOf(*player, StatEnum::CONCENTRATION, 500), 500) << "and so does the conditioned REPLACE change";
	std::vector<std::vector<uint8_t>> states = packetsOf<SM_ABNORMAL_STATE>(client);
	ASSERT_EQ(states.size(), 1u) << "PlayerEffectController.addEffect -> updatePlayerEffectIcons";
	AbnormalStateFields shown = decodeAbnormalState(states[0]);
	EXPECT_EQ(shown.slot, model::getId(model::SkillTargetSlot::BUFF));
	ASSERT_EQ(shown.effects.size(), 1u);
	EXPECT_EQ(shown.effects[0].effectorId, player->getObjectId());
	EXPECT_EQ(shown.effects[0].skillId, 9701);
	EXPECT_EQ(shown.effects[0].level, 2);
	EXPECT_EQ(shown.effects[0].targetSlot, BUFF_ORDINAL);
	EXPECT_EQ(shown.effects[0].remainingTime, 10000);

	client.clearSent();
	advance(9999);
	EXPECT_EQ(statOf(*player, StatEnum::BOOST_SPELL_ATTACK, 0), 13) << "one millisecond before the end";
	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(9701));
	EXPECT_EQ(statOf(*player, StatEnum::BOOST_SPELL_ATTACK, 0), 0) << "endEffect removed the effect's functions";
	EXPECT_EQ(statOf(*player, StatEnum::BOOST_HATE, 1000), 1000);
	EXPECT_EQ(statOf(*player, StatEnum::MAGICAL_CRITICAL_RESIST, 500), 500);
	states = packetsOf<SM_ABNORMAL_STATE>(client);
	ASSERT_EQ(states.size(), 1u) << "clearEffect(broadcast) -> updatePlayerEffectIcons";
	EXPECT_TRUE(decodeAbnormalState(states[0]).effects.empty());
}

/**
 * `maxstat` (BufEffect.java:51-52): after the functions are added, the life stats are synchronized with the new maxima - the current HP becomes the
 * raised MAXHP. Without it the current HP is only scaled with the maximum by CreatureGameStats.checkMaxHPChanged and stays below it.
 */
TEST_F(BuffEffectsTest, AMaxstatBufFillsTheLifeStatsToTheRaisedMaximum) {
	EFFECT_TEST_SCOPE;
	auto maxHpBuff = [this](int32_t skillId, std::string_view maxstat) {
		return bindSkill(R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="maxstat" nameId="1" stack="EFFECTS_AL_MAXSTAT_)"
			+ std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">)"
			+ R"(<effects><statboost duration2="10000" e="1" noresist="true")" + std::string(maxstat) + R"(>)"
			+ R"(<change stat="MAXHP" func="ADD" value="100"/></statboost></effects></skill_template>)");
	};
	Ref<Player> kept = makePlayer(5011);
	Ref<Player> filled = makePlayer(5012);
	const int32_t maxHp = kept->getLifeStats()->getMaxHp();
	ASSERT_EQ(filled->getLifeStats()->getMaxHp(), maxHp);
	kept->getLifeStats()->setCurrentHp(maxHp / 2);
	filled->getLifeStats()->setCurrentHp(maxHp / 2);

	cast(*kept, *kept, maxHpBuff(9711, ""), 1);
	EXPECT_EQ(kept->getLifeStats()->getMaxHp(), maxHp + 100) << "the MAXHP function of the buff";
	EXPECT_LT(kept->getLifeStats()->getCurrentHp(), maxHp + 100) << "no maxstat: not synchronized with the new maximum";

	cast(*filled, *filled, maxHpBuff(9712, R"( maxstat="true")"), 1);
	EXPECT_EQ(filled->getLifeStats()->getMaxHp(), maxHp + 100);
	EXPECT_EQ(filled->getLifeStats()->getCurrentHp(), maxHp + 100) << "maxstat: effected.getLifeStats().synchronizeWithMaxStats()";
}

/**
 * The shape of a level-1 Warrior's enter-world pair on one stat (m5b2-plan.md D2: 140 Boost Physical Attack I, PHYSICAL_ATTACK ADD 7, beside 37's
 * PERCENT 16), here as two buffs on a stat no player function reads. Both of BufEffect's functions are bonuses, so the rate is taken of the base
 * alone (StatRateFunction.java: `stat.getBaseWithoutBaseRate() * value / 100f` added to the bonus) and the ADD lands in the bonus beside it: base
 * 1000, bonus 7 + 1000 * 16 / 100 = 167. An ADD on the base would be scaled by the rate (base 1007, bonus 161, 1168); a PERCENT on the base would
 * give the same sum with base 1160 and bonus 7, which the client's SM_STATS_INFO would still show apart.
 */
TEST_F(BuffEffectsTest, AnAddAndAPercentBufOnOneStatAreBothBonusesOverTheBase) {
	EFFECT_TEST_SCOPE;
	auto buff = [this](int32_t skillId, std::string_view change) {
		return bindSkill(R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="pair" nameId="1" stack="EFFECTS_AL_PAIR_)"
			+ std::to_string(skillId) + R"(" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">)"
			+ R"(<effects><statboost duration2="10000" e="1" noresist="true">)" + std::string(change) + R"(</statboost></effects></skill_template>)");
	};
	Ref<Player> player = makePlayer(5015);

	cast(*player, *player, buff(9713, R"(<change stat="BOOST_HATE" func="ADD" value="7"/>)"), 1);
	cast(*player, *player, buff(9714, R"(<change stat="BOOST_HATE" func="PERCENT" value="16"/>)"), 1);
	EXPECT_EQ(statBaseOf(*player, StatEnum::BOOST_HATE, 1000), 1000);
	EXPECT_EQ(statBonusOf(*player, StatEnum::BOOST_HATE, 1000), 167) << "7 + 1000 * 16 / 100f";
	EXPECT_EQ(statOf(*player, StatEnum::BOOST_HATE, 1000), 1167);
}

/**
 * The enter-world passive (SkillEngine.applyEffectDirectly: `new Effect(effector, effected, template, level, null, ForceType.DEFAULT)`, initialize,
 * applyEffect; still held back by the O-09 partial until the regate step): 140 Boost Physical Attack I adds PHYSICAL_ATTACK +7 as a bonus while the
 * effect lasts - a passive has no duration and no icon (PlayerEffectController.updatePlayerIconsAndGroup skips a passive effect).
 */
TEST_F(BuffEffectsTest, TheEnterWorldPassiveAddsItsFunctionWithoutAnIconUntilItEnds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5021);
	cp::RecordingAionConnection& client = connect(*player, *accounts.back());
	// skill_templates.xml:1872-1882, verbatim
	const model::SkillTemplate* skill = bindSkill(
		R"(<skill_template skill_id="140" name="Boost Physical Attack I" nameId="282029" group="P_STATBOOSTPHYSICALOFFENSE")"
		R"( stack="P_STATBOOSTPHYSICALOFFENSE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0")"
		R"( duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><properties first_target="ME" />)"
		R"(<useconditions><move_casting allow="false" /></useconditions><effects><statboost effectid="103041" e="1">)"
		R"(<change stat="PHYSICAL_ATTACK" func="ADD" value="7" /></statboost></effects></skill_template>)");
	const int32_t attack = statOf(*player, StatEnum::PHYSICAL_ATTACK, 0);

	Ref<Effect> effect = Effect::create(*player, Ptr<Creature>(*player), skill, 1, std::nullopt, model::Effect::ForceType::DEFAULT);
	effect->initialize();
	effect->applyEffect();
	EXPECT_TRUE(effect->isInSuccessEffects(1)) << "EffectTemplate.calculate's passive arm";
	EXPECT_EQ(player->getEffectController()->getAllEffects().size(), 1u);
	EXPECT_EQ(statOf(*player, StatEnum::PHYSICAL_ATTACK, 0), attack + 7);
	EXPECT_EQ(effect->getDuration(), 0) << "no duration2: no end task";
	EXPECT_TRUE(packetsOf<SM_ABNORMAL_STATE>(client).empty()) << "a passive effect shows no icon";

	advance(3600000);
	EXPECT_EQ(statOf(*player, StatEnum::PHYSICAL_ATTACK, 0), attack + 7) << "a passive lasts";
	effect->endEffect();
	EXPECT_EQ(statOf(*player, StatEnum::PHYSICAL_ATTACK, 0), attack);
	EXPECT_TRUE(player->getEffectController()->getAllEffects().empty());
	EXPECT_TRUE(packetsOf<SM_ABNORMAL_STATE>(client).empty());
}

// ---- ArmorMasteryEffect ---------------------------------------------------------------------------------------------------------------------

/**
 * ArmorMasteryEffect.startEffect (ArmorMasteryEffect.java:29-40) replaces each change function by a StatArmorMasteryFunction of the armor type:
 * fixedBonus = value + delta * skillLevel = 4 + 1 * 2 = 6, and the equipment factor of the equipped CLOTHES pieces, TORSO 30 + PANTS 25 = 55
 * (StatArmorMasteryFunction.java:33-49). On a base of 1000 the rate is value * factor / 100 = 10 * 55 / 100 = 5 (int), a bonus of 1000 * 5 / 100f
 * = 50, plus the fixed bonus 6 * 55 / 100f = 3.3: (int) 1053.3 = 1053. A player without clothes gets factor 0: no rate and no fixed bonus.
 */
TEST_F(BuffEffectsTest, ArmorMasteryScalesItsChangesByTheEquippedArmorOfItsType) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* mastery = bindSkill(
		R"(<skill_template skill_id="9721" name="mastery" nameId="1" stack="EFFECTS_AL_MASTERY" lvl="2" skilltype="PHYSICAL" skillsubtype="NONE")"
		R"( tslot="NOSHOW" activation="PASSIVE" duration="0"><effects><armormastery armor="CLOTHES" value="4" delta="1" e="1">)"
		R"(<change stat="BOOST_HATE" func="PERCENT" value="10"/></armormastery></effects></skill_template>)");
	ASSERT_EQ(effectOf(*mastery, 0).javaClassName(), "ArmorMasteryEffect");
	Ref<Player> dressed = makePlayer(5031);
	learn(*dressed, {40}); // CL_TORSO and CL_PANTS want skill 40 (ItemGroup.java:42-46)
	equip(*dressed, 5032, itemTemplate(R"(<item_template id="110000008" name="Lepharist Uniform" level="1" item_group="CL_TORSO"/>)"),
		gameserver::model::items::ItemSlot::TORSO);
	equip(*dressed, 5033, itemTemplate(R"(<item_template id="113000001" name="test pants" level="1" item_group="CL_PANTS"/>)"),
		gameserver::model::items::ItemSlot::PANTS);
	ASSERT_EQ(dressed->getEquipment().getEquippedItems().size(), 2u) << "both pieces were equipped";
	Ref<Player> bare = makePlayer(5034);

	Ref<Effect> dressedEffect = cast(*dressed, *dressed, mastery, 2);
	Ref<Effect> bareEffect = cast(*bare, *bare, mastery, 2);
	EXPECT_EQ(statOf(*dressed, StatEnum::BOOST_HATE, 1000), 1053) << "1000 + 1000 * (10 * 55 / 100) / 100f + (4 + 1 * 2) * 55 / 100f";
	EXPECT_EQ(statBaseOf(*dressed, StatEnum::BOOST_HATE, 1000), 1000) << "the mastery keeps the change's bonus flag: a bonus rate, not the base";
	EXPECT_EQ(statBonusOf(*dressed, StatEnum::BOOST_HATE, 1000), 53) << "50 of the rate and (int) 3.3 of the fixed bonus";
	EXPECT_EQ(statOf(*bare, StatEnum::BOOST_HATE, 1000), 1000) << "equipment factor 0: the mastery adds nothing";
	EXPECT_EQ(dressed->getEffectController()->getAllEffects().size(), 1u);

	dressedEffect->endEffect();
	bareEffect->endEffect();
	EXPECT_EQ(statOf(*dressed, StatEnum::BOOST_HATE, 1000), 1000) << "the mastery functions belong to the effect and end with it";
}

/**
 * The order of ArmorMasteryEffect.startEffect: `if (change == null) return;` comes before the `(Player)` cast, so a mastery without changes adds
 * nothing to anyone, and one with changes casts the effected - an npc is a ClassCastException. The real 40 Basic Clothing Proficiency binds as an
 * ArmorMasteryEffect whose PHYSICAL_DEFENSE rate needs the clothes to count.
 */
TEST_F(BuffEffectsTest, ArmorMasteryReturnsWithoutChangesBeforeItCastsTheEffectedToAPlayer) {
	EFFECT_TEST_SCOPE;
	// skill_templates.xml:768-778, verbatim
	const model::SkillTemplate* clothing = bindSkill(
		R"(<skill_template skill_id="40" name="Basic Clothing Proficiency" nameId="281821" group="P_EQUIP_ENHANCEDCLOTH")"
		R"( stack="P_EQUIP_ENHANCEDCLOTH" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><properties first_target="ME" />)"
		R"(<useconditions><move_casting allow="false" /></useconditions><effects>)"
		R"(<armormastery armor="CLOTHES" value="1" delta="0" effectid="121" e="1" basiclvl="1">)"
		R"(<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="10" /></armormastery></effects></skill_template>)");
	const model::SkillTemplate* empty = bindSkill(
		R"(<skill_template skill_id="9722" name="no change" nameId="1" stack="EFFECTS_AL_NO_CHANGE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
		R"( tslot="NOSHOW" activation="PASSIVE" duration="0"><effects><armormastery armor="CLOTHES" value="1" e="1"/></effects></skill_template>)");
	Ref<Npc> npc = makeNpc(700531);
	Ref<Player> player = makePlayer(5041);

	Ref<Effect> onNpc = Effect::create(*player, Ptr<Creature>(*npc), clothing, 1);
	EXPECT_THROW(effectOf(*clothing, 0).startEffect(*onNpc), runtime::ClassCastException) << "Java: ((Player) effect.getEffected())";
	Ref<Effect> emptyOnNpc = Effect::create(*player, Ptr<Creature>(*npc), empty, 1);
	EXPECT_NO_THROW(effectOf(*empty, 0).startEffect(*emptyOnNpc)) << "change == null returns before the cast";

	const int32_t defense = statOf(*player, StatEnum::PHYSICAL_DEFENSE, 1000);
	Ref<Effect> bare = cast(*player, *player, clothing, 1);
	EXPECT_EQ(statOf(*player, StatEnum::PHYSICAL_DEFENSE, 1000), defense) << "no clothes equipped: factor 0";
	bare->endEffect();
}

// ---- AlwaysDodgeEffect and AlwaysResistEffect -----------------------------------------------------------------------------------------------

/** 3195 Focused Evasion, skill_templates.xml:53955-53967 verbatim: <alwaysdodge value="1"> at position 1, <alwaysresist value="1"> at 2 */
constexpr const char* FOCUSED_EVASION_XML =
	R"(<skill_template skill_id="3195" name="Focused Evasion" nameId="2287183" cooldownId="174" group="SC_WHISPEROFWIND" stack="SC_WHISPEROFWIND")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><properties first_target="ME" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" />)"
	R"(<endconditions><chargeweapon value="9" /><chargearmor value="10" /><polishchargeweapon value="69" /></endconditions><effects>)"
	R"(<alwaysdodge value="1" duration2="5000" effectid="142" e="1" noresist="true" hoptype="SKILLLV" hopb="368" />)"
	R"(<alwaysresist value="1" duration2="5000" effectid="204" e="2" noresist="true" preeffect="1" />)"
	R"(</effects><motion name="phburst" speed="50" instant_skill="true" /></skill_template>)";

/**
 * Focused Evasion (a Scout's level-1 skill, m5b2-plan.md §2.4) adds one AttackStatusObserver per position (AlwaysDodgeEffect.java:25-36,
 * AlwaysResistEffect.java:25-37). Each one answers only its own status, counts its value down and ends the whole effect at 0 - which removes both
 * observers (Effect.endEffect -> removeObservers, cycles.toml "AlwaysDodgeEffect$1#effect" / "AlwaysResistEffect$1#effect"). Unused, the effect
 * ends after duration2 = 5000 ms. The player sees the icon come and go.
 */
TEST_F(BuffEffectsTest, FocusedEvasionEndsAtTheFirstDodgeOrResistOrAfterFiveSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5051);
	cp::RecordingAionConnection& client = connect(*player, *accounts.back());
	const model::SkillTemplate* skill = bindSkill(FOCUSED_EVASION_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "AlwaysDodgeEffect");
	ASSERT_EQ(effectOf(*skill, 1).javaClassName(), "AlwaysResistEffect");
	auto controller = player->getObserveController();

	Ref<Effect> dodged = cast(*player, *player, skill, 1);
	ASSERT_TRUE(dodged->isInSuccessEffects(1) && dodged->isInSuccessEffects(2));
	EXPECT_EQ(dodged->getDuration(), 5000);
	std::vector<std::vector<uint8_t>> states = packetsOf<SM_ABNORMAL_STATE>(client);
	ASSERT_EQ(states.size(), 1u);
	AbnormalStateFields shown = decodeAbnormalState(states[0]);
	ASSERT_EQ(shown.effects.size(), 1u);
	EXPECT_EQ(shown.effects[0].skillId, 3195);
	EXPECT_EQ(shown.effects[0].remainingTime, 5000);
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::PARRY)) << "neither observer answers another status";
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::CRITICAL_DODGE)) << "only DODGE itself";
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(3195));
	client.clearSent();
	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::DODGE)) << "the dodge observer takes the hit";
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(3195)) << "--value <= 0: effect.endEffect()";
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::RESIST)) << "the resist observer went with the effect";
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::DODGE));
	states = packetsOf<SM_ABNORMAL_STATE>(client);
	ASSERT_EQ(states.size(), 1u);
	EXPECT_TRUE(decodeAbnormalState(states[0]).effects.empty());

	Ref<Effect> resisted = cast(*player, *player, skill, 1);
	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::RESIST)) << "the resist observer takes the spell";
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(3195));
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::DODGE)) << "the dodge observer went with the effect";

	Ref<Effect> unused = cast(*player, *player, skill, 1);
	advance(4999);
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(3195));
	advance(1);
	EXPECT_TRUE(unused->isEndedByTime());
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::DODGE));
	EXPECT_FALSE(controller->checkAttackStatus(AttackStatus::RESIST));
}

/** The observer's own `value` (the inherited AttackStatusObserver field, not the template's) counts down: value 2 takes two dodges */
TEST_F(BuffEffectsTest, AlwaysDodgeTakesAsManyDodgesAsItsValueAndAlwaysResistAsManyResists) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5061);
	auto twice = [this](int32_t skillId, std::string_view element) {
		return bindSkill(R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="twice" nameId="1" stack="EFFECTS_AL_TWICE_)"
			+ std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" duration="0">)"
			+ R"(<effects><)" + std::string(element) + R"( value="2" duration2="5000" e="1" noresist="true"/></effects></skill_template>)");
	};
	auto controller = player->getObserveController();

	cast(*player, *player, twice(9731, "alwaysdodge"), 1);
	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::DODGE));
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9731)) << "2 - 1 = 1: still active";
	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::DODGE));
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(9731)) << "1 - 1 = 0: ended";

	cast(*player, *player, twice(9732, "alwaysresist"), 1);
	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::RESIST));
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9732));
	EXPECT_TRUE(controller->checkAttackStatus(AttackStatus::RESIST));
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(9732));
}

// ---- HideEffect -----------------------------------------------------------------------------------------------------------------------------

/** 3222 Stealth (skill_templates.xml:54396-54414) without its start and end conditions and its randomtime: HIDE1, bufcount 1, SPEED -40 % */
std::string hideXml(int32_t skillId, std::string_view hideAttributes) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="Stealth" nameId="2287794" cooldownId="175" group="SC_HIDE")"
		+ R"( stack="EFFECTS_AL_HIDE_)" + std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
		+ R"( dispel_category="EXTRA" activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" hostile_type="INDIRECT")"
		+ R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
		+ R"(<properties first_target="ME" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" /><effects>)"
		+ R"(<hide )" + std::string(hideAttributes) + R"( duration2="50000" effectid="165" e="1" basiclvl="1" noresist="true" hoptype="SKILLLV">)"
		+ R"(<change stat="SPEED" func="PERCENT" value="-40" /></hide></effects></skill_template>)";
}

/** A skill the hider casts, built as Skill.java's constructor builds it: `self` is a BUFF of first target ME and target ONLYONE (isSelfBuff) */
std::string castXml(int32_t skillId, bool self, std::string_view extra = {}) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="cast" nameId="1" stack="EFFECTS_AL_CAST_)" + std::to_string(skillId)
		+ R"(" lvl="1" skilltype="MAGICAL" skillsubtype=")" + (self ? "BUFF" : "DEBUFF") + R"(" activation="ACTIVE" )" + std::string(extra)
		+ R"(><properties first_target=")" + (self ? "ME" : "TARGET") + R"(" target_type="ONLYONE"/></skill_template>)";
}

/**
 * A skill the hider starts to cast (HideEffect$1 is a STARTSKILLCAST observer), built as Skill.java's constructor sees it: `self` is a BUFF of
 * first target ME and target ONLYONE (Skill.isSelfBuff), otherwise a DEBUFF of first target TARGET; `effect` is its one effect, whose type
 * HideEffect$1 asks for SHAPECHANGE; `duration` is the cast time an item's skill is checked for.
 */
std::string startedSkillXml(int32_t skillId, bool self, std::string_view effect, std::string_view duration = "0") {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="cast" nameId="1" stack="EFFECTS_AL_CAST_)" + std::to_string(skillId)
		+ R"(" lvl="1" skilltype="MAGICAL" skillsubtype=")" + (self ? "BUFF" : "DEBUFF") + R"(" activation="ACTIVE" duration=")" + std::string(duration)
		+ R"("><properties first_target=")" + (self ? "ME" : "TARGET") + R"(" target_type="ONLYONE"/><effects>)" + std::string(effect)
		+ R"(</effects></skill_template>)";
}

/** An effect that changes no shape, and the <shapechange> of the data's transformation skills (e.g. skill_templates.xml's model 202641) */
constexpr const char* PLAIN_EFFECT_XML = R"(<statboost duration2="10000" e="1" noresist="true"><change stat="BOOST_SPELL_ATTACK" func="ADD" value="1"/></statboost>)";
constexpr const char* SHAPECHANGE_EFFECT_XML = R"(<shapechange model="202641" type="PC" duration2="120000" e="1" noresist="true"/>)";

/**
 * 3222 Stealth on a player, run to its end (HideEffect.java:53-118, Effect.startEffect): BufEffect's SPEED function, the HIDE abnormal state of
 * the controller and the effect, the visual state, SM_PLAYER_STATE to everyone who sees the hider and to the hider itself, then
 * PlayerController.onHide, whose DuelService.fixTeamVisibility (DuelService.java:176-184) returns at once for a hider who is not dueling
 * (tests/playersvc: FixTeamVisibilityOfAHiderWhoIsNotDuelingEndsAtTheOpponentLookup), and after it the observers and the cancel on damage of
 * type 0. Effect.startEffect then schedules the end task: after duration2 the effect ends by time, HideEffect.endEffect undoes the visual state,
 * the HIDE state and the hide (onHideEnd, SM_PLAYER_STATE), and Effect.endEffects removes the SPEED function (CreatureGameStats.endEffect). Before
 * DuelService.fixTeamVisibility was ported it threw out of startEffect ahead of the observers, the cancel on damage and the end task, and the hide
 * stayed on the player for good.
 */
TEST_F(BuffEffectsTest, HideOfAPlayerLastsItsDurationAndEndsWithItsStatesAndStatFunction) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5071);
	cp::RecordingAionConnection& client = connect(*player, *accounts.back());
	const model::SkillTemplate* stealth = bindSkill(hideXml(9741, R"(state="HIDE1" bufcount="1")"));
	ASSERT_EQ(effectOf(*stealth, 0).javaClassName(), "HideEffect");
	const int32_t visible = player->getVisualState();
	ASSERT_EQ(statOf(*player, StatEnum::SPEED, 1000), 1000);
	ASSERT_FALSE(player->getObserveController()->hasObservers());
	client.clearSent();

	Ref<Effect> hide = cast(*player, *player, stealth, 1);
	EXPECT_TRUE(player->getEffectController()->isAbnormalSet(AbnormalState::HIDE));
	EXPECT_NE(hide->getAbnormals(), 0) << "effect.setAbnormal(AbnormalState.HIDE)";
	EXPECT_TRUE(player->isInVisualState(CreatureVisualState::HIDE1));
	EXPECT_EQ(statOf(*player, StatEnum::SPEED, 1000), 600) << "BufEffect.startEffect first: SPEED 1000 * -40 / 100 as a bonus";
	EXPECT_TRUE(hide->isCancelOnDmg()) << "type 0: setCancelOnDmg(true), after onHide";
	ASSERT_TRUE(player->getObserveController()->hasObservers()) << "HideEffect$1-$3 and the cancel observers of the cancel on damage";
	ASSERT_EQ(hide->getDuration(), 50000) << "duration2, which the end task is scheduled for";
	std::vector<std::vector<uint8_t>> playerStates = packetsOf<SM_PLAYER_STATE>(client);
	ASSERT_EQ(playerStates.size(), 1u) << "broadcastPacketAndReceive: the hider is told its own state";
	EXPECT_EQ(decodePlayerState(playerStates[0]).objectId, player->getObjectId());
	EXPECT_EQ(decodePlayerState(playerStates[0]).visualState, player->getVisualState());

	client.clearSent();
	advance(49999);
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9741));
	EXPECT_TRUE(player->isInVisualState(CreatureVisualState::HIDE1));
	advance(1);
	EXPECT_TRUE(hide->isEndedByTime()) << "the end task Effect.startEffect scheduled for duration2";
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(9741));
	EXPECT_FALSE(player->getEffectController()->isAbnormalSet(AbnormalState::HIDE)) << "endEffect: unsetAbnormal(HIDE)";
	EXPECT_EQ(player->getVisualState(), visible) << "endEffect: unsetVisualState(state)";
	EXPECT_EQ(statOf(*player, StatEnum::SPEED, 1000), 1000) << "Effect.endEffects: the SPEED function ends with the effect";
	EXPECT_FALSE(player->getObserveController()->hasObservers())
		<< "Effect.endEffect -> removeObservers: the observers HideEffect added through effect.addObserver leave with the hide";
	playerStates = packetsOf<SM_PLAYER_STATE>(client);
	ASSERT_EQ(playerStates.size(), 1u) << "endEffect: SM_PLAYER_STATE (update visibility)";
	EXPECT_EQ(decodePlayerState(playerStates[0]).visualState, visible);
}

/**
 * What a player's hide does to the creatures that know the hider (PlayerController.onHide -> CreatureController.onHide, and onHideEnd at the end,
 * CreatureController.java:86-95): each of them updates its view of the hider (KnownList.updateVisibleObject). A monster's see state (0) is below
 * HIDE1 (Creature.canSee), so it stops seeing the hider but keeps knowing it, until the end of the hide shows the hider again. An npc stands in
 * for the players around the hider: a player who sees the hider again is sent SM_PLAYER_INFO, which this fixture cannot build (the hider's
 * settings, abyss rank, house and motion parts).
 */
TEST_F(BuffEffectsTest, HideOfAPlayerTakesItOutOfTheSightOfTheCreaturesThatKnowItUntilItEnds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5076);
	connect(*player, *accounts.back());
	Ref<Npc> monster = makeMonster(700615);
	ASSERT_TRUE(KnownListPairing::pair(*monster, *player));
	const model::SkillTemplate* stealth = bindSkill(hideXml(9745, R"(state="HIDE1" bufcount="1")"));
	auto& monsterView = monster->getKnownList();
	ASSERT_TRUE(monsterView.knows(*player));
	ASSERT_TRUE(monsterView.sees(*player));

	cast(*player, *player, stealth, 1);
	EXPECT_FALSE(monsterView.sees(*player)) << "onHide: the monster's updateVisibleObject, and a see state of 0 does not see HIDE1";
	EXPECT_TRUE(monsterView.knows(*player)) << "the hider stays in the monster's known list, only its visibility changes";
	advance(49999);
	ASSERT_TRUE(player->getEffectController()->hasAbnormalEffect(9745));
	EXPECT_FALSE(monsterView.sees(*player)) << "for as long as the hide lasts";
	advance(1);
	ASSERT_FALSE(player->getEffectController()->hasAbnormalEffect(9745));
	EXPECT_TRUE(monsterView.sees(*player)) << "endEffect -> onHideEnd: the monster's updateVisibleObject sees the hider again";
	EXPECT_TRUE(monsterView.knows(*player));
}

/**
 * What ends a player's hide besides its duration (HideEffect.java:96-102, 116-118; Effect.addCancelOnDmgObserver): the hider's own attack
 * (HideEffect$2, for every type) and, for type 0 only, the first hit or damage over time it takes (the ATTACKED and DOT_ATTACKED observers that
 * setCancelOnDmg(true) makes Effect.startEffect add). Type 1 (18 hides of the data) keeps the hide through damage.
 */
TEST_F(BuffEffectsTest, HideOfAPlayerEndsAtItsAttackAndAtDamageUnlessItsTypeKeepsItThroughDamage) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5081);
	Ref<Npc> monster = makeMonster(700612);
	const model::SkillTemplate* stealth = bindSkill(hideXml(9751, R"(state="HIDE1" bufcount="1")"));
	const model::SkillTemplate* keptThroughDamage = bindSkill(hideXml(9752, R"(state="HIDE1" bufcount="1" type="1")"));
	auto observers = player->getObserveController();
	auto hidden = [&](int32_t skillId) { return player->getEffectController()->hasAbnormalEffect(skillId); };
	// the damage over time the DOT_ATTACKED observers are told about (an effect object only; it is never applied)
	Ref<Effect> dot = Effect::create(*monster, Ptr<Creature>(*player), stealth, 1);
	ASSERT_FALSE(observers->hasObservers());

	cast(*player, *player, stealth, 1);
	ASSERT_TRUE(hidden(9751));
	observers->notifyAttackObservers(*monster, 0);
	EXPECT_FALSE(hidden(9751)) << "HideEffect$2: the hider attacked";
	EXPECT_FALSE(player->isInVisualState(CreatureVisualState::HIDE1)) << "endEffect: unsetVisualState(state)";
	EXPECT_FALSE(player->getEffectController()->isAbnormalSet(AbnormalState::HIDE)) << "endEffect: unsetAbnormal(HIDE)";
	EXPECT_EQ(statOf(*player, StatEnum::SPEED, 1000), 1000) << "Effect.endEffects: the SPEED function went with the hide";
	EXPECT_FALSE(observers->hasObservers()) << "removeObservers: no observer outlives the hide that ended by an attack";

	cast(*player, *player, stealth, 1);
	observers->notifyAttackedObservers(*monster, 0);
	EXPECT_FALSE(hidden(9751)) << "type 0: the first hit ends it";
	EXPECT_FALSE(observers->hasObservers()) << "removeObservers: ended by a hit";

	cast(*player, *player, stealth, 1);
	observers->notifyDotAttackedObservers(*monster, *dot);
	EXPECT_FALSE(hidden(9751)) << "type 0: damage over time ends it too";
	EXPECT_FALSE(observers->hasObservers()) << "removeObservers: ended by damage over time";

	Ref<Effect> kept = cast(*player, *player, keptThroughDamage, 1);
	EXPECT_FALSE(kept->isCancelOnDmg()) << "type >= 1";
	observers->notifyAttackedObservers(*monster, 0);
	observers->notifyDotAttackedObservers(*monster, *dot);
	EXPECT_TRUE(hidden(9752)) << "type 1: no cancel on damage";
	observers->notifyAttackObservers(*monster, 0);
	EXPECT_FALSE(hidden(9752)) << "HideEffect$2 is added whatever the type";
	EXPECT_FALSE(observers->hasObservers()) << "removeObservers: type 1 ended by an attack";
}

/**
 * HideEffect$1 (HideEffect.java:79-95), the hider starting a skill: an item's skill (SkillMethod.ITEM) ends the hide if the item is a potion or
 * the skill has a cast time and otherwise leaves it alone without counting it; any other skill ends it if it changes the shape, if it is no self
 * buff, or when `++buffNumber >= bufcount` - so bufcount 1 (3222 Stealth) ends at the first self buff and bufcount 2 at the second.
 */
TEST_F(BuffEffectsTest, HideOfAPlayerEndsAtASkillUnlessItIsASelfBuffWithinItsBuffCount) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5091);
	Ref<Npc> monster = makeMonster(700613);
	const model::SkillTemplate* stealth = bindSkill(hideXml(9761, R"(state="HIDE1" bufcount="1")"));
	const model::SkillTemplate* twoBuffs = bindSkill(hideXml(9762, R"(state="HIDE1" bufcount="2")"));
	const model::SkillTemplate* selfBuff = bindSkill(startedSkillXml(9763, true, PLAIN_EFFECT_XML));
	const model::SkillTemplate* attack = bindSkill(startedSkillXml(9764, false, PLAIN_EFFECT_XML));
	const model::SkillTemplate* shapeChange = bindSkill(startedSkillXml(9765, true, SHAPECHANGE_EFFECT_XML));
	const model::SkillTemplate* castTimeBuff = bindSkill(startedSkillXml(9766, true, PLAIN_EFFECT_XML, "2000"));
	const gameserver::model::templates::item::ItemTemplate* potion =
		itemTemplate(R"(<item_template id="162000002" name="Minor Life Potion" level="10"/>)");
	const gameserver::model::templates::item::ItemTemplate* scroll = itemTemplate(R"(<item_template id="164000073" name="scroll" level="10"/>)");
	ASSERT_TRUE(potion->isPotion());
	ASSERT_FALSE(scroll->isPotion());
	auto hidden = [&](int32_t skillId) { return player->getEffectController()->hasAbnormalEffect(skillId); };
	// Java: Skill.startCast notifies the caster's STARTSKILLCAST observers; an item template makes the skill's method ITEM
	auto start = [&](const model::SkillTemplate* skill, const gameserver::model::templates::item::ItemTemplate* item = nullptr) {
		Ptr<Creature> firstTarget = skill == attack ? Ptr<Creature>(*monster) : Ptr<Creature>(*player);
		player->getObserveController()->notifyStartSkillCastObservers(*model::Skill::create(skill, *player, 1, firstTarget, item));
	};
	// Effect.endEffect -> removeObservers: no observer of an ended hide stays on the player
	auto observed = [&] { return player->getObserveController()->hasObservers(); };
	ASSERT_FALSE(observed());

	cast(*player, *player, stealth, 1);
	start(selfBuff);
	EXPECT_FALSE(hidden(9761)) << "bufcount 1: ++buffNumber (1) >= buffCount (1) at the first self buff";
	EXPECT_FALSE(observed()) << "ended by a self buff";

	cast(*player, *player, twoBuffs, 1);
	start(attack);
	EXPECT_FALSE(hidden(9762)) << "!skill.isSelfBuff(): a skill at a target ends it at once";
	EXPECT_FALSE(observed()) << "ended by a skill at a target";

	cast(*player, *player, twoBuffs, 1);
	start(shapeChange);
	EXPECT_FALSE(hidden(9762)) << "isShapeChange: ends it at once, though a self buff";
	EXPECT_FALSE(observed()) << "ended by a shapechange";

	cast(*player, *player, twoBuffs, 1);
	start(selfBuff);
	EXPECT_TRUE(hidden(9762)) << "bufcount 2: the first self buff is let through (1 < 2)";
	EXPECT_TRUE(player->isInVisualState(CreatureVisualState::HIDE1));
	start(selfBuff);
	EXPECT_FALSE(hidden(9762)) << "bufcount 2: the second self buff ends it (2 >= 2)";
	EXPECT_FALSE(observed()) << "ended by the second self buff";

	cast(*player, *player, stealth, 1);
	start(selfBuff, scroll);
	EXPECT_TRUE(hidden(9761)) << "SkillMethod.ITEM: a non-potion item's skill without a cast time returns before the buff count";
	start(selfBuff, scroll);
	EXPECT_TRUE(hidden(9761)) << "and is not counted: bufcount 1 would have ended at a counted self buff";
	start(selfBuff, potion);
	EXPECT_FALSE(hidden(9761)) << "SkillMethod.ITEM: a potion ends it";
	EXPECT_FALSE(observed()) << "ended by a potion";

	cast(*player, *player, stealth, 1);
	start(castTimeBuff, scroll);
	EXPECT_FALSE(hidden(9761)) << "SkillMethod.ITEM: an item's skill with a cast time (getDuration() > 0) ends it";
	EXPECT_FALSE(observed()) << "ended by an item's skill with a cast time";
}

/**
 * HideEffect$3 (HideEffect.java:103-114), the hider using an item: an item without <actions> never ends the hide; one with actions ends it when
 * the hide's bufcount is 0 or when none of its actions is a <skilluse> ("[4.5] Buff items do not affect Hide II. Hide I is cancelled.").
 */
TEST_F(BuffEffectsTest, HideOfAPlayerEndsAtAnItemUseUnlessItIsASkillItemItsBuffCountAllows) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(5101);
	const model::SkillTemplate* stealth = bindSkill(hideXml(9771, R"(state="HIDE1" bufcount="1")"));
	const model::SkillTemplate* noBuffs = bindSkill(hideXml(9772, R"(state="HIDE1" bufcount="0")"));
	Ref<gameserver::model::gameobjects::Item> plain =
		gameserver::model::gameobjects::Item::create(5102, itemTemplate(R"(<item_template id="182000001" name="plain" level="1"/>)"));
	Ref<gameserver::model::gameobjects::Item> skillItem = gameserver::model::gameobjects::Item::create(5103,
		itemTemplate(R"(<item_template id="162000002" name="Minor Life Potion" level="10"><actions><skilluse level="1" skillid="9889"/></actions>)"
					 R"(</item_template>)"));
	Ref<gameserver::model::gameobjects::Item> recipe = gameserver::model::gameobjects::Item::create(5104,
		itemTemplate(R"(<item_template id="169000001" name="recipe" level="1"><actions><craftlearn recipeid="155000001"/></actions></item_template>)"));
	ASSERT_EQ(plain->getItemTemplate()->getActions(), nullptr);
	ASSERT_NE(skillItem->getItemTemplate()->getActions()->getSkillUseAction(), nullptr);
	ASSERT_EQ(recipe->getItemTemplate()->getActions()->getSkillUseAction(), nullptr);
	auto hidden = [&](int32_t skillId) { return player->getEffectController()->hasAbnormalEffect(skillId); };
	auto use = [&](gameserver::model::gameobjects::Item& item) { player->getObserveController()->notifyItemuseObservers(item); };
	// Effect.endEffect -> removeObservers: no observer of an ended hide stays on the player
	auto observed = [&] { return player->getObserveController()->hasObservers(); };
	ASSERT_FALSE(observed());

	cast(*player, *player, stealth, 1);
	use(*plain);
	EXPECT_TRUE(hidden(9771)) << "no <actions>: nothing to check";
	use(*skillItem);
	EXPECT_TRUE(hidden(9771)) << "bufcount 1 and a <skilluse>: a buff item keeps the hide";
	use(*recipe);
	EXPECT_FALSE(hidden(9771)) << "actions without a <skilluse> end it";
	EXPECT_FALSE(observed()) << "ended by an item without a <skilluse>";

	cast(*player, *player, noBuffs, 1);
	use(*plain);
	EXPECT_TRUE(hidden(9772)) << "no <actions>, whatever the bufcount";
	use(*skillItem);
	EXPECT_FALSE(hidden(9772)) << "bufcount 0: every item with actions ends it";
	EXPECT_FALSE(observed()) << "ended by an item with actions at bufcount 0";
}

/**
 * An npc's hide (HideEffect.java:53-74, 119-143; the post-spawn hides of D7): the npc's visual state and SPEED function, the players who see it are
 * told the abnormal effect; type 0 ends at the npc's first attack (HideEffect$4), skill (HideEffect$5) or damage (setCancelOnDmg -> the cancel
 * observers of Effect.addCancelOnDmgObserver); type 3 adds no observer at all and outlasts all three; the end task ends it after duration2.
 * On the way: a cast aimed at the hider is cancelled (AttackUtil.cancelCastOn), the player who saw the npc is told it is gone
 * (CreatureController.onHide -> KnownList.updateVisibleObject -> SM_DELETE), a target the player still picks within 500 ms is dropped by the
 * delayed AttackUtil.removeTargetFrom(effected, true), and the end shows the npc again (onHideEnd -> SM_NPC_INFO).
 */
TEST_F(BuffEffectsTest, HideOfAnNpcEndsWhenItAttacksOrCastsUnlessItsTypeKeepsIt) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700611);
	cp::RecordingAionConnection& observer = observe(*npc, 5111);
	const model::SkillTemplate* npcHide = bindSkill(hideXml(9781, R"(state="HIDE1")"));
	const model::SkillTemplate* npcHideKept = bindSkill(hideXml(9782, R"(state="HIDE1" type="3")"));
	const model::SkillTemplate* npcSkill = bindSkill(castXml(9783, false, R"(duration="0")"));
	const model::SkillTemplate* victimSkill = bindSkill(castXml(9784, false, R"(duration="2000")"));
	Ref<Player> victim = players.back();
	auto hidden = [&](int32_t skillId) { return npc->getEffectController()->hasAbnormalEffect(skillId); };
	// the victim targets the npc and is casting at it (Java: Skill.startCast sets the effector's casting skill)
	victim->setTarget(Ptr<gameserver::model::gameobjects::VisibleObject>(*npc));
	victim->setCasting(Ptr<model::Skill>(*model::Skill::create(victimSkill, *victim, 1, Ptr<Creature>(*npc), nullptr)));
	ASSERT_TRUE(victim->getCastingSkill());
	observer.clearSent();

	Ref<Effect> first = cast(*npc, *npc, npcHide, 1);
	EXPECT_FALSE(victim->getCastingSkill()) << "AttackUtil.cancelCastOn: the cast at the hider is cancelled";
	std::vector<std::vector<uint8_t>> deleted = packetsOf<network::aion::serverpackets::SM_DELETE>(observer);
	ASSERT_EQ(deleted.size(), 1u) << "CreatureController.onHide: the player no longer sees the npc";
	EXPECT_EQ(deletedObjectOf(deleted[0]), npc->getObjectId());
	victim->setTarget(Ptr<gameserver::model::gameobjects::VisibleObject>(*npc)); // picked again before the client faded the npc out
	advance(499);
	EXPECT_EQ(victim->getTarget().get(), npc.get()) << "removeTargetFrom runs 500 ms after the start";
	advance(1);
	EXPECT_FALSE(victim->getTarget()) << "removeTargetFrom(effected, true): the victim cannot see the hider";
	EXPECT_TRUE(npc->isInVisualState(CreatureVisualState::HIDE1));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::HIDE));
	EXPECT_EQ(statOf(*npc, StatEnum::SPEED, 1000), 600) << "the <change> of the hide, through BufEffect.startEffect";
	EXPECT_TRUE(first->isCancelOnDmg()) << "type 0";
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_EQ(announced.size(), 1u) << "EffectController.addEffect -> broadCastEffects";
	AbnormalEffectFields fields = decodeAbnormalEffect(announced[0]);
	EXPECT_EQ(fields.effectedId, npc->getObjectId());
	EXPECT_EQ(fields.effectType, 1) << "an npc";
	EXPECT_EQ(fields.abnormals, npc->getEffectController()->getAbnormals());
	ASSERT_EQ(fields.effects.size(), 1u);
	EXPECT_EQ(fields.effects[0].skillId, 9781);
	EXPECT_EQ(fields.effects[0].remainingTime, 50000);
	observer.clearSent();
	npc->getObserveController()->notifyAttackObservers(*victim, 0);
	EXPECT_FALSE(hidden(9781)) << "HideEffect$4: the npc attacked";
	EXPECT_FALSE(npc->isInVisualState(CreatureVisualState::HIDE1)) << "endEffect: unsetVisualState(state)";
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::HIDE)) << "endEffect: unsetAbnormal(HIDE)";
	EXPECT_EQ(statOf(*npc, StatEnum::SPEED, 1000), 1000);
	EXPECT_EQ(packetsOf<network::aion::serverpackets::SM_NPC_INFO>(observer).size(), 1u) << "endEffect: onHideEnd shows the npc again";

	cast(*npc, *npc, npcHide, 1);
	npc->getObserveController()->notifyStartSkillCastObservers(*model::Skill::create(npcSkill, *npc, 1, victim, nullptr));
	EXPECT_FALSE(hidden(9781)) << "HideEffect$5: the npc cast a skill";

	cast(*npc, *npc, npcHide, 1);
	npc->getObserveController()->notifyAttackedObservers(*victim, 0);
	EXPECT_FALSE(hidden(9781)) << "type 0: the first damage ends it";

	Ref<Effect> kept = cast(*npc, *npc, npcHideKept, 1);
	EXPECT_FALSE(kept->isCancelOnDmg());
	npc->getObserveController()->notifyAttackObservers(*victim, 0);
	npc->getObserveController()->notifyAttackedObservers(*victim, 0);
	npc->getObserveController()->notifyStartSkillCastObservers(*model::Skill::create(npcSkill, *npc, 1, victim, nullptr));
	EXPECT_TRUE(hidden(9782)) << "type 3: no observer, no cancel on damage";
	advance(50000);
	EXPECT_FALSE(hidden(9782)) << "the end task after duration2";
	EXPECT_FALSE(npc->isInVisualState(CreatureVisualState::HIDE1));
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
