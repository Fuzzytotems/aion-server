// P5-02a, M5b-2 stage 1 part 2, the cast review's fixer run: the ported bodies of the cast lane no other case reached - the condition classes
// (skillengine/condition/*.java) with their Stat2 and Effect overloads, the cost actions and the two periodic actions, the property arms
// SkillCastTest and SkillRulesTest leave out (skillengine/properties/*.java), Skill.isValidTarget's dead and about-to-die arms,
// canUseSkill's counter-skill and item-while-moving arms, startPenaltySkill, and SkillEngine's effect entry points.
//
// The conditions and properties are bound from this file's own <skill_data> (conditionTemplate): a Skill built from any template runs its
// Properties and Conditions, and none of them looks its own template up in SKILL_DATA. SkillEngine's entry points look the id up in SKILL_DATA,
// so their cases use CastTestSupport.h's templates.

#include "CastTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/action/Action.h"
#include "aion/gameserver/skillengine/action/Actions.h"
#include "aion/gameserver/skillengine/condition/Condition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/periodicaction/PeriodicAction.h"
#include "aion/gameserver/skillengine/periodicaction/PeriodicActions.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"

namespace aion::gameserver::skillengine::test {
namespace {

using gameserver::model::Race;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::calc::AdditionStat;
using gameserver::model::stats::calc::functions::IStatFunction;
using gameserver::model::stats::calc::functions::RcStatFunction;
using gameserver::model::stats::calc::functions::StatAddFunction;
using gameserver::model::stats::calc::functions::StatFunctionProxy;
using gameserver::model::stats::container::StatEnum;
using gameserver::model::templates::item::enums::ItemGroup;
using network::aion::serverpackets::SM_CASTSPELL_RESULT;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using properties::Properties_CastState;
using runtime::Ptr;
using runtime::Ref;

// ids below 65536: SM_CASTSPELL_RESULT writes the skill id as H

/** <startconditions><abnormal value="STUN"/></startconditions> */
constexpr int32_t ABNORMAL_CONDITIONS = 61001;
/** <startconditions><back/><front/><race race="ASMODIANS"/></startconditions> */
constexpr int32_t POSITION_CONDITIONS = 61002;
/** noflying, onfly, selfflying FLY / GROUND / ALL, targetflying FLY / GROUND */
constexpr int32_t FLYING_CONDITIONS = 61003;
/** form AVATAR, dp 100, ride_robot, lefthandweapon DUAL / SHIELD, charge 1 / charge 0 */
constexpr int32_t PLAYER_CONDITIONS = 61004;
/** <endconditions><hp value="10" ratio="true"/></endconditions>, <actions>: hpuse 5 + 1/level, hpuse 10 %, mpuse 10 %, hpuse 2,000,000,000 % */
constexpr int32_t RATIO_COSTS = 61005;
/** <periodicactions>: hpuse 30, mpuse 20, hpuse 10 %, mpuse 10 %, hpuse and mpuse 2,000,000,000 %; tslot BUFF (EffectController needs a slot) */
constexpr int32_t PERIODIC_COSTS = 61006;
/** <endconditions><chargeweapon value="53"/><chargearmor value="36"/><polishchargeweapon value="422"/></endconditions> (1282's) */
constexpr int32_t CHARGE_CONDITIONS = 61007;
/** <actions><itemuse itemid="186000246" count="2"/><itemuse itemid="186000246" count="1" expendable="false"/></actions> */
constexpr int32_t ITEM_COSTS = 61008;
/** 186000246, the item of 20 <itemuse> costs of skill_templates.xml */
constexpr int32_t COST_ITEM = 186000246;
/** first_target TARGET, ENEMY, ONLYONE, target_status="STUN SPIN" */
constexpr int32_t STATUS_PROPERTY = 61101;
/** first_target TARGET, relation ALL, ONLYONE, target_species="NPC" */
constexpr int32_t SPECIES_PROPERTY = 61102;
/** first_target MYPET */
constexpr int32_t MYPET_PROPERTY = 61103;
/** first_target MYMASTER */
constexpr int32_t MYMASTER_PROPERTY = 61104;
/** first_target PASSIVE */
constexpr int32_t PASSIVE_PROPERTY = 61105;
/** first_target POINT, target_type POINT, target_distance 5, relation ENEMY */
constexpr int32_t POINT_PROPERTY = 61106;
/** first_target TARGET, ENEMY, AREA, effective_range 10, effective_altitude 3 */
constexpr int32_t ALTITUDE_PROPERTY = 61107;
/** first_target TARGET, relation FRIEND, ONLYONE */
constexpr int32_t FRIEND_PROPERTY = 61108;
/** first_target ME, relation FRIEND, AREA, effective_range 10 */
constexpr int32_t FRIEND_AREA_PROPERTY = 61112;
/** counter_skill="DODGE", first_target ME */
constexpr int32_t COUNTER_SKILL = 61109;
/** penalty_skill_id of an unknown skill, penalty_skill_send_msg false (applyEffectDirectly) */
constexpr int32_t PENALTY_EFFECT_SKILL = 61110;
/** penalty_skill_id INSTANT_SKILL, penalty_skill_send_msg="true" (a PenaltySkill is cast) */
constexpr int32_t PENALTY_CAST_SKILL = 61111;

constexpr int32_t UNKNOWN_PENALTY_SKILL = 99998;

std::string conditionSkillData() {
	auto skill = [](int32_t id, std::string_view extra, std::string_view body) {
		return "<skill_template skill_id=\"" + std::to_string(id) + "\" name=\"c" + std::to_string(id) + "\" nameId=\"1\" stack=\"C"
			+ std::to_string(id) + "\" lvl=\"1\" skilltype=\"PHYSICAL\" skillsubtype=\"BUFF\" activation=\"ACTIVE\" cooldown=\"0\" duration=\"0\" "
			+ std::string(extra) + ">" + std::string(body) + "</skill_template>";
	};
	const std::string enemy = R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE"/>)";
	const std::string me = R"(<properties first_target="ME" target_type="ONLYONE"/>)";
	return "<skill_data>" + skill(ABNORMAL_CONDITIONS, "", enemy + R"(<startconditions><abnormal value="STUN"/></startconditions>)")
		+ skill(POSITION_CONDITIONS, "", enemy + R"(<startconditions><back/><front/><race race="ASMODIANS"/></startconditions>)")
		+ skill(FLYING_CONDITIONS, "",
			me + R"(<startconditions><noflying/><onfly/><selfflying restriction="FLY"/><selfflying restriction="GROUND"/>)"
				 R"(<selfflying restriction="ALL"/><targetflying restriction="FLY"/><targetflying restriction="GROUND"/></startconditions>)")
		+ skill(PLAYER_CONDITIONS, "",
			me + R"(<startconditions><form value="AVATAR"/><dp value="100"/><ride_robot/><lefthandweapon type="DUAL"/>)"
				 R"(<lefthandweapon type="SHIELD"/><charge value="1"/><charge value="0"/></startconditions>)")
		+ skill(RATIO_COSTS, "",
			me + R"(<endconditions><hp value="10" ratio="true"/></endconditions>)"
				 R"(<actions><hpuse value="5" delta="1"/><hpuse value="10" ratio="true"/><mpuse value="10" ratio="true"/>)"
				 R"(<hpuse value="2000000000" ratio="true"/></actions>)")
		+ skill(PERIODIC_COSTS, R"(tslot="BUFF")",
			me + R"(<periodicactions checktime="1000"><hpuse value="30"/><mpuse value="20"/><hpuse value="10" ratio="true"/>)"
				 R"(<mpuse value="10" ratio="true"/><hpuse value="2000000000" ratio="true"/><mpuse value="2000000000" ratio="true"/>)"
				 R"(</periodicactions>)")
		+ skill(CHARGE_CONDITIONS, "",
			me + R"(<endconditions><chargeweapon value="53"/><chargearmor value="36"/><polishchargeweapon value="422"/></endconditions>)")
		+ skill(ITEM_COSTS, "",
			me + R"(<actions><itemuse itemid="186000246" count="2"/><itemuse itemid="186000246" count="1" expendable="false"/></actions>)")
		+ skill(STATUS_PROPERTY, "",
			R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" target_status="STUN SPIN"/>)")
		+ skill(SPECIES_PROPERTY, "",
			R"(<properties first_target="TARGET" first_target_range="25" target_relation="ALL" target_type="ONLYONE" target_species="NPC"/>)")
		+ skill(MYPET_PROPERTY, "", R"(<properties first_target="MYPET" target_type="ONLYONE"/>)")
		+ skill(MYMASTER_PROPERTY, "", R"(<properties first_target="MYMASTER" target_type="ONLYONE"/>)")
		+ skill(PASSIVE_PROPERTY, "", R"(<properties first_target="PASSIVE" target_type="ONLYONE"/>)")
		+ skill(POINT_PROPERTY, "", R"(<properties first_target="POINT" target_relation="ENEMY" target_type="POINT" target_distance="5"/>)")
		+ skill(ALTITUDE_PROPERTY, "",
			R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="AREA" effective_range="10")"
			R"( effective_altitude="3"/>)")
		+ skill(FRIEND_PROPERTY, "", R"(<properties first_target="TARGET" first_target_range="25" target_relation="FRIEND" target_type="ONLYONE"/>)")
		+ skill(FRIEND_AREA_PROPERTY, "", R"(<properties first_target="ME" target_relation="FRIEND" target_type="AREA" effective_range="10"/>)")
		+ skill(COUNTER_SKILL, R"(counter_skill="DODGE")", me)
		+ skill(PENALTY_EFFECT_SKILL, R"(penalty_skill_id="99998")", me)
		+ skill(PENALTY_CAST_SKILL, R"(penalty_skill_id="60002" penalty_skill_send_msg="true")", me) + "</skill_data>";
}

/** Exposes the protected KnownList::addPair, so two objects know each other without the World singleton (AttackSeamTest's KnownListPairing) */
struct KnownListPairing : world::knownlist::KnownList {
	static bool pair(gameserver::model::gameobjects::VisibleObject& a, gameserver::model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

class SkillConditionsTest : public CastTest {
protected:
	void SetUp() override {
		CastTest::SetUp();
		conditionData = xml::bindString<dataholders::SkillData>(conditionContext, conditionSkillData());
	}

	void TearDown() override {
		CastTest::TearDown();
		conditionData.reset();
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return cp::serialized(std::move(packet), client->con()); }

	bool sentMessage(SM_SYSTEM_MESSAGE&& packet) {
		std::vector<uint8_t> expected = message(std::move(packet));
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == expected)
				return true;
		return false;
	}

	void setMp(int32_t value) {
		caster.player->getLifeStats()->setCurrentMp(value);
		ASSERT_EQ(currentMp(), value);
		(*client)->clearSent();
	}

	const model::SkillTemplate* conditionTemplate(int32_t skillId) { return conditionData->getSkillTemplate(skillId); }

	/** A Skill of this file's own templates: Java `new Skill(template, player, target, level)`, no skill list involved */
	Ref<model::Skill> conditionSkill(int32_t skillId, Ptr<Creature> firstTarget = nullptr) {
		return model::Skill::create(conditionTemplate(skillId), *caster.player, firstTarget, 1);
	}

	const condition::Condition& startCondition(int32_t skillId, size_t index) {
		return *conditionTemplate(skillId)->getStartconditions()->getConditions()[index];
	}

	/**
	 * Puts a weapon of the given group into the player's main hand as Java's PlayerService loads an equipped row: Equipment.onLoadHandler, after
	 * teaching the weapon skills the group requires (CriticalProcEffectTest.cpp's equipMainHand). `improve` is the item template's <improve>
	 * element: with one, the Item constructor creates the ChargeInfo (Item.java:161-168).
	 */
	Ref<Item> equipMainHand(Player& player, ItemGroup itemGroup, int32_t itemId, int32_t itemObjId, std::string_view improve = "") {
		namespace m = gameserver::model;
		std::vector<Ptr<m::skill::PlayerSkillEntry>> weaponSkills;
		for (int32_t skillId : m::templates::item::enums::getRequiredSkills(itemGroup)) {
			Ref<m::skill::PlayerSkillEntry> entry = m::skill::PlayerSkillEntry::create(skillId, 1, 0, m::gameobjects::Persistable_PersistentState::NOACTION);
			weaponSkills.push_back(Ptr<m::skill::PlayerSkillEntry>(entry));
			learned.push_back(std::move(entry));
		}
		player.setSkillList(m::skill::PlayerSkillList::create(weaponSkills));
		const std::string xmlText = R"(<item_template id=")" + std::to_string(itemId) + R"(" item_group=")"
			+ std::string(xml::EnumTraits<ItemGroup>::names[static_cast<size_t>(itemGroup)]) + R"(">)" + std::string(improve) + "</item_template>";
		const m::templates::item::ItemTemplate* itemTemplate = xml::bindString<m::templates::item::ItemTemplate>(itemContext, xmlText).release();
		Ref<Item> weapon = Item::create(itemObjId, itemTemplate);
		weapon->setEquipmentSlot(m::gameobjects::player::detail::MAIN_HAND);
		player.getEquipment().onLoadHandler(*weapon);
		EXPECT_TRUE(player.getEquipment().getMainHandWeaponType().has_value()) << "onLoadHandler put the weapon back into the inventory";
		items.push_back(weapon);
		return weapon;
	}

