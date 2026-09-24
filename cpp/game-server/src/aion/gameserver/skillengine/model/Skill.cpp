#include "aion/gameserver/skillengine/model/Skill.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/ai/manager/SkillAttackManager.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/SkillChargeData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/abyss/AbyssService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/action/Action.h"
#include "aion/gameserver/skillengine/action/Actions.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/ActivationAttribute.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/model/ChargeSkillEntry.h"
#include "aion/gameserver/skillengine/model/ChargedSkill.h"
#include "aion/gameserver/skillengine/model/DashStatusInfo.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/Motion.h"
#include "aion/gameserver/skillengine/model/PenaltySkill.h"
#include "aion/gameserver/skillengine/model/SkillSubType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/skillengine/condition/SkillChargeCondition.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/TargetRangeAttribute.h"
#include "aion/gameserver/skillengine/properties/TargetRelationAttribute.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::skillengine::model {

using controllers::attack::AttackStatus;
using controllers::observer::DeathObserver;
using dataholders::DataManager;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;
using network::aion::serverpackets::SM_CASTSPELL;
using network::aion::serverpackets::SM_CASTSPELL_RESULT;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using properties::FirstTargetAttribute;
using properties::Properties;
using properties::Properties_CastState;
using properties::TargetRangeAttribute;
using properties::TargetRelationAttribute;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;
using utils::PositionUtil;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.model.Skill");

namespace {

/** Java dereferences skillTemplate.getProperties() unchecked in several places (61 templates of skill_templates.xml have none) */
const Properties& requireProperties(const SkillTemplate* skillTemplate) {
	const Properties* properties = skillTemplate->getProperties();
	if (properties == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"Properties.getFirstTarget()\" because the return value of \"SkillTemplate.getProperties()\" is null");
	return *properties;
}

/** Java `player.getLastSkill()` dereferenced unchecked (Skill.java:462, :474) */
const SkillTemplate& requireLastSkill(Player& player) {
	const SkillTemplate* lastSkill = player.getLastSkill();
	if (lastSkill == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"SkillTemplate.isMultiCast()\" because the return value of \"Player.getLastSkill()\" is null");
	return *lastSkill;
}

/** Java `(int) f` for a float: NaN 0, saturating */
int32_t javaFloatToInt(float value) {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java `(int) d` for a double: NaN 0, saturating */
int32_t javaDoubleToInt(double value) {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483647.0)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java `a.equals(b)` of AionObject with a nullable argument: false for null */
bool equalsNullable(Creature& a, Ptr<Creature> b) {
	return b && a.equals(*b);
}

/** the borrowed view SM_CASTSPELL_RESULT takes of the effect list (Java passes the list itself) */
std::vector<Ptr<Effect>> borrow(const std::vector<Ref<Effect>>& effects) {
	return std::vector<Ptr<Effect>>(effects.begin(), effects.end());
}

} // namespace

Skill::Skill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::player::Player& effectorValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue)
	// Java: this(skillTemplate, effector, effector.getSkillList().getSkillLevel(skillTemplate.getSkillId()), firstTarget, null)
	: Skill(skillTemplateValue, effectorValue, effectorValue.getSkillList()->getSkillLevel(skillTemplateValue->getSkillId()), firstTargetValue,
		  nullptr) {
}

Skill::Skill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::player::Player& effectorValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue, int32_t skillLevelValue)
	: Skill(skillTemplateValue, effectorValue, skillLevelValue, firstTargetValue, nullptr) {
}

Skill::Skill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLvl,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue, const gameserver::model::templates::item::ItemTemplate* itemTemplateValue)
	// Java: this.effectedList = new ArrayList<>() (the member's default state)
	: firstTarget(firstTargetValue), effector(effectorValue), skillLevel(skillLvl),
	  moveListener(controllers::observer::StartMovingListener::create()), skillTemplate(skillTemplateValue), itemTemplate(itemTemplateValue),
	  baseCastDuration(skillTemplateValue->getDuration()), castDuration(skillTemplateValue->getDuration()) {
	// Java calls the overridable initializeSkillMethod() here. A C++ constructor cannot dispatch to the subclass override, so this runs
	// Skill::initializeSkillMethod and PenaltySkill's constructor calls its own again after this one (Skill.h, docs/deviations/P5-02.md).
	Skill::initializeSkillMethod();
}

Skill::~Skill() = default;

void Skill::setFirstTarget(runtime::Ptr<gameserver::model::gameobjects::Creature> value) {
	firstTarget.set(value);
}

runtime::Ref<Skill> Skill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::player::Player& effectorValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue) {
	return runtime::makeRef<Skill>(skillTemplateValue, effectorValue, firstTargetValue);
}

runtime::Ref<Skill> Skill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::player::Player& effectorValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue, int32_t skillLevelValue) {
	return runtime::makeRef<Skill>(skillTemplateValue, effectorValue, firstTargetValue, skillLevelValue);
}

runtime::Ref<Skill> Skill::create(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLvl,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue, const gameserver::model::templates::item::ItemTemplate* itemTemplateValue) {
	return runtime::makeRef<Skill>(skillTemplateValue, effectorValue, skillLvl, firstTargetValue, itemTemplateValue);
}

void Skill::initializeSkillMethod() {
	if (itemTemplate != nullptr)
		skillMethod.set(SkillMethod::ITEM);
	else if (skillTemplate->isPassive())
		skillMethod.set(SkillMethod::PASSIVE);
	else if (skillTemplate->isProvoked())
		skillMethod.set(SkillMethod::PROVOKED);
	else
		skillMethod.set(SkillMethod::CAST);
}

