#include "aion/gameserver/controllers/ObserveController.h"

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::controllers {

ObserveController::ObserveController() = default;

ObserveController::~ObserveController() = default;

runtime::Ref<ObserveController> ObserveController::create() {
	return runtime::makeRef<ObserveController>();
}

void ObserveController::attach(observer::ActionObserver& observer) {
	AION_UNPORTED();
}

void ObserveController::addObserver(observer::ActionObserver& observer) {
	AION_UNPORTED();
}

void ObserveController::addAttackCalcObserver(observer::AttackCalcObserver& observer) {
	AION_UNPORTED();
}

void ObserveController::removeObserver(observer::ActionObserver& observer) {
	AION_UNPORTED();
}

void ObserveController::removeAttackCalcObserver(observer::AttackCalcObserver& observer) {
	AION_UNPORTED();
}

void ObserveController::notifyObservers(observer::ObserverType type, std::initializer_list<std::any> object) {
	AION_UNPORTED();
}

void ObserveController::notifyAction(observer::ObserverType type, observer::ActionObserver& observer, std::span<const std::any> object) {
	AION_UNPORTED();
}

void ObserveController::notifyDeathObservers(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

void ObserveController::notifyMoveObservers() {
	AION_UNPORTED();
}

void ObserveController::notifySitObservers() {
	AION_UNPORTED();
}

void ObserveController::notifyAttackObservers(model::gameobjects::Creature& creature, int32_t skillId) {
	AION_UNPORTED();
}

void ObserveController::notifyAttackedObservers(model::gameobjects::Creature& creature, int32_t skillId) {
	AION_UNPORTED();
}

void ObserveController::notifyDotAttackedObservers(model::gameobjects::Creature& creature, skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void ObserveController::notifyStartSkillCastObservers(skillengine::model::Skill& skill) {
	AION_UNPORTED();
}

void ObserveController::notifyEndSkillCastObservers(skillengine::model::Skill& skill) {
	AION_UNPORTED();
}

void ObserveController::notifyBoostSkillCostObservers(skillengine::model::Skill& skill) {
	AION_UNPORTED();
}

void ObserveController::notifyItemEquip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {
	AION_UNPORTED();
}

void ObserveController::notifyItemUnEquip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {
	AION_UNPORTED();
}

void ObserveController::abortItemUseObservers() {
	AION_UNPORTED();
}

void ObserveController::notifyItemuseObservers(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void ObserveController::notifyAbnormalSettedObservers(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

void ObserveController::notifySummonReleaseObservers() {
	AION_UNPORTED();
}

void ObserveController::notifyHPChangeObservers(int32_t hpValue) {
	AION_UNPORTED();
}

bool ObserveController::checkAttackStatus(attack::AttackStatus status) {
	AION_UNPORTED();
}

bool ObserveController::checkAttackerStatus(attack::AttackStatus status) {
	AION_UNPORTED();
}

runtime::Ref<observer::AttackerCriticalStatus> ObserveController::checkAttackerCriticalStatus(attack::AttackStatus status, bool isSkill) {
	AION_UNPORTED();
}

void ObserveController::checkShieldStatus(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList,
	runtime::Ptr<skillengine::model::Effect> effect, model::gameobjects::Creature& attacker) {
	AION_UNPORTED();
}

void ObserveController::checkShieldStatus(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList,
	runtime::Ptr<skillengine::model::Effect> effect, model::gameobjects::Creature& attacker,
	std::optional<skillengine::model::ShieldType> shieldType) {
	AION_UNPORTED();
}

float ObserveController::getBasePhysicalDamageMultiplier(bool isSkill) {
	AION_UNPORTED();
}

float ObserveController::getBaseMagicalDamageMultiplier() {
	AION_UNPORTED();
}

void ObserveController::clear() {
	AION_UNPORTED();
}

void ObserveController::clearWithoutNotify() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