	xml::LoadContext conditionContext;
	xml::LoadContext itemContext;
	std::unique_ptr<dataholders::SkillData> conditionData;
	std::vector<Ref<Item>> items;
};

// ------------------------------------------------------------------------------------------------------------------------- stat and effect checks

TEST_F(SkillConditionsTest, AWeaponConditionOfAStatFunctionChecksTheOwnersMainHand) {
	// WeaponCondition.validate(Stat2, IStatFunction) -> isValidWeapon(stat.getOwner()) (WeaponCondition.java:37-52): the weapon-mastery passives'
	// stat functions ask it on every stat calculation. Conditions.validate(Stat2, ...) asks every condition (Conditions.java:59-66); the other
	// three of CONDITIONS_SKILL's list (target, combatcheck, move_casting) have no stat check, Condition.validate(Stat2, ...) answers true
	const condition::Conditions& conditions = *skillTemplate(CONDITIONS_SKILL)->getStartconditions();
	const condition::Condition& weapon = *conditions.getConditions()[0];
	Ref<IStatFunction> function = RcStatFunction<StatAddFunction>::create(StatEnum::PHYSICAL_ATTACK, 10, true);

	AdditionStat bareHanded(StatEnum::PHYSICAL_ATTACK, 100.0f, *caster.player);
	EXPECT_FALSE(weapon.validate(bareHanded, *function)) << "Java List.contains(null) is false";
	EXPECT_FALSE(conditions.validate(bareHanded, *function));

	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	AdditionStat npcStat(StatEnum::PHYSICAL_ATTACK, 100.0f, *npc);
	EXPECT_TRUE(weapon.validate(npcStat, *function)) << "for npcs we don't validate weapon";
	EXPECT_TRUE(conditions.validate(npcStat, *function));

	equipMainHand(*caster.player, ItemGroup::SWORD, 100000094, 810001);
	AdditionStat withSword(StatEnum::PHYSICAL_ATTACK, 100.0f, *caster.player);
	EXPECT_TRUE(weapon.validate(withSword, *function)) << "SWORD is in weapon=\"SWORD MACE\"";
	EXPECT_TRUE(conditions.validate(withSword, *function));

	cp::PlayerFixture lancer = cp::makePlayer(410011, 9411, "Lancer");
	equipMainHand(*lancer.player, ItemGroup::POLEARM, 101300001, 810002);
	AdditionStat withPolearm(StatEnum::PHYSICAL_ATTACK, 100.0f, *lancer.player);
	EXPECT_FALSE(weapon.validate(withPolearm, *function));
}