bool Skill::canUseSkill(properties::Properties_CastState castState) {
	const Properties* properties = skillTemplate->getProperties();
	if (properties != nullptr && !properties->validate(*this, castState)) {
		log.debug("properties failed");
		return false;
	}

	if (!preCastCheck())
		return false;

	if (castState == Properties_CastState::CAST_START && isInvalidRecall())
		return false;

	// check for counter skill
	if (Ptr<Player> player = runtime::as<Player>(effector)) {
		if (skillMethod.get() == SkillMethod::CAST && chainCategory.get().empty()) // category gets set in preCastCheck()
			player->getChainSkills()->resetChain();

		if (skillTemplate->getCounterSkill().has_value()) {
			int64_t time = player->getLastCounterSkill(*skillTemplate->getCounterSkill());
			if ((time + 5000) < commons::utils::currentTimeMillis()) {
				log.debug("chain skill failed, too late");
				return false;
			}
		}

		if (skillMethod.get() == SkillMethod::ITEM && baseCastDuration.get() > 0 && player->getMoveController()->isInMove()) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
			return false;
		}
	}

	if (castState == Properties_CastState::CAST_START && !canPayCastCosts())
		return false;

	return validateEffectedList();
}

bool Skill::canPayCastCosts() {
	const condition::Conditions* endConditions = skillTemplate->getEndConditions();
	if (endConditions != nullptr && !endConditions->canValidate(*this))
		return false;
	const action::Actions* skillActions = skillTemplate->getActions();
	if (skillActions == nullptr)
		return true;
	for (const std::unique_ptr<action::Action>& action : skillActions->getActions())
		if (!action->canAct(*this))
			return false;
	return true;
}

bool Skill::validateEffectedList() {
	if (Ptr<Player> player = runtime::as<Player>(effector)) {
		if (canUseSkill(*player))
			effectedList.removeIf([this, &player](const Ptr<Creature>& effected) { return !isValidTarget(*player, *effected); });
		else
			effectedList.clear();
	}

	if (targetType.get() == 0 && effectedList.isEmpty()) { // target selected but no target will be hit
		if (getTargetRangeAttribute() != TargetRangeAttribute::AREA) { // don't restrict AoE activation
			if (Ptr<Player> player = runtime::as<Player>(effector))
				PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
			return false;
		}
	}

	return true;
}

bool Skill::canUseSkill(gameserver::model::gameobjects::player::Player& player) {
	if (player.isUsingFlightTransporterOrWindstream())
		return false;
	if (!getSkillTemplate()->hasEvadeEffect() && player.getEffectController()->isInAnyAbnormalState(effect::AbnormalState::CANT_ATTACK_STATE))
		return false;
	if (player.getStore())
		return false;
	return true;
}

bool Skill::isValidTarget(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& target) {
	if (Ptr<Player> targetPlayer = runtime::as<Player>(target)) {
		if (targetPlayer->isUsingFlightTransporterOrWindstream())
			return false;
		if (target.getRace() != player.getRace()) {
			if (!target.isEnemyFrom(player))
				return false;
		} else if (targetPlayer->isDueling(player) && requireProperties(getSkillTemplate()).getTargetRelation() != TargetRelationAttribute::ENEMY) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
			return false;
		}
	}

	if (target.getLifeStats()->isAboutToDie() && !isNonTargetAOE())
		return false;

	if (target.isDead() && !getSkillTemplate()->hasResurrectEffect() && !isNonTargetAOE()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
		return false;
	}

	// cant resurrect non players and non dead
	if (getSkillTemplate()->hasResurrectEffect() && (!runtime::as<Player>(target) || !target.isDead()))
		return false;

	return true;
}

bool Skill::useSkill() {
	return useSkill(configs::main::SecurityConfig::CHECK_ANIMATIONS, true);
}

bool Skill::useNoAnimationSkill() {
	return useSkill(false, true);
}

bool Skill::useWithoutPropSkill() {
	return useSkill(false, false);
}

bool Skill::useSkill(bool checkAnimation, bool checkproperties) {
	boostSkillCost.set(0);
	effector->getObserveController()->notifyBoostSkillCostObservers(*this);

	if (checkproperties && !canUseSkill(Properties_CastState::CAST_START))
		return false;

	updateCastDurationAndSpeed();
	updateHitTime(checkAnimation);

	// notify skill use observers
	if (skillMethod.get() == SkillMethod::CAST || skillMethod.get() == SkillMethod::ITEM)
		effector->getObserveController()->notifyStartSkillCastObservers(*this);

	// start casting
	effector->setCasting(Ptr<Skill>(this));

	// send packets to start casting
	if (skillMethod.get() == SkillMethod::CAST || skillMethod.get() == SkillMethod::ITEM) {
		castStartTime.set(commons::utils::currentTimeMillis());
		startCast();
		if (runtime::as<Npc>(effector))
			effector->getAi().setSubStateIfNot(ai::AISubState::CAST);
	}

	effector->getObserveController()->attach(*moveListener);

	if (Ptr<Npc> npc = runtime::as<Npc>(effector)) {
		Ptr<gameserver::model::skill::NpcSkillEntry> currentNpcSkillEntry = npc->getGameStats()->getLastSkill();
		if (currentNpcSkillEntry) {
			currentNpcSkillEntry->setLastTimeUsed();
			npc->getGameStats()->setNextSkillDelay(currentNpcSkillEntry->getNextSkillTime());
		} else {
			npc->getGameStats()->setNextSkillDelay(-1);
		}
	}
	effector->getAi().onStartUseSkill(skillTemplate, skillLevel);
	// Java: schedule(this::cancelCurrentSkillCast / this::endCast, castDuration) - the member form pins this Skill (fieldmap Skill@L312/@L314)
	if (skillTemplate->isCharge()) {
		utils::ThreadPoolManager::getInstance().schedule(this, &Skill::cancelCurrentSkillCast, castDuration.get());
	} else if (castDuration.get() > 0) {
		utils::ThreadPoolManager::getInstance().schedule(this, &Skill::endCast, castDuration.get());
	} else {
		endCast();
	}
	return true;
}

