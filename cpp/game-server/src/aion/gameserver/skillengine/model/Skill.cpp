#include "aion/gameserver/skillengine/model/Skill.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::model {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.model.Skill");

Skill::Skill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::player::Player& effectorValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue)
	: Skill(skillTemplateValue, effectorValue, 0, firstTargetValue, nullptr) {
	// Java: this(skillTemplate, effector, effector.getSkillList().getSkillLevel(skillTemplate.getSkillId()), firstTarget, null)
	AION_UNPORTED();
}

Skill::Skill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::player::Player& effectorValue,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue, int32_t skillLevelValue)
	: Skill(skillTemplateValue, effectorValue, skillLevelValue, firstTargetValue, nullptr) {
}

Skill::Skill(const SkillTemplate* skillTemplateValue, gameserver::model::gameobjects::Creature& effectorValue, int32_t skillLvl,
	runtime::Ptr<gameserver::model::gameobjects::Creature> firstTargetValue, const gameserver::model::templates::item::ItemTemplate* itemTemplateValue)
	: firstTarget(firstTargetValue), effector(effectorValue), skillLevel(skillLvl), moveListener(nullptr), skillTemplate(skillTemplateValue),
	  itemTemplate(itemTemplateValue), baseCastDuration(skillTemplateValue->getDuration()), castDuration(skillTemplateValue->getDuration()) {
	// Java: this.moveListener = new StartMovingListener(); then initializeSkillMethod() (the subclass override in Java, see Skill.h)
	AION_UNPORTED();
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
	AION_UNPORTED();
}

bool Skill::canUseSkill(properties::Properties_CastState castState) {
	AION_UNPORTED();
}

bool Skill::canPayCastCosts() {
	AION_UNPORTED();
}

bool Skill::validateEffectedList() {
	AION_UNPORTED();
}

bool Skill::canUseSkill(gameserver::model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool Skill::isValidTarget(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

bool Skill::useSkill() {
	AION_UNPORTED();
}

bool Skill::useNoAnimationSkill() {
	AION_UNPORTED();
}

bool Skill::useWithoutPropSkill() {
	AION_UNPORTED();
}

// Stored method references com.aionemu.gameserver.skillengine.model.Skill@L312:45 (cancelCurrentSkillCast) and @L314:45 (endCast), scheduled
// with pin {this}: TaskStructs here once ported
bool Skill::useSkill(bool checkAnimation, bool checkproperties) {
	AION_UNPORTED();
}

void Skill::setCooldowns() {
	AION_UNPORTED();
}

int32_t Skill::getCooldown() {
	AION_UNPORTED();
}

void Skill::updateCastDurationAndSpeed() {
	AION_UNPORTED();
}

int32_t Skill::calculateChargeCastDuration() {
	AION_UNPORTED();
}

int32_t Skill::calculateCastDuration() {
	AION_UNPORTED();
}

int32_t Skill::calculateMagicalCastDuration() {
	AION_UNPORTED();
}

std::optional<gameserver::model::stats::container::StatEnum> Skill::getSkillCastBoostStat() {
	AION_UNPORTED();
}

bool Skill::isSummonType(SkillSubType type) {
	AION_UNPORTED();
}

void Skill::updateHitTime(bool checkAnimation) {
	AION_UNPORTED();
}

float Skill::getDistanceTolerance(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

bool Skill::isSuspiciousClientHitTime(int32_t clientHitTimeValue, int32_t serverHitTime, int32_t tolerance, gameserver::model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

std::vector<std::string> Skill::collectUncertaintyFactorsForHitTime(gameserver::model::gameobjects::player::Player& player, int32_t toleranceMillis) {
	AION_UNPORTED();
}

void Skill::startPenaltySkill() {
	AION_UNPORTED();
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Skill@L535:47 (DeathObserver callback, pin {this}): a callback struct here once ported
void Skill::startCast() {
	AION_UNPORTED();
}

void Skill::cancelCast() {
	AION_UNPORTED();
}

void Skill::cancelCurrentSkillCast() {
	AION_UNPORTED();
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Skill@L663:45 (hit time task, pin {this}, captures effects as
// const Ref<RcArrayList<Ref<Effect>>>): a TaskStruct here once ported
void Skill::endCast() {
	AION_UNPORTED();
}

void Skill::removeObservers() {
	AION_UNPORTED();
}

void Skill::addResistedEffectHateAndNotifyFriends(const std::vector<runtime::Ref<Effect>>& effects) {
	AION_UNPORTED();
}

void Skill::applyEffect(const std::vector<runtime::Ref<Effect>>& effects) {
	AION_UNPORTED();
}

bool Skill::isInvalidRecall() {
	AION_UNPORTED();
}

bool Skill::isHostile() {
	AION_UNPORTED();
}

bool Skill::sendCastSpellEnd(int32_t dashStatus, const std::vector<runtime::Ref<Effect>>& effects) {
	AION_UNPORTED();
}

bool Skill::payCastCosts() {
	AION_UNPORTED();
}

bool Skill::preCastCheck() {
	AION_UNPORTED();
}

bool Skill::preUsageCheck() {
	AION_UNPORTED();
}

bool Skill::endCondCheck() {
	AION_UNPORTED();
}

int32_t Skill::getSkillId() {
	AION_UNPORTED();
}

bool Skill::isPassive() {
	AION_UNPORTED();
}

std::optional<properties::FirstTargetAttribute> Skill::getFirstTargetAttribute() {
	AION_UNPORTED();
}

std::optional<properties::TargetRangeAttribute> Skill::getTargetRangeAttribute() {
	AION_UNPORTED();
}

bool Skill::isNonTargetAOE() {
	AION_UNPORTED();
}

bool Skill::isTargetAOE() {
	AION_UNPORTED();
}

bool Skill::isSelfBuff() {
	AION_UNPORTED();
}

bool Skill::isFirstTargetSelf() {
	AION_UNPORTED();
}

bool Skill::isPointSkill() {
	AION_UNPORTED();
}

bool Skill::allowAnimationBoostByCastSpeed() {
	AION_UNPORTED();
}

bool Skill::isCastDurationAffectedByCastSpeed() {
	AION_UNPORTED();
}

bool Skill::isPointPointSkill() {
	AION_UNPORTED();
}

int32_t Skill::getMultiCastCount() {
	AION_UNPORTED();
}

bool Skill::isInstantSkill() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