TEST_F(SkillConditionsTest, ConditionsOfAnEffectAskTheEffectedsAbnormalState) {
	// Conditions.validate(Effect) (Conditions.java:68-75) and AbnormalStateCondition.validate(Effect): the effected's effect controller
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	const condition::Conditions& conditions = *conditionTemplate(ABNORMAL_CONDITIONS)->getStartconditions();
	Ref<model::Effect> effect = model::Effect::create(*caster.player, npc, conditionTemplate(ABNORMAL_CONDITIONS), 1);
	EXPECT_FALSE(conditions.validate(*effect)) << "not stunned";
	npc->getEffectController()->setAbnormal(effect::AbnormalState::STUN);
	EXPECT_TRUE(conditions.validate(*effect));

	// a condition without an effect check answers true (Condition.validate(Effect), Condition.java): the weapon of CONDITIONS_SKILL
	EXPECT_TRUE(skillTemplate(CONDITIONS_SKILL)->getStartconditions()->validate(*effect));
}

TEST_F(SkillConditionsTest, AbnormalStateConditionAsksTheFirstTarget) {
	// AbnormalStateCondition.validate(Skill) (AbnormalStateCondition.java:18-23): false without a first target
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	const condition::Condition& stunned = startCondition(ABNORMAL_CONDITIONS, 0);
	EXPECT_FALSE(stunned.validate(*conditionSkill(ABNORMAL_CONDITIONS, npc)));
	npc->getEffectController()->setAbnormal(effect::AbnormalState::STUN);
	EXPECT_TRUE(stunned.validate(*conditionSkill(ABNORMAL_CONDITIONS, npc)));
	EXPECT_FALSE(stunned.validate(*conditionSkill(ABNORMAL_CONDITIONS))) << "no first target";
}

TEST_F(SkillConditionsTest, BackFrontAndRaceConditionsLookAtTheFirstTargetOrTheEffected) {
	// BackCondition / FrontCondition: PositionUtil.isBehind(effector, target) / isInFrontOf(effector, target) against the target's heading
	// (PositionUtil.java:30-60, heading * 3 degrees). The caster stands at x 100, the npc at x 110.
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	const condition::Condition& back = startCondition(POSITION_CONDITIONS, 0);
	const condition::Condition& front = startCondition(POSITION_CONDITIONS, 1);
	const condition::Condition& race = startCondition(POSITION_CONDITIONS, 2);

	npc->getPosition()->setH(0); // 0 degrees: the npc looks away from the caster
	Ref<model::Effect> effect = model::Effect::create(*caster.player, npc, conditionTemplate(POSITION_CONDITIONS), 1);
	EXPECT_TRUE(back.validate(*conditionSkill(POSITION_CONDITIONS, npc)));
	EXPECT_FALSE(front.validate(*conditionSkill(POSITION_CONDITIONS, npc)));
	EXPECT_TRUE(back.validate(*effect));
	EXPECT_FALSE(front.validate(*effect));

	npc->getPosition()->setH(60); // 180 degrees: it faces the caster
	EXPECT_FALSE(back.validate(*conditionSkill(POSITION_CONDITIONS, npc)));
	EXPECT_TRUE(front.validate(*conditionSkill(POSITION_CONDITIONS, npc)));
	EXPECT_FALSE(back.validate(*effect));
	EXPECT_TRUE(front.validate(*effect));

	EXPECT_FALSE(back.validate(*conditionSkill(POSITION_CONDITIONS))) << "no first target";
	EXPECT_FALSE(front.validate(*conditionSkill(POSITION_CONDITIONS)));

	// RaceCondition (RaceCondition.java:25-50): race="ASMODIANS" matches the first target's / the effected's race
	cp::PlayerFixture asmodian = cp::makePlayer(410012, 9412, "Asmo", Race::ASMODIANS);
	cp::PlayerFixture elyos = cp::makePlayer(410013, 9413, "Ely");
	EXPECT_TRUE(race.validate(*conditionSkill(POSITION_CONDITIONS, asmodian.player)));
	EXPECT_FALSE(race.validate(*conditionSkill(POSITION_CONDITIONS, elyos.player)));
	EXPECT_FALSE(race.validate(*conditionSkill(POSITION_CONDITIONS))) << "no first target";
	EXPECT_TRUE(race.validate(*model::Effect::create(*caster.player, asmodian.player, conditionTemplate(POSITION_CONDITIONS), 1)));
	EXPECT_FALSE(race.validate(*effect)) << "the npc's race";
}