void Skill::setCooldowns() {
	int32_t cooldown = effector->getSkillCooldown(skillTemplate);
	if (cooldown != 0) {
		if (skillTemplate->getCooldownDeltaLv() != 0)
			cooldown = cooldown + skillTemplate->getCooldownDeltaLv() * skillLevel;
		effector->setSkillCoolDown(skillTemplate->getCooldownId(), cooldown * 100 + commons::utils::currentTimeMillis());
	}
}

int32_t Skill::getCooldown() {
	int32_t cooldown = effector->getSkillCooldown(skillTemplate);
	if (cooldown != 0)
		if (skillTemplate->getCooldownDeltaLv() != 0)
			cooldown = cooldown + skillTemplate->getCooldownDeltaLv() * skillLevel;
	return cooldown;
}

void Skill::updateCastDurationAndSpeed() {
	if (Ptr<Npc> npc = runtime::as<Npc>(effector)) { // TODO: check if all skills should be effected
		castDuration.set(utils::JavaMath::round(static_cast<float>(baseCastDuration.get()) * (static_cast<float>(npc->getGameStats()->getCastSpeed()) / 1000.0f)));
		castSpeedForAnimationBoostAndChargeSkills.set(1.0f);
	} else if (skillTemplate->isCharge()) {
		castDuration.set(calculateChargeCastDuration());
		castSpeedForAnimationBoostAndChargeSkills.set(static_cast<float>(castDuration.get()) / static_cast<float>(baseCastDuration.get()));
	} else {
		castDuration.set(calculateCastDuration());
		castSpeedForAnimationBoostAndChargeSkills.set(
			1 - static_cast<float>(effector->getGameStats()->getStat(StatEnum::BOOST_CASTING_TIME, 1000)->getBonus()) / 1000.0f);
	}
}

int32_t Skill::calculateChargeCastDuration() {
	SkillType chargeTimeBonusType = SkillType::NONE;
	// cast/attack speed can affect charge time since 4.8 (https://aionpowerbook.com/powerbook/New_World_Update_-_Skill_Changes#Other_Changes)
	const condition::SkillChargeCondition* chargeCondition = skillTemplate->getSkillChargeCondition();
	if (chargeCondition != nullptr) {
		int32_t maxCastDuration = 0;
		const ChargeSkillEntry* skillCharge = DataManager::SKILL_CHARGE_DATA->getChargedSkillEntry(chargeCondition->getValue());
		if (skillCharge == nullptr) // Java dereferences the entry unchecked
			throw runtime::NullPointerException("Cannot invoke \"ChargeSkillEntry.getChargeTimeBonusType()\" because \"skillCharge\" is null");
		chargeTimeBonusType = skillCharge->getChargeTimeBonusType();
		for (const ChargedSkill& chargedSkill : skillCharge->getSkills()) {
			maxCastDuration += chargedSkill.getTime();
		}
		baseCastDuration.set(maxCastDuration);
	}
	float speedRatio;
	switch (chargeTimeBonusType) {
		case SkillType::PHYSICAL:
			speedRatio = effector->getGameStats()->getAttackSpeedRate();
			break;
		case SkillType::MAGICAL:
			speedRatio = isCastDurationAffectedByCastSpeed()
				? static_cast<float>(calculateMagicalCastDuration()) / static_cast<float>(baseCastDuration.get())
				: 1.0f;
			break;
		default:
			speedRatio = 1.0f;
			break;
	}
	// charge skills are only affected by half of the speed bonus
	return javaFloatToInt(static_cast<float>(baseCastDuration.get()) * (1 - (1 - speedRatio) / 2));
}

int32_t Skill::calculateCastDuration() {
	if (getItemTemplate() != nullptr)
		return getItemTemplate()->getCastingDelay();
	// 2nd+ time of multicast-skill activation
	if (getMultiCastCount() > 0)
		return 0;
	if (!isCastDurationAffectedByCastSpeed())
		return baseCastDuration.get();
	return calculateMagicalCastDuration();
}

int32_t Skill::calculateMagicalCastDuration() {
	int32_t baseDurationCap = utils::JavaMath::round(static_cast<float>(baseCastDuration.get()) * 0.25f);
	// casting time stats cap 75%
	int32_t castDurationValue =
		std::max(effector->getGameStats()->getPositiveReverseStat(StatEnum::BOOST_CASTING_TIME, baseCastDuration.get()), baseDurationCap);
	int32_t boostValue = effector->getGameStats()->getPositiveReverseStat(StatEnum::BOOST_CASTING_TIME_SKILL, baseCastDuration.get());
	std::optional<StatEnum> skillCastBoostStat = getSkillCastBoostStat();
	if (skillCastBoostStat.has_value())
		boostValue = effector->getGameStats()->getPositiveReverseStat(*skillCastBoostStat, boostValue);

	int32_t buffDelta = baseCastDuration.get() - boostValue;
	castDurationValue -= buffDelta;

	if (!isSummonType(skillTemplate->getSubType())) {
		castDurationValue = std::max(castDurationValue, baseDurationCap);
	}
	return std::max(castDurationValue, 0);
}

