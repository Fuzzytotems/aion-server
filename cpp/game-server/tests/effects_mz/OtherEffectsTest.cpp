// P5-04, M5b-2 stage 1 part 3 (m5b2-plan.md F-03, F-05, D13): ReturnEffect (243 Return, on every character's bar), TransformEffect through its
// concrete subclass PolymorphEffect (242 Drakan Transformation), and the two classes of 8751 "Light of Repose" that CuringZoneService casts
// once a second near a curing object (D13): MPHealEffect and ProcVPHealInstantEffect.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_EXP.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/HealEffectTemplate.h"
#include "aion/gameserver/skillengine/effect/MPHealEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::TribeClass;
using network::aion::serverpackets::SM_STATUPDATE_EXP;

class OtherEffectsTest : public EffectsMzTest {};

// ---- ReturnEffect (ReturnEffect.java:16-28) -------------------------------------------------------------------------------------------------

/**
 * 243 Return: calculate lands only on a spawned effected (`effect.getEffected().isSpawned()`, noresist and no resist roll otherwise), and
 * applyEffect teleports the effector - cast to Player - to its bind point (TeleportService.moveToBindLocation): here the same map and instance
 * of the World's test Poeta, so TeleportService takes the spawnOnSameMap arm and the character stands at the bind point again. The character
 * has no client connection for the teleport (as TeleportOnSameMapTest, PlayerReviveServiceTest.cpp: SM_PLAYER_INFO would read houses from a
 * database).
 */
TEST_F(OtherEffectsTest, ReturnTakesTheCasterToItsBindPoint) {
	EFFECT_TEST_SCOPE;
	Ref<Player> traveller = player(9101);
	traveller->setClientConnection(nullptr);
	traveller->setMotions(std::make_unique<gameserver::model::gameobjects::player::motion::MotionList>(*traveller));
	world::World& world = world::World::getInstance();
	world.storeObject(*traveller);
	ASSERT_TRUE(world.setPosition(*traveller, effecttest::POETA, 300.0f, 300.0f, 10.0f, int8_t{0}));
	world.spawn(Ptr<gameserver::model::gameobjects::VisibleObject>(*traveller));
	ASSERT_TRUE(traveller->isSpawned());
	traveller->setBindPoint(gameserver::model::gameobjects::player::BindPointPosition::create(effecttest::POETA, 500.0f, 400.0f, 12.5f, int8_t{7}));

	Ref<Effect> effect = calculated(243, *traveller, *traveller);
	ASSERT_TRUE(effect->isInSuccessEffects(1)) << "a spawned effected";
	effect->applyEffect();
	EXPECT_FLOAT_EQ(traveller->getX(), 500.0f);
	EXPECT_FLOAT_EQ(traveller->getY(), 400.0f);
	EXPECT_FLOAT_EQ(traveller->getZ(), 12.5f);
	EXPECT_EQ(traveller->getHeading(), 7);
	EXPECT_TRUE(traveller->isSpawned()) << "spawnOnSameMap";

	world::zone::ZoneUpdateService::getInstance().run();
	world.despawn(*traveller);
	world.removeObject(*traveller);
	traveller->setTarget(nullptr);
}

/** ReturnEffect.calculate: a despawned effected gets no success effect; applyEffect casts the effector to Player (ClassCastException for an npc) */
TEST_F(OtherEffectsTest, ReturnNeedsASpawnedEffectedAndAPlayerEffector) {
	EFFECT_TEST_SCOPE;
	Ref<Player> traveller = player(9111);
	traveller->getPosition()->setIsSpawned(false);
	EXPECT_FALSE(calculated(243, *traveller, *traveller)->isInSuccessEffects(1));
	traveller->getPosition()->setIsSpawned(true);

	Ref<Npc> npc = monster();
	Ref<Effect> byNpc = calculated(243, *npc, *npc);
	ASSERT_TRUE(byNpc->isInSuccessEffects(1));
	EXPECT_THROW(byNpc->getEffectTemplates()[0]->applyEffect(*byNpc), runtime::ClassCastException);
}

// ---- TransformEffect and PolymorphEffect (TransformEffect.java:17-124, PolymorphEffect.java:17-35) --------------------------------------