TEST_F(SkillConditionsTest, FlyingConditionsTellFlyingFromGliding) {
	// Player.isFlying() is `flyState != 0`, isInFlyingState() is the FLYING bit alone (Player.java:727-739): gliding is flying but not the
	// flying state. NoFlyingCondition asks isFlying, OnFlyCondition and SelfFlyingCondition isInFlyingState of the effector, TargetFlyingCondition
	// isFlying of the first target (the *Condition.java files)
	using gameserver::model::gameobjects::state::FlyState;
	const condition::Condition& noFlying = startCondition(FLYING_CONDITIONS, 0);
	const condition::Condition& onFly = startCondition(FLYING_CONDITIONS, 1);
	const condition::Condition& selfFly = startCondition(FLYING_CONDITIONS, 2);
	const condition::Condition& selfGround = startCondition(FLYING_CONDITIONS, 3);
	const condition::Condition& selfAll = startCondition(FLYING_CONDITIONS, 4);
	const condition::Condition& targetFly = startCondition(FLYING_CONDITIONS, 5);
	const condition::Condition& targetGround = startCondition(FLYING_CONDITIONS, 6);
	cp::PlayerFixture other = cp::makePlayer(410014, 9414, "Other");
	Ref<IStatFunction> function = RcStatFunction<StatAddFunction>::create(StatEnum::SPEED, 10, true);
	AdditionStat speed(StatEnum::SPEED, 100.0f, *caster.player);
	auto cast = [&] { return conditionSkill(FLYING_CONDITIONS, other.player); };
	auto effect = [&] { return model::Effect::create(*caster.player, other.player, conditionTemplate(FLYING_CONDITIONS), 1); };

	{
		SCOPED_TRACE("on the ground");
		EXPECT_TRUE(noFlying.validate(*cast()));
		EXPECT_FALSE(onFly.validate(*cast()));
		EXPECT_FALSE(onFly.validate(speed, *function));
		EXPECT_FALSE(selfFly.validate(*cast()));
		EXPECT_TRUE(selfGround.validate(*cast()));
		EXPECT_TRUE(selfAll.validate(*cast())) << "ALL: no restriction";
		EXPECT_FALSE(targetFly.validate(*cast()));
		EXPECT_TRUE(targetGround.validate(*cast()));
	}
	caster.player->setFlyState(FlyState::GLIDING);
	other.player->setFlyState(FlyState::GLIDING);
	{
		SCOPED_TRACE("gliding");
		EXPECT_FALSE(noFlying.validate(*cast()));
		EXPECT_FALSE(onFly.validate(*cast()));
		EXPECT_FALSE(onFly.validate(speed, *function)) << "gliding is not the flying state";
		EXPECT_FALSE(selfFly.validate(*cast()));
		EXPECT_TRUE(selfGround.validate(*cast()));
		EXPECT_FALSE(selfFly.validate(*effect())) << "the effector glides";
		EXPECT_TRUE(selfGround.validate(*effect()));
		EXPECT_TRUE(targetFly.validate(*cast())) << "a gliding target is flying";
		EXPECT_FALSE(targetGround.validate(*cast()));
		EXPECT_FALSE(noFlying.validate(*effect())) << "the effected glides";
		EXPECT_FALSE(onFly.validate(*effect()));
		EXPECT_TRUE(targetFly.validate(*effect()));
	}
	caster.player->unsetFlyState(FlyState::GLIDING);
	caster.player->setFlyState(FlyState::FLYING);
	{
		SCOPED_TRACE("flying");
		EXPECT_FALSE(noFlying.validate(*cast()));
		EXPECT_TRUE(onFly.validate(*cast()));
		EXPECT_TRUE(onFly.validate(speed, *function)) << "OnFlyCondition.validate(Stat2): the stat owner";
		EXPECT_TRUE(selfFly.validate(*cast()));
		EXPECT_FALSE(selfGround.validate(*cast()));
		EXPECT_TRUE(selfFly.validate(*effect())) << "SelfFlyingCondition.validate(Effect): the effector";
		EXPECT_FALSE(selfGround.validate(*effect()));
		EXPECT_FALSE(onFly.validate(*effect())) << "OnFlyCondition.validate(Effect): the effected, who only glides";
	}
	EXPECT_FALSE(targetFly.validate(*conditionSkill(FLYING_CONDITIONS))) << "no first target";
	EXPECT_FALSE(targetGround.validate(*conditionSkill(FLYING_CONDITIONS)));
}

TEST_F(SkillConditionsTest, PlayerOnlyConditions) {
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> npcSkill = model::Skill::create(conditionTemplate(PLAYER_CONDITIONS), *npc, 1, nullptr, nullptr);
	const condition::Condition& form = startCondition(PLAYER_CONDITIONS, 0);
	const condition::Condition& dp = startCondition(PLAYER_CONDITIONS, 1);
	const condition::Condition& robot = startCondition(PLAYER_CONDITIONS, 2);
	const condition::Condition& dual = startCondition(PLAYER_CONDITIONS, 3);
	const condition::Condition& shield = startCondition(PLAYER_CONDITIONS, 4);

	// FormCondition (FormCondition.java:27-37): a player must be transformed into the form; an npc always passes
	EXPECT_FALSE(form.validate(*conditionSkill(PLAYER_CONDITIONS)));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_THIS_FORM()));
	EXPECT_TRUE(form.validate(*npcSkill));

	// DpCondition (DpCondition.java:22-24): getDp() >= value; a starting class holds no DP, so the caster becomes a Gladiator
	caster.commonData->setPlayerClass(gameserver::model::PlayerClass::GLADIATOR);
	caster.player->getCommonData()->setDp(99);
	EXPECT_FALSE(dp.validate(*conditionSkill(PLAYER_CONDITIONS)));
	caster.player->getCommonData()->setDp(100);
	EXPECT_TRUE(dp.validate(*conditionSkill(PLAYER_CONDITIONS)));

	// RideRobotCondition (RideRobotCondition.java:18-24): a player must be in robot mode; an npc always passes
	EXPECT_FALSE(robot.validate(*conditionSkill(PLAYER_CONDITIONS)));
	EXPECT_TRUE(robot.validate(*npcSkill));

	// LeftHandCondition (LeftHandCondition.java:26-50): DUAL wants an off-hand weapon or a two-handed main weapon, SHIELD a shield; anything
	// else - an npc too - is false
	(*client)->clearSent();
	EXPECT_FALSE(dual.validate(*conditionSkill(PLAYER_CONDITIONS)));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NEED_DUAL_WEAPON()));
	EXPECT_FALSE(shield.validate(*conditionSkill(PLAYER_CONDITIONS)));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NEED_SHIELD()));
	EXPECT_FALSE(dual.validate(*npcSkill));
	EXPECT_FALSE(shield.validate(*npcSkill));
	equipMainHand(*caster.player, ItemGroup::POLEARM, 101300001, 810003);
	EXPECT_TRUE(dual.validate(*conditionSkill(PLAYER_CONDITIONS))) << "a two-handed main weapon counts as dual";
}

TEST_F(SkillConditionsTest, ItemChargeConditionOnlyAppliesToTheStatFunctionsOfAnItem) {
	// ItemChargeCondition (ItemChargeCondition.java:21-33): false for a skill and for a stat function whose owner is no Item; an Item's charge
	// level against the value (an item without ChargeInfo has level 0, Item.java:746-750)
	const condition::Condition& chargeOne = startCondition(PLAYER_CONDITIONS, 5);
	const condition::Condition& chargeZero = startCondition(PLAYER_CONDITIONS, 6);
	EXPECT_FALSE(chargeZero.validate(*conditionSkill(PLAYER_CONDITIONS)));

	Ref<IStatFunction> function = RcStatFunction<StatAddFunction>::create(StatEnum::PHYSICAL_ATTACK, 10, true);
	AdditionStat stat(StatEnum::PHYSICAL_ATTACK, 100.0f, *caster.player);
	EXPECT_FALSE(chargeZero.validate(stat, *function)) << "no owner";

	Ref<Item> sword = equipMainHand(*caster.player, ItemGroup::SWORD, 100000094, 810004);
	Ref<StatFunctionProxy> ofItem = StatFunctionProxy::create(Ptr<gameserver::model::stats::calc::StatOwner>(sword), *function);
	ASSERT_EQ(sword->getChargeLevel(), 0);
	EXPECT_TRUE(chargeZero.validate(stat, *ofItem)) << "0 >= 0";
	EXPECT_FALSE(chargeOne.validate(stat, *ofItem)) << "0 >= 1";
}