std::optional<gameserver::model::stats::container::StatEnum> Skill::getSkillCastBoostStat() {
	switch (skillTemplate->getSubType()) {
		case SkillSubType::SUMMON:
			return StatEnum::BOOST_CASTING_TIME_SUMMON;
		case SkillSubType::SUMMONHOMING:
			return StatEnum::BOOST_CASTING_TIME_SUMMONHOMING;
		case SkillSubType::SUMMONTRAP:
			return StatEnum::BOOST_CASTING_TIME_TRAP;
		case SkillSubType::HEAL:
			return StatEnum::BOOST_CASTING_TIME_HEAL;
		case SkillSubType::ATTACK:
			return StatEnum::BOOST_CASTING_TIME_ATTACK;
		default:
			return std::nullopt;
	}
}

bool Skill::isSummonType(SkillSubType type) {
	return type == SkillSubType::SUMMON || type == SkillSubType::SUMMONHOMING || type == SkillSubType::SUMMONTRAP;
}

void Skill::updateHitTime(bool checkAnimation) {
	hitTime.set(clientHitTime.get());
	Ptr<Player> player = runtime::as<Player>(effector);
	if (!checkAnimation || !player || (skillMethod.get() != SkillMethod::CAST && skillMethod.get() != SkillMethod::ITEM))
		return;

	float animationTimeUntilFirstHit = DataManager::MOTION_DATA->calculateAnimationTimeUntilFirstHit(*player, *this);
	int32_t toleranceMillis = 1;
	if (skillTemplate->getAmmoSpeed() != 0) {
		Ptr<Creature> target = firstTarget.get();
		float distance = static_cast<float>(PositionUtil::getDistance(*player, *target));
		if (player->getMoveController()->isInMove() || target->getMoveController()->isInMove()) // subtract the run distance until ammo is actually fired
			distance -= PositionUtil::calculateMaxCoveredDistance(*player, utils::JavaMath::round(animationTimeUntilFirstHit));
		float distanceTolerance = getDistanceTolerance(*player, *target);
		float ammoTime = std::max(0.0f, distance / static_cast<float>(skillTemplate->getAmmoSpeed()) * 1000);
		toleranceMillis +=
			std::max(0, javaDoubleToInt(std::ceil(static_cast<double>(distanceTolerance / static_cast<float>(skillTemplate->getAmmoSpeed()) * 1000))));
		animationTimeUntilFirstHit += ammoTime;
	}

	int32_t motionDelay = skillTemplate->getMotion() == nullptr ? 0 : skillTemplate->getMotion()->getDelay();
	int32_t serverHitTime = motionDelay + utils::JavaMath::round(animationTimeUntilFirstHit);
	if (serverHitTime > clientHitTime.get()) {
		hitTime.set(serverHitTime);
		if (isSuspiciousClientHitTime(clientHitTime.get(), serverHitTime, toleranceMillis, *player)) {
			std::vector<std::string> uncertainties = collectUncertaintyFactorsForHitTime(*player, toleranceMillis);
			std::string uncertaintyFactors;
			if (!uncertainties.empty()) {
				uncertaintyFactors = " Uncertainty factors: ";
				for (size_t i = 0; i < uncertainties.size(); ++i) {
					if (i > 0)
						uncertaintyFactors += ", ";
					uncertaintyFactors += uncertainties[i];
				}
			}
			utils::audit::AuditLogger::log(*player, "modified hit time for skill " + std::to_string(getSkillId()) + " (client < server: "
				+ std::to_string(clientHitTime.get()) + "/" + std::to_string(serverHitTime) + ")." + uncertaintyFactors);
		}
	}
}

float Skill::getDistanceTolerance(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& target) {
	int64_t nowMillis = commons::utils::currentTimeMillis();
	// even when not yet moving on server side, the player can just have started to move before casting (CM_MOVE is sent after CM_CASTSPELL)
	int64_t maxMovementMillis = player.getMoveController()->isInMove() ? 1000 : 200;
	int64_t movementMillis = std::min(maxMovementMillis, nowMillis - player.getMoveController()->getLastMoveUpdate());
	float distanceTolerance = PositionUtil::calculateMaxCoveredDistance(player, movementMillis);
	if (target.getMoveController()->isInMove())
		distanceTolerance += PositionUtil::calculateMaxCoveredDistance(target, nowMillis - target.getMoveController()->getLastMoveUpdate());
	return distanceTolerance;
}

bool Skill::isSuspiciousClientHitTime(int32_t clientHitTimeValue, int32_t serverHitTime, int32_t tolerance,
	gameserver::model::gameobjects::player::Player& player) {
	if (clientHitTimeValue >= serverHitTime - tolerance)
		return false;
	if (clientHitTimeValue == 0 && (itemTemplate != nullptr || (skillTemplate->getMotion() != nullptr && skillTemplate->getMotion()->isInstantSkill())))
		return false; // effects apply immediately (damage too, though visually delayed)
	if (clientHitTimeValue == 0 && player.isInRobotMode()
		&& (requireLastSkill(player).isMultiCast() || DataManager::SKILL_CHARGE_DATA->isChargeSkill(requireLastSkill(player))))
		return false; // AT sends no hitTime when casting a non-instant skill within the animation time of a previous multiCast or charge skill, like 2640
	return true;
}