/**
 * 242 Drakan Transformation: TransformEffect.startEffect applies model 281812 with its restrictions (cantUseSkills), PolymorphEffect adds the
 * model's tribe from NPC_DATA (DRAKANPOLYMORPH); after duration2 60,000 ms endEffect finds no other transformation and ends it
 * (effected.endTransformation), and PolymorphEffect clears the tribe.
 */
TEST_F(OtherEffectsTest, DrakanTransformationChangesTheModelAndTheTribeForAMinute) {
	EFFECT_TEST_SCOPE;
	publishPolymorphNpcs();
	Ref<Player> p = player(9201);
	ASSERT_FALSE(p->getTransformModel().isActive());

	Ref<Effect> effect = applied(242, *p, *p);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(p->getTransformModel().getModelId(), DRAKAN_MODEL);
	EXPECT_TRUE(p->getTransformModel().cantUseSkills());
	EXPECT_FALSE(p->getTransformModel().cantMove());
	EXPECT_EQ(p->getTransformModel().getTribe(), std::optional<TribeClass>(TribeClass::DRAKANPOLYMORPH));
	EXPECT_EQ(effect->getDuration(), 60000);

	advance(60000);
	EXPECT_FALSE(p->getTransformModel().isActive()) << "endTransformation";
	EXPECT_FALSE(p->getTransformModel().cantUseSkills());
	EXPECT_EQ(p->getTransformModel().getTribe(), std::nullopt) << "setTribe(null)";
}

/**
 * TransformEffect.endEffect looks through the effected's abnormal effects for another transformation (another model): with 64019 (model
 * 281813, cantMove) still active, the end of 242 applies 281813 instead of ending the transformation; PolymorphEffect.endEffect still clears
 * the tribe. The end of 64019 then ends it.
 */
TEST_F(OtherEffectsTest, TheEndOfATransformationFallsBackToAnotherOne) {
	EFFECT_TEST_SCOPE;
	publishPolymorphNpcs();
	Ref<Player> p = player(9211);
	Ref<Effect> drakan = applied(242, *p, *p);
	Ref<Effect> second = applied(64019, *p, *p);
	ASSERT_TRUE(second->isInSuccessEffects(1));
	EXPECT_EQ(p->getTransformModel().getModelId(), SECOND_MODEL) << "the later transformation is on top";
	EXPECT_TRUE(p->getTransformModel().cantMove()) << "64019's cantMove";
	EXPECT_FALSE(p->getTransformModel().cantRecall());
	EXPECT_FALSE(p->getTransformModel().cantUseSkills());
	EXPECT_EQ(p->getTransformModel().getTribe(), std::optional<TribeClass>(TribeClass::MONSTER));

	second->endEffect();
	EXPECT_EQ(p->getTransformModel().getModelId(), DRAKAN_MODEL) << "242 is still active: its model is applied again";
	EXPECT_TRUE(p->getTransformModel().cantUseSkills());
	EXPECT_FALSE(p->getTransformModel().cantMove());
	EXPECT_EQ(p->getTransformModel().getTribe(), std::nullopt) << "PolymorphEffect.endEffect clears the tribe anyway";

	drakan->endEffect();
	EXPECT_FALSE(p->getTransformModel().isActive());
}

/**
 * TransformEffect.endEffect's `break` leaves only the inner loop over an effect's templates, so the outer loop goes on through the abnormal
 * effects (a LinkedHashMap: the order they were added in) and the last other transformation wins. With 242 (added first) and 64033 (model
 * 281814, cantMove) under 64019, the end of 64019 applies 64033's model and restrictions, not 242's.
 */
TEST_F(OtherEffectsTest, TheLastOtherTransformationIsTheFallback) {
	EFFECT_TEST_SCOPE;
	publishPolymorphNpcs();
	Ref<Player> p = player(9231);
	applied(242, *p, *p);
	applied(64033, *p, *p);
	Ref<Effect> top = applied(64019, *p, *p);
	ASSERT_EQ(p->getTransformModel().getModelId(), SECOND_MODEL);

	top->endEffect();
	EXPECT_EQ(p->getTransformModel().getModelId(), THIRD_MODEL) << "the last one found, not the first (242)";
	EXPECT_TRUE(p->getTransformModel().cantMove()) << "64033's cantMove";
	EXPECT_FALSE(p->getTransformModel().cantUseSkills()) << "242's cantUseSkills is not applied";
}