TEST_F(SkillConditionsTest, ChargeConditionsBurnTheChargeOfTheMainWeapon) {
	// ChargeWeaponCondition / PolishChargeCondition / ChargeArmorCondition (the three *.java files): every equipped weapon (armor) with a
	// ChargeInfo, except the off hands, loses `value` charge points; the conditions always pass. An item template with <improve level="1"> has
	// a ChargeInfo from its constructor (Item.java:161-168)
	Ref<Item> weapon =
		equipMainHand(*caster.player, ItemGroup::SWORD, 100000095, 810005, R"(<improve way="1" level="1" burn_attack="8" burn_defend="3"/>)");
	ASSERT_TRUE(weapon->getConditioningInfo());
	weapon->getConditioningInfo()->updateChargePoints(1000);
	ASSERT_EQ(weapon->getChargePoints(), 1000);
	const condition::Conditions& conditions = *conditionTemplate(CHARGE_CONDITIONS)->getEndConditions();
	EXPECT_TRUE(conditions.getConditions()[0]->validate(*conditionSkill(CHARGE_CONDITIONS)));
	EXPECT_EQ(weapon->getChargePoints(), 947) << "chargeweapon value 53";
	EXPECT_TRUE(conditions.getConditions()[1]->validate(*conditionSkill(CHARGE_CONDITIONS)));
	EXPECT_EQ(weapon->getChargePoints(), 947) << "chargearmor burns armor only";
	EXPECT_TRUE(conditions.getConditions()[2]->validate(*conditionSkill(CHARGE_CONDITIONS)));
	EXPECT_EQ(weapon->getChargePoints(), 947) << "polishchargeweapon burns an idian stone, which this weapon has none of";

	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	EXPECT_TRUE(conditions.getConditions()[0]->validate(*model::Skill::create(conditionTemplate(CHARGE_CONDITIONS), *npc, 1, nullptr, nullptr)));
}

// ------------------------------------------------------------------------------------------------------------------------- costs

TEST_F(SkillConditionsTest, RatioCostsArePercentagesOfTheMaximum) {
	int32_t maxHp = caster.player->getLifeStats()->getMaxHp();
	int32_t maxMp = caster.player->getLifeStats()->getMaxMp();
	ASSERT_EQ(maxHp, 244) << "the fixture's level-1 Warrior";
	ASSERT_EQ(maxMp, 210);
	const condition::Condition& hpRatio = *conditionTemplate(RATIO_COSTS)->getEndConditions()->getConditions()[0];
	const std::vector<std::unique_ptr<action::Action>>& actions = conditionTemplate(RATIO_COSTS)->getActions()->getActions();

	// HpCondition.getCost, ratio: (maxHp * valueWithDelta) / 100 in int arithmetic (HpCondition.java:49-54) = 24
	caster.player->getLifeStats()->setCurrentHp(200);
	EXPECT_TRUE(hpRatio.validate(*conditionSkill(RATIO_COSTS)));
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 176);

	// HpUseAction.getCost: value + delta * level = 5 + 1 * 3; ratio: (int) (valueWithDelta / 100f * maxHp) = 24 (HpUseAction.java:49-54)
	Ref<model::Skill> level3 = model::Skill::create(conditionTemplate(RATIO_COSTS), *caster.player, nullptr, 3);
	EXPECT_TRUE(actions[0]->act(*level3));
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 168);
	EXPECT_TRUE(actions[1]->act(*conditionSkill(RATIO_COSTS)));
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 144);

	// MpUseAction.getCost, ratio: maxMp * valueWithDelta / 100 = 21 (MpUseAction.java:48-57)
	caster.player->getLifeStats()->setCurrentMp(100);
	EXPECT_TRUE(actions[2]->act(*conditionSkill(RATIO_COSTS)));
	EXPECT_EQ(currentMp(), 79);

	// an HP cost the caster cannot pay without dying is refused (canAct: currentHp <= cost), and an npc is never refused
	caster.player->getLifeStats()->setCurrentHp(8);
	(*client)->clearSent();
	EXPECT_FALSE(actions[0]->canAct(*level3));
	EXPECT_FALSE(actions[0]->act(*level3));
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 8);
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_HP()));
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ptr<Creature>(npc)->getLifeStats()->setCurrentHp(5);
	EXPECT_TRUE(actions[0]->act(*model::Skill::create(conditionTemplate(RATIO_COSTS), *npc, 3, nullptr, nullptr)));
	EXPECT_EQ(Ptr<Creature>(npc)->getLifeStats()->getCurrentHp(), 1)
		<< "an npc pays what it has, down to 1 hp (USED_HP's minHp, CreatureLifeStats.java:101)";
}

TEST_F(SkillConditionsTest, ARatioCostBeyondTheIntRangeSaturatesLikeJavasCast) {
	// HpUseAction.getCost, ratio: (int) (2_000_000_000 / 100f * 244) = (int) 4.88e9f, which Java saturates to Integer.MAX_VALUE (JLS 5.1.3): no
	// player can pay it
	caster.player->getLifeStats()->setCurrentHp(200);
	(*client)->clearSent();
	const action::Action& huge = *conditionTemplate(RATIO_COSTS)->getActions()->getActions()[3];
	EXPECT_FALSE(huge.canAct(*conditionSkill(RATIO_COSTS)));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_HP()));
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 200);
}

TEST_F(SkillConditionsTest, PeriodicCostsArePaidUntilTheyCannotBeAndThenEndTheEffect) {
	// HpUsePeriodicAction / MpUsePeriodicAction.act (the two *.java files): value, or (int) (max * (value / 100f)) with ratio; less than that
	// left ends the effect (Effect.endEffect -> EffectController.clearEffect) and takes nothing
	const std::vector<std::unique_ptr<periodicaction::PeriodicAction>>& actions =
		conditionTemplate(PERIODIC_COSTS)->getPeriodicActions()->getPeriodicActions();
	auto addedEffect = [&] {
		Ref<model::Effect> effect = model::Effect::create(*caster.player, caster.player, conditionTemplate(PERIODIC_COSTS), 1);
		caster.player->getEffectController()->addEffect(*effect);
		return effect;
	};
	caster.player->getLifeStats()->setCurrentHp(100);
	caster.player->getLifeStats()->setCurrentMp(100);
	Ref<model::Effect> effect = addedEffect();
	ASSERT_EQ(caster.player->getEffectController()->getAllEffects().size(), 1u);

	actions[0]->act(*effect);
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 70) << "hpuse 30";
	actions[1]->act(*effect);
	EXPECT_EQ(currentMp(), 80) << "mpuse 20";
	actions[2]->act(*effect);
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 46) << "hpuse 10 %: (int) (244 * 0.1f)";
	actions[3]->act(*effect);
	EXPECT_EQ(currentMp(), 59) << "mpuse 10 %: (int) (210 * 0.1f)";
	EXPECT_EQ(caster.player->getEffectController()->getAllEffects().size(), 1u) << "still running";

	caster.player->getLifeStats()->setCurrentHp(29);
	actions[0]->act(*effect);
	EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 29) << "29 < 30: nothing taken";
	EXPECT_TRUE(caster.player->getEffectController()->getAllEffects().empty()) << "the effect ended";

	Ref<model::Effect> mpEffect = addedEffect();
	caster.player->getLifeStats()->setCurrentMp(19);
	actions[1]->act(*mpEffect);
	EXPECT_EQ(currentMp(), 19);
	EXPECT_TRUE(caster.player->getEffectController()->getAllEffects().empty());
}