std::vector<std::string> Skill::collectUncertaintyFactorsForHitTime(gameserver::model::gameobjects::player::Player& player, int32_t toleranceMillis) {
	std::vector<std::string> uncertainties;
	if (allowAnimationBoostByCastSpeed() && !player.isHitTimeBoosted())
		uncertainties.push_back("cast speed");
	if (skillTemplate->getAmmoSpeed() != 0)
		uncertainties.push_back("movement (calculated tolerance: " + std::to_string(toleranceMillis) + " ms)");
	if (clientHitTime.get() == 0 && player.isInRobotMode()) // TODO remove once isSuspiciousClientHitTime() identifies all false positives
		// Java's literal ends in U+1F937 U+200D U+2642 U+FE0F (a shrugging man), written here as its UTF-8 bytes
		uncertainties.push_back("Aethertech being weird \xF0\x9F\xA4\xB7\xE2\x80\x8D\xE2\x99\x82\xEF\xB8\x8F (previous skill: "
			+ std::to_string(requireLastSkill(player).getSkillId()) + ")");
	return uncertainties;
}

void Skill::startPenaltySkill() {
	int32_t penaltySkill = skillTemplate->getPenaltySkillId();
	if (penaltySkill == 0)
		return;
	if (getSkillTemplate()->shouldPenaltySkillSendMsg()) {
		Ref<PenaltySkill> penaltySkill1 = SkillEngine::getInstance().getPenaltySkill(*effector, penaltySkill, 1);
		if (penaltySkill1) {
			penaltySkill1->useSkill();
		}
	} else {
		// Java: applyEffectDirectly(penaltySkill, firstTarget, effector) - the first target is the effector of the penalty effect; a null first
		// target reaches `new Effect(null, ...)` in Java and dereferences here
		SkillEngine::getInstance().applyEffectDirectly(penaltySkill, *firstTarget.get(), *effector);
	}
}

void Skill::startCast() {
	Ptr<Creature> target = firstTarget.get();
	int32_t targetObjId = target ? target->getObjectId() : 0;
	bool needsCast = itemTemplate != nullptr && itemTemplate->isCombatActivated();
	if (skillMethod.get() == SkillMethod::CAST || needsCast) {
		switch (targetType.get()) {
			case 0: // PlayerObjectId as Target
				PacketSendUtility::broadcastPacketAndReceive(*effector,
					SM_CASTSPELL(*effector, skillTemplate->getSkillId(), skillLevel, targetType.get(), targetObjId, castDuration.get(),
						castSpeedForAnimationBoostAndChargeSkills.get(), allowAnimationBoostByCastSpeed()));
				if (runtime::as<Npc>(effector)) {
					// Java: ShoutEventHandler.onCast((NpcAI) effector.getAi(), firstTarget), whose body is `if (firstTarget instanceof Player && ...)`:
					// a null first target does nothing there, and the C++ signature takes a reference, so the call is skipped for null
					if (target)
						ai::handler::ShoutEventHandler::onCast(*runtime::cast<ai::NpcAI>(effector->getAi()), *target);
				}
				break;

			case 3: // Target not in sight?
				PacketSendUtility::broadcastPacketAndReceive(*effector,
					SM_CASTSPELL(*effector, skillTemplate->getSkillId(), skillLevel, targetType.get(), targetObjId, castDuration.get(),
						castSpeedForAnimationBoostAndChargeSkills.get(), allowAnimationBoostByCastSpeed()));
				break;

			case 1: // XYZ as Target
				PacketSendUtility::broadcastPacketAndReceive(*effector,
					SM_CASTSPELL(*effector, skillTemplate->getSkillId(), skillLevel, targetType.get(), x.get(), y.get(), z.get(), castDuration.get(),
						castSpeedForAnimationBoostAndChargeSkills.get(), allowAnimationBoostByCastSpeed()));
				break;
		}
		if (Ptr<Player> player = runtime::as<Player>(effector))
			player->setNextSkillUse(commons::utils::currentTimeMillis() + configs::main::GSConfig::MIN_SKILL_CAST_INTERVAL_MILLIS);
	} else if (skillMethod.get() == SkillMethod::ITEM && castDuration.get() > 0) {
		PacketSendUtility::broadcastPacketAndReceive(*effector, SM_ITEM_USAGE_ANIMATION(effector->getObjectId(), target->getObjectId(), itemObjectId.get(),
			itemTemplate->getTemplateId(), castDuration.get(), 0, 0));
	}

	if (target && !target->equals(*effector) && !skillTemplate->hasResurrectEffect() && (castDuration.get() > 0)
		&& requireProperties(skillTemplate).getFirstTarget() != FirstTargetAttribute::POINT
		&& requireProperties(skillTemplate).getFirstTarget() != FirstTargetAttribute::ME) {
		Ptr<Npc> npc = runtime::as<Npc>(effector);
		if ((npc && npc->isBoss())
			|| (requireProperties(skillTemplate).getFirstTarget() == FirstTargetAttribute::TARGET && requireProperties(skillTemplate).getEffectiveDist() > 0)) {
			return;
		}
		// Java: new DeathObserver(_ -> getEffector().getController().cancelCurrentSkill(null, SM_SYSTEM_MESSAGE.STR_SKILL_TARGET_LOST())), the lambda
		// of fieldmap Skill@L535 pinning this Skill. The Skill -> observer -> Skill cycle is cut by removeObservers (cycles.toml
		// Skill.firstTargetDieObserver, cpp-breaker), which endCast and cancelCast always run.
		firstTargetDieObserver.set(DeathObserver::create(runtime::PinnedCallback<void(Creature&)>(runtime::Pin{this}, [this](Creature&) {
			SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_LOST();
			getEffector()->getController().cancelCurrentSkill(nullptr, &message);
		})));
		target->getObserveController()->attach(*firstTargetDieObserver.get());
	}
}