/**
 * TransformEffect.applyEffect: a FORM1 transformation with a panel (64028) first removes the transformations the effected has, when one is
 * active (EffectController.removeTransformEffects); a type NONE one (64019) does not.
 */
TEST_F(OtherEffectsTest, AFormTransformationWithAPanelReplacesTheActiveOnes) {
	EFFECT_TEST_SCOPE;
	publishPolymorphNpcs();
	Ref<Player> p = player(9221);
	applied(242, *p, *p);
	applied(64019, *p, *p);
	EXPECT_TRUE(p->getEffectController()->hasAbnormalEffect(242)) << "a NONE transformation removes nothing";

	Ref<Effect> form = applied(64028, *p, *p);
	ASSERT_TRUE(form->isInSuccessEffects(1));
	EXPECT_FALSE(p->getEffectController()->hasAbnormalEffect(242));
	EXPECT_FALSE(p->getEffectController()->hasAbnormalEffect(64019));
	EXPECT_EQ(p->getTransformModel().getModelId(), SECOND_MODEL);
	EXPECT_EQ(p->getTransformModel().getPanelId(), 1);
}

// ---- MPHealEffect (MPHealEffect.java:15-36) -------------------------------------------------------------------------------------------------

/**
 * 64020 (percent, value 10, checktime 2000, duration2 12,000): the snapshot is getMaxStatValue * 10 / 100 = 315 * 10 / 100 = 31 MP
 * (HealEffectTemplate.calculateSnapshotHealValue with MPHealEffect's MAXMP), and each tick heals min(max - current, 31)
 * (HealOverTimeEffect.onPeriodicAction with MPHealEffect's current MP): from 265, +31 at 2300 ms, then the 19 missing at 4300 ms.
 */
TEST_F(OtherEffectsTest, AnMpHealRestoresItsShareOfTheMaximumEachTick) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(9301);
	mage->getLifeStats()->setCurrentMp(265);
	ASSERT_EQ(mage->getLifeStats()->getCurrentMp(), 265);
	ASSERT_EQ(mage->getLifeStats()->getCurrentHp(), 158);

	Ref<Effect> effect = applied(64020, *mage, *mage);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	const auto* mpHeal = dynamic_cast<const MPHealEffect*>(effect->getEffectTemplates()[0]);
	ASSERT_NE(mpHeal, nullptr);
	EXPECT_EQ(mpHeal->getCurrentStatValue(*effect), 265) << "the current MP, not HP";
	EXPECT_EQ(mpHeal->getMaxStatValue(*effect), 315) << "the maximum MP (getMaxMp().getCurrent())";
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 31);
	EXPECT_EQ(effect->getReserveds(1)->getType(), model::EffectReserved::ResourceType::MP) << "startEffect(effect, HealType.MP)";
	EXPECT_EQ(effect->getDuration(), 13000) << "AbstractOverTimeEffect: duration2 + 1000";

	advance(2299);
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), 265);
	advance(1);
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), 296);
	EXPECT_EQ(mpHeal->getCurrentStatValue(*effect), 296);
	advance(2000);
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), 315);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), 158) << "HP is not healed";
}

// ---- ProcVPHealInstantEffect (ProcVPHealInstantEffect.java:14-46) ---------------------------------------------------------------------------

/**
 * 8751 Light of Repose on a level 10 Sorcerer (repose energy needs level 10, PlayerCommonData.isReadyForReposeEnergy): the maximum repose energy
 * is (long) (getExpNeed() * 0.25f) = (long) ((182252 - 126069) * 0.25f) = 14045, the cap value2 15 % of it: 14045 * 15 / 100 = 2106. Below the
 * cap the percent heal adds (int) (14045 * 1 * 0.001) = 14 and sends SM_STATUPDATE_EXP with the new repose values; its heal and MP heal
 * positions start their periodic tasks.
 */