TEST_F(SkillConditionsTest, APeriodicRatioCostBeyondTheIntRangeSaturatesAndEndsTheEffect) {
	// (int) (244 * (2e9f / 100f)) and (int) (210 * 2e7f) exceed the int range: Java's cast saturates to Integer.MAX_VALUE, more than anyone has
	const std::vector<std::unique_ptr<periodicaction::PeriodicAction>>& actions =
		conditionTemplate(PERIODIC_COSTS)->getPeriodicActions()->getPeriodicActions();
	for (size_t index : {size_t{4}, size_t{5}}) {
		SCOPED_TRACE(index == 4 ? "hpuse" : "mpuse");
		caster.player->getLifeStats()->setCurrentHp(200);
		caster.player->getLifeStats()->setCurrentMp(150);
		Ref<model::Effect> effect = model::Effect::create(*caster.player, caster.player, conditionTemplate(PERIODIC_COSTS), 1);
		caster.player->getEffectController()->addEffect(*effect);
		actions[index]->act(*effect);
		EXPECT_EQ(caster.player->getLifeStats()->getCurrentHp(), 200);
		EXPECT_EQ(currentMp(), 150);
		EXPECT_TRUE(caster.player->getEffectController()->getAllEffects().empty()) << "the cost cannot be paid: the effect ended";
	}
}

TEST_F(SkillConditionsTest, AnItemCostCountsTheItemsOfTheInventory) {
	// ItemUseAction (ItemUseAction.java:32-49): canAct wants `count` of the item in the inventory, STR_SKILL_NOT_ENOUGH_ITEM with the item's name
	// otherwise; act takes them (decreaseByItemId), or with expendable="false" only checks; an npc is never asked. Taking them is not observable
	// here: Storage.decreaseByItemId sends the inventory update through ItemPacketService, whose bodies are AION_UNPORTED (P5-07's), and so is
	// the add path, which is why the items are loaded with onLoadHandler (the DAO's path, no packet)
	if (dataholders::DataManager::ITEM_DATA)
		GTEST_SKIP() << "another test of this process published ITEM_DATA";
	dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext,
		R"(<item_templates><item_template id="186000246" name="medal" level="1"/></item_templates>)"));
	struct Unpublish {
		~Unpublish() { dataholders::DataManager::ITEM_DATA.resetForTests(); }
	} unpublish;
	const std::vector<std::unique_ptr<action::Action>>& actions = conditionTemplate(ITEM_COSTS)->getActions()->getActions();
	const action::Action& takeTwo = *actions[0];
	const action::Action& checkOne = *actions[1];

	(*client)->clearSent();
	EXPECT_FALSE(takeTwo.canAct(*conditionSkill(ITEM_COSTS)));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_ITEM(dataholders::DataManager::ITEM_DATA->getItemTemplate(COST_ITEM)->getL10n())));
	EXPECT_FALSE(takeTwo.act(*conditionSkill(ITEM_COSTS)));
	EXPECT_FALSE(checkOne.act(*conditionSkill(ITEM_COSTS))) << "expendable=\"false\": act is canAct";
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	EXPECT_TRUE(takeTwo.act(*model::Skill::create(conditionTemplate(ITEM_COSTS), *npc, 1, nullptr, nullptr)));

	Ref<Item> medal = Item::create(810010, dataholders::DataManager::ITEM_DATA->getItemTemplate(COST_ITEM), 1, false, 0);
	caster.player->getInventory().onLoadHandler(*medal);
	ASSERT_EQ(caster.player->getInventory().getItemCountByItemId(COST_ITEM), 1);
	EXPECT_TRUE(checkOne.canAct(*conditionSkill(ITEM_COSTS)));
	EXPECT_TRUE(checkOne.act(*conditionSkill(ITEM_COSTS)));
	EXPECT_EQ(caster.player->getInventory().getItemCountByItemId(COST_ITEM), 1) << "expendable=\"false\" takes nothing";
	(*client)->clearSent();
	EXPECT_FALSE(takeTwo.canAct(*conditionSkill(ITEM_COSTS))) << "1 < 2";
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_ITEM(dataholders::DataManager::ITEM_DATA->getItemTemplate(COST_ITEM)->getL10n())));

	Ref<Item> moreMedals = Item::create(810011, dataholders::DataManager::ITEM_DATA->getItemTemplate(COST_ITEM), 1, false, 0);
	caster.player->getInventory().onLoadHandler(*moreMedals);
	ASSERT_EQ(caster.player->getInventory().getItemCountByItemId(COST_ITEM), 2);
	EXPECT_TRUE(takeTwo.canAct(*conditionSkill(ITEM_COSTS))) << "2 of 2";
}

// ------------------------------------------------------------------------------------------------------------------------- properties

TEST_F(SkillConditionsTest, TargetStatusKeepsOnlyTargetsInOneOfTheStates) {
	// TargetStatusProperty.set (TargetStatusProperty.java:20-33): targets without any of STUN / SPIN are dropped; a dropped first target fails
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> unstunned = conditionSkill(STATUS_PROPERTY, npc);
	EXPECT_FALSE(unstunned->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(unstunned->getEffectedList().isEmpty());

	npc->getEffectController()->setAbnormal(effect::AbnormalState::SPIN);
	Ref<model::Skill> spinning = conditionSkill(STATUS_PROPERTY, npc);
	EXPECT_TRUE(spinning->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(spinning->getEffectedList().size(), 1u);
}

TEST_F(SkillConditionsTest, TargetSpeciesDropsTheOtherKind) {
	// TargetSpeciesProperty.set (TargetSpeciesProperty.java:11-17): target_species="NPC" drops players; the ONLYONE skill then has no target
	// left and Skill.validateEffectedList refuses (Skill.java:208-214)
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	cp::PlayerFixture other = cp::makePlayer(410015, 9415, "Other");
	place(*other.player, 105.0f, 100.0f, 50.0f);
	Ref<model::Skill> onNpc = conditionSkill(SPECIES_PROPERTY, npc);
	EXPECT_TRUE(onNpc->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(onNpc->getEffectedList().size(), 1u);

	Ref<model::Skill> onPlayer = conditionSkill(SPECIES_PROPERTY, other.player);
	EXPECT_FALSE(onPlayer->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(onPlayer->getEffectedList().isEmpty());
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID()));
}

TEST_F(SkillConditionsTest, FirstTargetPetMasterAndPassive) {
	// FirstTargetProperty.set (FirstTargetProperty.java:104-127): MYPET needs the player's summon (STR_SKILL_INVALID_TARGET_PET_ONLY), MYMASTER a
	// summon's master, and neither is anything for an npc; PASSIVE makes the effector the first target
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	EXPECT_FALSE(conditionSkill(MYPET_PROPERTY, npc)->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_PET_ONLY()));
	EXPECT_FALSE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID())) << "refused by the property, before the effected list is checked";
	EXPECT_FALSE(model::Skill::create(conditionTemplate(MYPET_PROPERTY), *npc, 1, nullptr, nullptr)->canUseSkill(Properties_CastState::CAST_START));
	(*client)->clearSent();
	EXPECT_FALSE(conditionSkill(MYMASTER_PROPERTY, npc)->canUseSkill(Properties_CastState::CAST_START)) << "a player is no summon";
	EXPECT_TRUE(sent().empty()) << "MYMASTER refuses silently";

	Ref<model::Skill> passive = conditionSkill(PASSIVE_PROPERTY, npc);
	EXPECT_TRUE(passive->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(passive->getFirstTarget(), Ptr<Creature>(caster.player));
	std::vector<Ptr<Creature>> effected = passive->getEffectedList().snapshot();
	ASSERT_EQ(effected.size(), 1u);
	EXPECT_EQ(effected[0], Ptr<Creature>(caster.player));
}