void Skill::cancelCast() {
	// java-race: Java's check-then-set on the non-volatile isCancelled is not atomic either; cancellation arrives from the owner's packet or task
	if (isCancelled.get())
		return;
	isCancelled.set(true);
	removeObservers();
}

void Skill::cancelCurrentSkillCast() {
	if (!isCancelled.get() && effector->getCastingSkill() == this)
		effector->getController().cancelCurrentSkill(nullptr, nullptr);
}

void Skill::endCast() {
	removeObservers();
	if (!effector->isCasting() || isCancelled.get())
		return;
	// check if target is out of skill range or other requirements are not met (anymore)
	const Properties* properties = skillTemplate->getProperties();
	if ((properties != nullptr && !properties->endCastValidate(*this)) || !validateEffectedList() || !preUsageCheck()) {
		effector->getController().cancelCurrentSkill(nullptr); // calls effector.setCasting(null) and sends skill cancel packet
		return;
	}
	if (isInvalidRecall()) {
		effector->getController().cancelCurrentSkill(nullptr, nullptr); // the validation already told the caster why the recall failed
		return;
	}
	if (!payCastCosts()) {
		effector->getController().cancelCurrentSkill(nullptr, nullptr); // the unpaid cost already told the player what is missing
		return;
	}
	effector->setCasting(nullptr);

	// Create effects and precalculate result
	int32_t dashStatus = 0;
	int32_t resistCount = 0;
	bool blockedChain = false;
	bool blockedStance = false;
	std::vector<Ref<Effect>> effects;
	if (skillTemplate->getEffects() != nullptr) {
		for (Ptr<Creature> effected : effectedList) {
			// TODO: RI_CHARGEATTACK fix: effect is not applied twice, but its Dash effect inflicts (weapon) damage.
			// Seems like Offi is creating a new "Effect" for each DamageEffect. Last one contains relevant info about spell status etc.
			// Client displays the wrong chat output in these cases. (e.g. RI_CHARGEATTACK displays 2x the spellatkinstant damage)
			Ref<Effect> effect = Effect::create(*this, effected);
			effect->initialize();
			if (runtime::as<Player>(effected)) {
				if (effect->getEffectResult() == EffectResult::CONFLICT)
					blockedStance = true;
			}
			const int32_t worldId = effector->getWorldId();
			const int32_t instanceId = effector->getInstanceId();
			effect->setWorldPosition(worldId, instanceId, x.get(), y.get(), z.get());
			effects.push_back(effect);
			Ptr<Creature> target = firstTarget.get();
			if (!target || (effected && target->equals(*effected)))
				dashStatus = getId(effect->getDashStatus());
			if (effect->getAttackStatus() == AttackStatus::RESIST || effect->getAttackStatus() == AttackStatus::DODGE) {
				resistCount++;
			}
		}

		if (resistCount == effectedList.size()) {
			blockedChain = true;
			blockedPenaltySkill.set(true);
		}

		// exception for point point skills(example Ice Sheet)
		if (effectedList.isEmpty()) {
			if (this->isPointPointSkill()) {
				Ref<Effect> effect = Effect::create(*this, nullptr);
				effect->initialize();
				effect->setWorldPosition(effector->getWorldId(), effector->getInstanceId(), x.get(), y.get(), z.get());
				effects.push_back(effect);
			}
		}
	}

	bool setCooldownsValue = true;
	if (Ptr<Player> playerEffector = runtime::as<Player>(effector)) {
		if (skillTemplate->isStance() && !blockedStance && skillMethod.get() == SkillMethod::CAST)
			playerEffector->getController().startStance(skillTemplate->getSkillId());
		if (getMultiCastCount() > 0)
			setCooldownsValue = false;

		// Check Chain Skill Trigger Rate, only for chain skills and only for player
		if (!chainCategory.get().empty()) {
			if (blockedChain)
				chainSuccess.set(false);
			else
				chainSuccess.set(commons::utils::Rnd::chance() < static_cast<float>(skillTemplate->getChainSkillProb())
					|| configs::main::CustomConfig::SKILL_CHAIN_DISABLE_TRIGGERRATE);

			if (chainSuccess.get())
				playerEffector->getChainSkills()->updateChain(chainCategory.get(), chainUsageDuration.get());
			else
				playerEffector->getChainSkills()->resetChain();
		}

		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(effector->getTarget(), *playerEffector, 0);
		questEngine::QuestEngine::getInstance().onUseSkill(*env, skillTemplate->getSkillId());
	}

	if (setCooldownsValue)
		setCooldowns();

	// Use penalty skill (now 100% success)
	if (!blockedPenaltySkill.get())
		startPenaltySkill();

	if (Ptr<Player> playerEffector = runtime::as<Player>(effector); isHostile() && playerEffector)
		playerEffector->getController().enterCombat(true);

	bool isItemSkill = skillMethod.get() == SkillMethod::ITEM;
	bool sentCastSpellResultPacket = false;
	// the client must learn the hit time before any HP change reaches it, or it updates the status bar before displaying the hit
	if (isItemSkill)
		sentCastSpellResultPacket = sendCastSpellEnd(dashStatus, effects);

	// item skills apply their effects immediately, hitTime only tells the client when to display the hit
	if (isInstantSkill() || isItemSkill)
		applyEffect(effects);
	else // Java: schedule(() -> applyEffect(effects), hitTime) - fieldmap Skill@L663: pins this Skill, captures the list of new effects
		utils::ThreadPoolManager::getInstance().schedule({this}, [this, effects] { applyEffect(effects); }, hitTime.get());

	if (skillMethod.get() == SkillMethod::PENALTY || skillMethod.get() == SkillMethod::CAST || isItemSkill) {
		if (!isItemSkill)
			sentCastSpellResultPacket = sendCastSpellEnd(dashStatus, effects);
		Ptr<Player> player = runtime::as<Player>(effector);
		if (sentCastSpellResultPacket && skillMethod.get() != SkillMethod::PENALTY && player) {
			// animation times must be calculated after applyEffect of instant skills in order to honor speed buffs from this skill
			std::optional<dataholders::MotionData::AnimationTimes> animation = DataManager::MOTION_DATA->calculateAnimationTimesAfterLastHit(*player, *this);
			int64_t nowMillis = commons::utils::currentTimeMillis();
			if (animation.has_value() && allowAnimationBoostByCastSpeed()) {
				int32_t latencyToleranceMillis = 50; // animation starts after client receives SM_CASTSPELL_RESULT, so add a few milliseconds
				player->setHitTimeBoost(nowMillis + animation->fullDurationMillis + latencyToleranceMillis, getCastSpeedForAnimationBoostAndChargeSkills());
			} else {
				player->setHitTimeBoost(0, 0);
			}
			if (animation.has_value()) // Math.max because nextSkillUse set from startCast() must not be undercut
				player->setNextSkillUse(std::max(player->getNextSkillUse(), nowMillis + animation->lastHitMillis));
		}
	}

	if (Ptr<Player> player = runtime::as<Player>(effector); skillTemplate->isDeityAvatar() && player) {
		services::abyss::AbyssService::announceAbyssSkillUsage(*player, skillTemplate->getL10n());
	}

	effector->getAi().onEndUseSkill(skillTemplate, skillLevel);
	if (Ptr<Npc> npc = runtime::as<Npc>(effector)) {
		Ptr<gameserver::model::skill::NpcSkillEntry> lastSkill = npc->getGameStats()->getLastSkill();
		if (lastSkill)
			lastSkill->fireOnEndCastEvents(*npc);

		ai::manager::SkillAttackManager::afterUseSkill(*runtime::cast<ai::NpcAI>(npc->getAi()));
	}

	if (skillMethod.get() == SkillMethod::CAST) {
		effector->getObserveController()->notifyEndSkillCastObservers(*this);
	}
	effector->getWorldMapInstance()->getInstanceHandler()->onEndCastSkill(*this);
}