TEST_F(OtherEffectsTest, LightOfReposeAddsItsPerMilleOfTheMaximumReposeBelowTheCap) {
	EFFECT_TEST_SCOPE;
	Ref<Player> sorcerer = player(9401, PlayerClass::SORCERER, 10);
	Ptr<gameserver::model::gameobjects::player::PlayerCommonData> pcd = sorcerer->getCommonData();
	ASSERT_EQ(pcd->getLevel(), 10);
	pcd->updateMaxRepose();
	ASSERT_EQ(pcd->getMaxReposeEnergy(), 14045);
	ASSERT_EQ(pcd->getCurrentReposeEnergy(), 0);
	clearSent(*sorcerer);

	Ref<Effect> effect = applied(8751, *sorcerer, *sorcerer);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(pcd->getCurrentReposeEnergy(), 14);
	std::vector<std::vector<uint8_t>> exp = sentTo<SM_STATUPDATE_EXP>(*sorcerer);
	ASSERT_EQ(exp.size(), 1u);
	EXPECT_EQ(exp[0], cp::serialized(SM_STATUPDATE_EXP(pcd->getExpShown(), pcd->getExpRecoverable(), pcd->getExpNeed(), 14, 14045),
					   &connection(*sorcerer)));
	EXPECT_TRUE(effect->isInSuccessEffects(3)) << "the MP heal position (MPHealEffect) landed too";
}

/**
 * 64021 (flat value 50 + 10 per level) at skill level 2: 70 while the current repose is below the cap of 2106 - from 2100 to 2170, and from
 * 2105 to 2175 - and nothing (no packet) once it is not, from 2106 on; a level 9 character and an npc get nothing.
 */
TEST_F(OtherEffectsTest, AFlatReposeHealStopsAtTheCap) {
	EFFECT_TEST_SCOPE;
	Ref<Player> sorcerer = player(9411, PlayerClass::SORCERER, 10);
	Ptr<gameserver::model::gameobjects::player::PlayerCommonData> pcd = sorcerer->getCommonData();
	pcd->updateMaxRepose();
	pcd->setCurrentReposeEnergy(2100);
	clearSent(*sorcerer);

	applied(64021, *sorcerer, *sorcerer, 2);
	EXPECT_EQ(pcd->getCurrentReposeEnergy(), 2170);
	EXPECT_EQ(sentTo<SM_STATUPDATE_EXP>(*sorcerer).size(), 1u);
	clearSent(*sorcerer);
	applied(64021, *sorcerer, *sorcerer, 2);
	EXPECT_EQ(pcd->getCurrentReposeEnergy(), 2170) << "2170 is not below the cap";
	EXPECT_TRUE(sentTo<SM_STATUPDATE_EXP>(*sorcerer).empty());

	// the boundary of `getCurrentReposeEnergy() < cap`: exactly the cap gets nothing, one below it the whole heal
	pcd->setCurrentReposeEnergy(2106);
	applied(64021, *sorcerer, *sorcerer, 2);
	EXPECT_EQ(pcd->getCurrentReposeEnergy(), 2106) << "2106 is not below the cap of 2106";
	EXPECT_TRUE(sentTo<SM_STATUPDATE_EXP>(*sorcerer).empty());
	pcd->setCurrentReposeEnergy(2105);
	applied(64021, *sorcerer, *sorcerer, 2);
	EXPECT_EQ(pcd->getCurrentReposeEnergy(), 2175) << "2105 is below it";
	clearSent(*sorcerer);

	// a character below level 10 is not ready for repose energy (isReadyForReposeEnergy); this one kept the maximum of its level 10
	// (setExp recomputes the level but, offline, not the maximum), so the cap alone would let the heal through
	Ref<Player> young = player(9412, PlayerClass::SORCERER, 10);
	young->getCommonData()->updateMaxRepose();
	young->getCommonData()->setLevel(9);
	ASSERT_EQ(young->getCommonData()->getLevel(), 9);
	ASSERT_EQ(young->getCommonData()->getMaxReposeEnergy(), 14045);
	young->getCommonData()->setCurrentReposeEnergy(0);
	clearSent(*young);
	applied(64021, *young, *young, 2);
	EXPECT_EQ(young->getCommonData()->getCurrentReposeEnergy(), 0) << "level 9: not ready for repose energy";
	EXPECT_TRUE(sentTo<SM_STATUPDATE_EXP>(*young).empty());

	Ref<Npc> npc = monster();
	EXPECT_NO_THROW(applied(64021, *npc, *npc)) << "only a player effected";
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