TEST_F(SkillConditionsTest, APointSkillHitsTheKnownCreaturesAroundThePointExceptTheBlinkingOnes) {
	// FirstTargetProperty POINT: the effector becomes the first target and is not added (FirstTargetProperty.java:137-139); TargetRangeProperty
	// POINT: the effector's known creatures within target_distance + 1 of the point, alive and not BLINKING (TargetRangeProperty.java:85-94, :110)
	Ref<CastTestNpc> atPoint = spawnMonster(120.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> nearPoint = spawnMonster(125.5f, 100.0f, 50.0f); // 5.5 m: inside target_distance + 1
	Ref<CastTestNpc> farFromPoint = spawnMonster(127.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> blinking = spawnMonster(121.0f, 100.0f, 50.0f);
	for (const Ref<CastTestNpc>& npc : {atPoint, nearPoint, farFromPoint, blinking})
		ASSERT_TRUE(KnownListPairing::pair(*caster.player, *npc));
	blinking->setVisualState(gameserver::model::gameobjects::state::CreatureVisualState::BLINKING);

	Ref<model::Skill> cast = conditionSkill(POINT_PROPERTY);
	cast->setTargetType(1, 120.0f, 100.0f, 50.0f);
	ASSERT_TRUE(cast->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(cast->getFirstTarget(), Ptr<Creature>(caster.player));
	std::vector<Ptr<Creature>> effected = cast->getEffectedList().snapshot();
	ASSERT_EQ(effected.size(), 2u);
	EXPECT_NE(std::find(effected.begin(), effected.end(), Ptr<Creature>(atPoint)), effected.end());
	EXPECT_NE(std::find(effected.begin(), effected.end(), Ptr<Creature>(nearPoint)), effected.end());
}

TEST_F(SkillConditionsTest, AnAreaSkillKeepsTheCreaturesWithinTheEffectiveAltitude) {
	// TargetRangeProperty AREA: `Math.abs(firstTarget.getZ() - creature.getZ()) <= altitude`, effective_altitude="3" (TargetRangeProperty.java:34-48)
	Ref<CastTestNpc> first = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> threeAbove = spawnMonster(112.0f, 100.0f, 53.0f);
	Ref<CastTestNpc> fourAbove = spawnMonster(112.0f, 100.0f, 54.0f);
	ASSERT_TRUE(KnownListPairing::pair(*first, *threeAbove));
	ASSERT_TRUE(KnownListPairing::pair(*first, *fourAbove));
	Ref<model::Skill> cast = conditionSkill(ALTITUDE_PROPERTY, first);
	ASSERT_TRUE(cast->canUseSkill(Properties_CastState::CAST_START));
	std::vector<Ptr<Creature>> effected = cast->getEffectedList().snapshot();
	ASSERT_EQ(effected.size(), 2u);
	EXPECT_EQ(effected[0], Ptr<Creature>(first));
	EXPECT_EQ(effected[1], Ptr<Creature>(threeAbove));
}

TEST_F(SkillConditionsTest, AFriendSkillRefusesAnEnemyAndKeepsAFriend) {
	// FirstTargetProperty TARGET with target_relation FRIEND (FirstTargetProperty.java:87-92): no target or an enemy target is refused with
	// STR_SKILL_INVALID_TARGET_NOTENEMY_ONLY; TargetRelationProperty FRIEND keeps a friend and makes it the first target
	// (TargetRelationProperty.java:25-34)
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	EXPECT_FALSE(conditionSkill(FRIEND_PROPERTY, npc)->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_NOTENEMY_ONLY()));
	(*client)->clearSent();
	EXPECT_FALSE(conditionSkill(FRIEND_PROPERTY)->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_NOTENEMY_ONLY()));

	cp::PlayerFixture friendly = cp::makePlayer(410016, 9416, "Friend");
	place(*friendly.player, 105.0f, 100.0f, 50.0f);
	Ref<model::Skill> onFriend = conditionSkill(FRIEND_PROPERTY, friendly.player);
	EXPECT_TRUE(onFriend->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(onFriend->getFirstTarget(), Ptr<Creature>(friendly.player));
	std::vector<Ptr<Creature>> effected = onFriend->getEffectedList().snapshot();
	ASSERT_EQ(effected.size(), 1u);
	EXPECT_EQ(effected[0], Ptr<Creature>(friendly.player));
}

TEST_F(SkillConditionsTest, AFriendlyAreaSkillDropsTheEnemiesAroundTheCaster) {
	// TargetRelationProperty FRIEND (TargetRelationProperty.java:25-34): `removeIf(target -> effector.isEnemy(target) || !isBuffAllowed(...))`,
	// then the first remaining target is the first target - here the caster (first_target ME), then the friend; the enemy npc is dropped
	Ref<CastTestNpc> npc = spawnMonster(105.0f, 100.0f, 50.0f);
	cp::PlayerFixture friendly = cp::makePlayer(410017, 9417, "Friend");
	place(*friendly.player, 104.0f, 100.0f, 50.0f);
	ASSERT_TRUE(KnownListPairing::pair(*caster.player, *npc));
	ASSERT_TRUE(KnownListPairing::pair(*caster.player, *friendly.player));
	Ref<model::Skill> cast = conditionSkill(FRIEND_AREA_PROPERTY);
	ASSERT_TRUE(cast->canUseSkill(Properties_CastState::CAST_START));
	std::vector<Ptr<Creature>> effected = cast->getEffectedList().snapshot();
	ASSERT_EQ(effected.size(), 2u);
	EXPECT_EQ(effected[0], Ptr<Creature>(caster.player));
	EXPECT_EQ(effected[1], Ptr<Creature>(friendly.player));
	EXPECT_EQ(cast->getFirstTarget(), Ptr<Creature>(caster.player));
}

// ------------------------------------------------------------------------------------------------------------------------- Skill arms

TEST_F(SkillConditionsTest, ADeadOrDyingTargetIsNoValidTarget) {
	// Skill.isValidTarget (Skill.java:242-248): a target that is about to die, or dead without a resurrect effect, is dropped; the ONLYONE skill
	// is then refused with STR_SKILL_TARGET_IS_NOT_VALID
	Ref<CastTestNpc> dying = spawnMonster(110.0f, 100.0f, 50.0f);
	Ptr<Creature>(dying)->getLifeStats()->setKillingBlow(500);
	Ref<model::Skill> onDying = skill(ENEMY_SKILL, dying);
	EXPECT_FALSE(onDying->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(onDying->getEffectedList().isEmpty());

	Ref<CastTestNpc> dead = spawnCorpse(112.0f, 100.0f, 50.0f);
	ASSERT_TRUE(dead->isDead());
	(*client)->clearSent();
	Ref<model::Skill> onDead = skill(ENEMY_SKILL, dead);
	EXPECT_FALSE(onDead->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(onDead->getEffectedList().isEmpty());
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID()));

	Ref<CastTestNpc> alive = spawnMonster(114.0f, 100.0f, 50.0f);
	EXPECT_TRUE(skill(ENEMY_SKILL, alive)->canUseSkill(Properties_CastState::CAST_START));
}

TEST_F(SkillConditionsTest, ACounterSkillMustFollowItsTriggerWithinFiveSeconds) {
	// Skill.canUseSkill (Skill.java:161-167): `if ((player.getLastCounterSkill(status) + 5000) < now) return false`
	EXPECT_FALSE(conditionSkill(COUNTER_SKILL)->canUseSkill(Properties_CastState::CAST_START)) << "never dodged: the time is 0";
	caster.player->setLastCounterSkill(controllers::attack::AttackStatus::DODGE);
	EXPECT_TRUE(conditionSkill(COUNTER_SKILL)->canUseSkill(Properties_CastState::CAST_START)) << "a dodge just now";
	caster.player->setLastCounterSkill(controllers::attack::AttackStatus::PARRY);
	EXPECT_TRUE(conditionSkill(COUNTER_SKILL)->canUseSkill(Properties_CastState::CAST_START)) << "another status does not replace the dodge";
}

TEST_F(SkillConditionsTest, AnItemSkillWithACastTimeCannotBeUsedWhileMoving) {
	// Skill.canUseSkill (Skill.java:169-172): the ITEM method, a cast duration (the template's) and a moving player: STR_ITEM_CANCELED
	xml::LoadContext context;
	const gameserver::model::templates::item::ItemTemplate* scroll =
		xml::bindString<gameserver::model::templates::item::ItemTemplate>(context, R"(<item_template id="164000001" item_group="NONE"/>)").release();
	setMp(100);
	auto itemSkill = [&](int32_t skillId) { return model::Skill::create(skillTemplate(skillId), *caster.player, 1, nullptr, scroll); };
	ASSERT_EQ(itemSkill(TIMED_SKILL)->getSkillMethod(), model::Skill::SkillMethod::ITEM);
	EXPECT_TRUE(itemSkill(TIMED_SKILL)->canUseSkill(Properties_CastState::CAST_START));
	caster.player->getMoveController()->setInMove(true);
	(*client)->clearSent();
	EXPECT_FALSE(itemSkill(TIMED_SKILL)->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED()));
	EXPECT_TRUE(itemSkill(INSTANT_SKILL)->canUseSkill(Properties_CastState::CAST_START)) << "no cast duration: using it while moving is fine";
	caster.player->getMoveController()->setInMove(false);
}

TEST_F(SkillConditionsTest, APenaltySkillIsCastOrAppliedWhenTheCastEnds) {
	// Skill.startPenaltySkill (Skill.java:478-490), called from endCast unless every target resisted: with penalty_skill_send_msg the penalty
	// skill is cast (a PenaltySkill: its own SM_CASTSPELL_RESULT), else its effect is applied directly - here to an unknown id, which
	// SkillEngine.checkAndGetSkillTemplate reports (SkillEngine.java:181-188)
	setMp(100);
	ASSERT_TRUE(conditionSkill(PENALTY_CAST_SKILL, caster.player)->useSkill());
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 2u) << "the penalty skill's result, then the skill's";
	EXPECT_EQ(decodeCastSpellResult(results[0]).skillId, INSTANT_SKILL);
	EXPECT_EQ(decodeCastSpellResult(results[1]).skillId, PENALTY_CAST_SKILL);

	network::test::LogCapture engineLog({"com.aionemu.gameserver.skillengine.SkillEngine"});
	ASSERT_TRUE(conditionSkill(PENALTY_EFFECT_SKILL, caster.player)->useSkill());
	EXPECT_TRUE(engineLog.contains("Could not apply effect, invalid skill id " + std::to_string(UNKNOWN_PENALTY_SKILL))) << engineLog.dump();
}