void Skill::removeObservers() {
	if (Ptr<DeathObserver> observer = firstTargetDieObserver.get()) {
		firstTarget->getObserveController()->removeObserver(*observer);
		// C++ breaker (cycles.toml Skill.firstTargetDieObserver): Java keeps the field, so the Skill and its observer's lambda keep each other
		firstTargetDieObserver.set(nullptr);
	}
	effector->getObserveController()->removeObserver(*moveListener);
}

void Skill::addResistedEffectHateAndNotifyFriends(const std::vector<runtime::Ref<Effect>>& effects) {
	if (effects.empty()) { // Java: effects == null || effects.isEmpty() - the reference is never null here
		return;
	}
	for (const Ref<Effect>& effect : effects) {
		if (!(effect->getTauntHate() >= 0 && (effect->getAttackStatus() == AttackStatus::RESIST || effect->getAttackStatus() == AttackStatus::DODGE)))
			continue;
		Ptr<Creature> effected = effect->getEffected();
		effected->getAggroList().addHate(*effector, 1);
		effected->getKnownList().forEachNpc(
			[&effected](Npc& object) { object.getAi().onCreatureEvent(ai::event::AIEventType::CREATURE_NEEDS_SUPPORT, *effected); });
	}
}

void Skill::applyEffect(const std::vector<runtime::Ref<Effect>>& effects) {
	// Apply effects to effected objects
	for (const Ref<Effect>& effect : effects)
		effect->applyEffect();

	if (isHostile()) {
		for (const Ref<Effect>& effect : effects) {
			if (Ptr<Player> effectedPlayer = runtime::as<Player>(effect->getEffected()))
				effectedPlayer->getController().enterCombat(false);
		}
	}

	addResistedEffectHateAndNotifyFriends(effects);
}

bool Skill::isInvalidRecall() {
	if (!skillTemplate->hasRecallInstant())
		return false;
	Ptr<Player> caster = runtime::as<Player>(effector);
	// Java passes the nullable first target to validateCast, which answers "not valid" for a non-player; the C++ signature takes a reference
	// (services/RecallService.h), so a null first target dereferences here
	return caster && !services::RecallService::validateCast(*caster, *firstTarget.get());
}

bool Skill::isHostile() {
	SkillSubType subType = skillTemplate->getSubType();
	return subType == SkillSubType::ATTACK || subType == SkillSubType::DEBUFF;
}

