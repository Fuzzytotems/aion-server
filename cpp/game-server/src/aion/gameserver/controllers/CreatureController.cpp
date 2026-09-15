#include "aion/gameserver/controllers/CreatureController.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/observer/TerrainZoneCollisionMaterialActor.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.CreatureController");

/**
 * Java: private static final class DelayedOnAttack implements Runnable, scheduled by attackTarget. C++: K4 (fieldmap.toml [kinds]) because the
 * task outlives attackTarget: RefCounted, created with create(), the members Refs; run() keeps Java's clearing of the references (a one-shot
 * task: the pending Future is its only holder).
 */
class CreatureController::DelayedOnAttack final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::gameobjects::Creature>> target;
	runtime::Field<runtime::Ref<model::gameobjects::Creature>> creature;
	const int32_t finalDamage;
	const attack::AttackStatus attackStatus;
	runtime::Field<runtime::Ref<skillengine::model::Effect>> criticalProcEffect;

protected:
	DelayedOnAttack(model::gameobjects::Creature& target, model::gameobjects::Creature& creature, int32_t finalDamage,
		attack::AttackStatus attackStatus, runtime::Ptr<skillengine::model::Effect> criticalProcEffect);
	~DelayedOnAttack() override;

public:
	/** Java: new DelayedOnAttack(target, creature, finalDamage, attackStatus, criticalProcEffect) */
	static runtime::Ref<DelayedOnAttack> create(model::gameobjects::Creature& target, model::gameobjects::Creature& creature, int32_t finalDamage,
		attack::AttackStatus attackStatus, runtime::Ptr<skillengine::model::Effect> criticalProcEffect);

	void run();
};

CreatureController::DelayedOnAttack::DelayedOnAttack(model::gameobjects::Creature& targetValue, model::gameobjects::Creature& creatureValue,
	int32_t finalDamageValue, attack::AttackStatus attackStatusValue,
	runtime::Ptr<skillengine::model::Effect> criticalProcEffectValue)
	: target(runtime::Ref<model::gameobjects::Creature>(targetValue)), creature(runtime::Ref<model::gameobjects::Creature>(creatureValue)),
	  finalDamage(finalDamageValue), attackStatus(attackStatusValue),
	  criticalProcEffect(runtime::Ref<skillengine::model::Effect>(criticalProcEffectValue)) {
}

CreatureController::DelayedOnAttack::~DelayedOnAttack() = default;

runtime::Ref<CreatureController::DelayedOnAttack> CreatureController::DelayedOnAttack::create(model::gameobjects::Creature& targetValue,
	model::gameobjects::Creature& creatureValue, int32_t finalDamageValue, attack::AttackStatus attackStatusValue,
	runtime::Ptr<skillengine::model::Effect> criticalProcEffectValue) {
	return runtime::makeRef<DelayedOnAttack>(targetValue, creatureValue, finalDamageValue, attackStatusValue, criticalProcEffectValue);
}

void CreatureController::DelayedOnAttack::run() {
	AION_UNPORTED();
}

model::gameobjects::Creature& CreatureController::getOwner() const {
	return static_cast<model::gameobjects::Creature&>(VisibleObjectController::getOwner());
}

CreatureController::CreatureController() = default;

CreatureController::~CreatureController() = default;

void CreatureController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

void CreatureController::notKnow(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void CreatureController::onHide() {
	AION_UNPORTED();
}

void CreatureController::onHideEnd() {
	AION_UNPORTED();
}

void CreatureController::onStartMove() {
	AION_UNPORTED();
}

void CreatureController::onMove() {
	AION_UNPORTED();
}

void CreatureController::onStopMove() {
	AION_UNPORTED();
}

void CreatureController::notifyAIOnMove() {
	AION_UNPORTED();
}

void CreatureController::updateZone() {
	AION_UNPORTED();
}

void CreatureController::onDie(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

void CreatureController::onAddHate(model::gameobjects::Creature& attacker, bool isNewInAggroList) {
	AION_UNPORTED();
}

void CreatureController::onAttack(model::gameobjects::Creature& creature, int32_t damage, std::optional<attack::AttackStatus> attackStatus) {
	AION_UNPORTED();
}

void CreatureController::onAttack(model::gameobjects::Creature& creature, int32_t damage, attack::AttackStatus attackStatus,
	runtime::Ptr<skillengine::model::Effect> criticalProcEffect) {
	AION_UNPORTED();
}

void CreatureController::onAttack(skillengine::model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage,
	bool notifyAttack, network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, skillengine::model::HopType hopType) {
	AION_UNPORTED();
}

void CreatureController::onAttack(skillengine::model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage,
	bool notifyAttack, network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, skillengine::model::HopType hopType, bool criticalHit) {
	AION_UNPORTED();
}

void CreatureController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> status,
	std::optional<skillengine::model::HopType> hopType) {
	AION_UNPORTED();
}

void CreatureController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> status,
	std::optional<skillengine::model::HopType> hopType, runtime::Ptr<skillengine::model::Effect> criticalProcEffect, bool criticalHit) {
	AION_UNPORTED();
}

void CreatureController::calculateGodStoneEffects(model::gameobjects::player::Player& attacker) {
	AION_UNPORTED();
}

void CreatureController::applyGodStoneEffect(model::gameobjects::player::Player& attacker, runtime::Ptr<model::gameobjects::Item> weapon,
	bool isMainHandWeapon) {
	AION_UNPORTED();
}

void CreatureController::attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) {
	AION_UNPORTED();
}

bool CreatureController::hasTask(model::TaskId taskId) {
	AION_UNPORTED();
}

bool CreatureController::hasScheduledTask(model::TaskId taskId) {
	AION_UNPORTED();
}

runtime::FutureRef CreatureController::getAndRemoveTask(model::TaskId taskId) {
	AION_UNPORTED();
}

runtime::FutureRef CreatureController::cancelTask(model::TaskId taskId) {
	AION_UNPORTED();
}

bool CreatureController::cancelTaskIfPresent(model::TaskId taskId, runtime::FutureRef task) {
	AION_UNPORTED();
}

void CreatureController::addTask(model::TaskId taskId, runtime::FutureRef task) {
	AION_UNPORTED();
}

void CreatureController::cancelAllTasks() {
	AION_UNPORTED();
}

void CreatureController::onDelete() {
	AION_UNPORTED();
}

bool CreatureController::die() {
	AION_UNPORTED();
}

bool CreatureController::die(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

bool CreatureController::die(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> value, model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

bool CreatureController::useSkill(int32_t skillId) {
	AION_UNPORTED();
}

bool CreatureController::useSkill(int32_t skillId, int32_t skillLevel) {
	AION_UNPORTED();
}

bool CreatureController::useChargeSkill(skillengine::model::Skill& startSkill, int64_t chargeTimeMillis) {
	AION_UNPORTED();
}

runtime::Ptr<skillengine::model::Skill> CreatureController::abortCast() {
	AION_UNPORTED();
}

void CreatureController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker) {
	AION_UNPORTED();
}

void CreatureController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker,
	network::aion::serverpackets::SM_SYSTEM_MESSAGE* msg) {
	AION_UNPORTED();
}

void CreatureController::onAfterSpawn() {
	AION_UNPORTED();
}

void CreatureController::onDespawn() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