// ------------------------------------------------------------------------------------------------------------------------- SkillEngine

TEST_F(SkillConditionsTest, SkillEngineAppliesEffectsByIdWithTheLevelDurationAndForceTypeAsked) {
	SkillEngine& engine = SkillEngine::getInstance();
	// applyEffect(int, effector, effected): not forced - forceType null (SkillEngine.java:169-172)
	Ref<model::Effect> unforced = engine.applyEffect(PASSIVE_SKILL, *caster.player, *caster.player);
	ASSERT_TRUE(unforced);
	EXPECT_EQ(unforced->getForceType(), nullptr);
	EXPECT_FALSE(engine.applyEffect(99999, *caster.player, *caster.player));

	// applyEffectDirectly(int, lvl, effector, effected, duration, forceType) (SkillEngine.java:140-143): the level and the forced duration asked
	Ref<model::Effect> leveled = engine.applyEffectDirectly(PASSIVE_SKILL, 7, *caster.player, *caster.player, 3000, model::Effect_ForceType::DEFAULT);
	ASSERT_TRUE(leveled);
	EXPECT_EQ(leveled->getSkillLevel(), 7);
	EXPECT_EQ(leveled->getDuration(), 3000);
	EXPECT_EQ(leveled->getForceType(), model::Effect_ForceType::DEFAULT);
	EXPECT_FALSE(engine.applyEffectDirectly(99999, 7, *caster.player, *caster.player, 3000, nullptr));

	// applyEffectDirectly(int, effector, effected, duration, forceType) (SkillEngine.java:135-138): the template's lvl
	Ref<model::Effect> templateLevel = engine.applyEffectDirectly(PASSIVE_SKILL, *caster.player, *caster.player, 0, nullptr);
	ASSERT_TRUE(templateLevel);
	EXPECT_EQ(templateLevel->getSkillLevel(), 1);
	EXPECT_EQ(templateLevel->getForceType(), nullptr);
	EXPECT_FALSE(engine.applyEffectDirectly(99999, *caster.player, *caster.player, 0, nullptr));

	// getSkillFor(player, template, target, level) (SkillEngine.java:70-76): no skill-list check, the level asked
	learn({INSTANT_SKILL});
	Ref<model::Skill> unlearned = engine.getSkillFor(*caster.player, skillTemplate(TIMED_SKILL), caster.player, 4);
	ASSERT_TRUE(unlearned);
	EXPECT_EQ(unlearned->getSkillLevel(), 4);
	EXPECT_EQ(unlearned->getFirstTarget(), Ptr<Creature>(caster.player));
}

TEST_F(SkillConditionsTest, ApplyEffectsDirectlyAppliesTheSkillToEveryValidTargetAroundTheFirst) {
	// SkillEngine.applyEffectsDirectly (SkillEngine.java:154-164): the first target, then Properties.validateEffectedList adds the targets in
	// range (AREA_ALL_SKILL: every enemy within 10 m of the first target); each gets the effect and the list is the answer
	Ref<CastTestNpc> first = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> near = spawnMonster(115.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> far = spawnMonster(125.0f, 100.0f, 50.0f);
	ASSERT_TRUE(KnownListPairing::pair(*first, *near));
	ASSERT_TRUE(KnownListPairing::pair(*first, *far));
	std::vector<Ptr<Creature>> targets =
		SkillEngine::getInstance().applyEffectsDirectly(AREA_ALL_SKILL, *caster.player, *first, 110.0f, 100.0f, 50.0f);
	ASSERT_EQ(targets.size(), 2u);
	EXPECT_EQ(targets[0], Ptr<Creature>(first));
	EXPECT_EQ(targets[1], Ptr<Creature>(near));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	EXPECT_THROW(SkillEngine::getInstance().applyEffectsDirectly(99999, *caster.player, *first, 0, 0, 0), runtime::NullPointerException)
		<< "Java dereferences the unknown template";
}

} // namespace
} // namespace aion::gameserver::skillengine::test