bool Skill::sendCastSpellEnd(int32_t dashStatus, const std::vector<runtime::Ref<Effect>>& effects) {
	bool sentCastSpellPacket = false;
	if (itemTemplate != nullptr && !itemTemplate->isCombatActivated()) {
		PacketSendUtility::broadcastPacketAndReceive(*effector, SM_ITEM_USAGE_ANIMATION(effector->getObjectId(), firstTarget->getObjectId(), itemObjectId.get(),
			itemTemplate->getTemplateId(), 0, 1, 0));
	} else {
		std::optional<ai::event::AIEventType> et =
			skillTemplate->getSubType() == SkillSubType::ATTACK ? std::optional(ai::event::AIEventType::CREATURE_NEEDS_HELP) : std::nullopt;
		switch (targetType.get()) {
			case 0: // PlayerObjectId as Target
			case 3: // Target not in sight?
				PacketSendUtility::broadcastPacketAndReceive(*effector,
					SM_CASTSPELL_RESULT(*this, borrow(effects), hitTime.get(), chainSuccess.get(), dashStatus), et);
				sentCastSpellPacket = true;
				break;
			case 1: // XYZ as Target
				PacketSendUtility::broadcastPacketAndReceive(*effector,
					SM_CASTSPELL_RESULT(*this, borrow(effects), hitTime.get(), chainSuccess.get(), dashStatus, targetType.get()), et);
				sentCastSpellPacket = true;
				break;
		}
	}
	if (Ptr<Player> player = runtime::as<Player>(effector); skillMethod.get() == SkillMethod::ITEM && player)
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(getItemTemplate()->getL10n()));
	return sentCastSpellPacket;
}

bool Skill::payCastCosts() {
	if (!canPayCastCosts()) // nothing may be paid before it is certain that everything can be paid
		return false;

	// try removing item, if its not possible return to prevent exploits
	if (Ptr<Player> itemUser = runtime::as<Player>(effector); skillMethod.get() == SkillMethod::ITEM && itemUser) {
		Ptr<Item> item = itemUser->getInventory().getItemByObjId(itemObjectId.get());
		if (!item)
			return false;
		if (item->getActivationCount() > 1)
			item->setActivationCount(item->getActivationCount() - 1);
		else if (!itemUser->getInventory().decreaseByObjectId(item->getObjectId(), 1, services::item::ItemPacketService_ItemUpdateType::DEC_ITEM_USE))
			return false;
		itemUser->startCooldown(*item);
	}

	if (!endCondCheck())
		return false;

	// Perform necessary actions (use mp,dp items etc)
	const action::Actions* skillActions = skillTemplate->getActions();
	if (skillActions != nullptr) {
		for (const std::unique_ptr<action::Action>& action : skillActions->getActions()) {
			if (!action->act(*this))
				return false;
		}
	}
	return true;
}

bool Skill::preCastCheck() {
	const condition::Conditions* skillConditions = skillTemplate->getStartconditions();
	return skillConditions == nullptr || skillConditions->validate(*this);
}

bool Skill::preUsageCheck() {
	const condition::Conditions* skillConditions = skillTemplate->getUseconditions();
	return skillConditions == nullptr || skillConditions->validate(*this);
}

bool Skill::endCondCheck() {
	const condition::Conditions* skillConditions = skillTemplate->getEndConditions();
	return skillConditions == nullptr || skillConditions->validate(*this);
}

int32_t Skill::getSkillId() {
	return skillTemplate->getSkillId();
}

bool Skill::isPassive() {
	return skillTemplate->getActivationAttribute() == ActivationAttribute::PASSIVE;
}

std::optional<properties::FirstTargetAttribute> Skill::getFirstTargetAttribute() {
	if (skillTemplate->getProperties() == nullptr)
		return std::nullopt;
	return skillTemplate->getProperties()->getFirstTarget();
}

std::optional<properties::TargetRangeAttribute> Skill::getTargetRangeAttribute() {
	return skillTemplate->getProperties() == nullptr ? std::nullopt : skillTemplate->getProperties()->getTargetType();
}

bool Skill::isNonTargetAOE() {
	return getFirstTargetAttribute() == FirstTargetAttribute::ME && getTargetRangeAttribute() == TargetRangeAttribute::AREA;
}

bool Skill::isTargetAOE() {
	return getFirstTargetAttribute() == FirstTargetAttribute::TARGET && getTargetRangeAttribute() == TargetRangeAttribute::AREA;
}

bool Skill::isSelfBuff() {
	return getFirstTargetAttribute() == FirstTargetAttribute::ME && getTargetRangeAttribute() == TargetRangeAttribute::ONLYONE
		&& skillTemplate->getSubType() == SkillSubType::BUFF && !skillTemplate->isDeityAvatar();
}

bool Skill::isFirstTargetSelf() {
	return getFirstTargetAttribute() == FirstTargetAttribute::ME;
}

bool Skill::isPointSkill() {
	return getFirstTargetAttribute() == FirstTargetAttribute::POINT;
}

bool Skill::allowAnimationBoostByCastSpeed() {
	return skillTemplate->isApplyCastingTimeBonus();
}

bool Skill::isCastDurationAffectedByCastSpeed() {
	return skillMethod.get() == SkillMethod::CAST && skillTemplate->isApplyCastingTimeBonus();
}

bool Skill::isPointPointSkill() {
	return requireProperties(this->getSkillTemplate()).getFirstTarget() == FirstTargetAttribute::POINT
		&& requireProperties(this->getSkillTemplate()).getTargetType() == TargetRangeAttribute::POINT;
}

int32_t Skill::getMultiCastCount() {
	Ptr<Player> p = runtime::as<Player>(effector);
	return skillTemplate->isMultiCast() && p ? p->getChainSkills()->getCurrentChainCount(chainCategory.get()) : 0;
}

bool Skill::isInstantSkill() {
	return hitTime.get() == 0 || (skillTemplate->getMotion() != nullptr && skillTemplate->getMotion()->isInstantSkill());
}

} // namespace aion::gameserver::skillengine::model
