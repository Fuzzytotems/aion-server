#include "aion/gameserver/controllers/PlayerController.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/observer/StanceObserver.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.PlayerController");

PlayerController::PlayerController() = default;

PlayerController::~PlayerController() = default;

model::gameobjects::player::Player& PlayerController::getOwner() const {
	return static_cast<model::gameobjects::player::Player&>(CreatureController::getOwner());
}

void PlayerController::see(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void PlayerController::sendPlayerInfoPackets(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

void PlayerController::onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
	runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {
	AION_UNPORTED();
}

void PlayerController::onHide() {
	AION_UNPORTED();
}

void PlayerController::onHideEnd() {
	AION_UNPORTED();
}

void PlayerController::updateNearbyQuests() {
	AION_UNPORTED();
}

void PlayerController::updateRepeatableQuests() {
	AION_UNPORTED();
}

void PlayerController::onEnterZone(world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void PlayerController::onLeaveZone(world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void PlayerController::onLeaveFlyArea() {
	AION_UNPORTED();
}

void PlayerController::onEnterFlyArea() {
	AION_UNPORTED();
}

void PlayerController::onEnterWorld() {
	AION_UNPORTED();
}

void PlayerController::onDie(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

void PlayerController::setRebirthReviveInfo() {
	AION_UNPORTED();
}

void PlayerController::onDespawn() {
	AION_UNPORTED();
}

// callbacks: com.aionemu.gameserver.controllers.PlayerController@L356:44 (scheduled resurrection options task)
void PlayerController::scheduleShowResurrectionOptions() {
	AION_UNPORTED();
}

void PlayerController::showResurrectionOptions() {
	AION_UNPORTED();
}

bool PlayerController::isInvader(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerController::doReward() {
	AION_UNPORTED();
}

void PlayerController::onBeforeSpawn() {
	AION_UNPORTED();
}

void PlayerController::attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) {
	AION_UNPORTED();
}

void PlayerController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> attackStatus,
	std::optional<skillengine::model::HopType> hopType) {
	AION_UNPORTED();
}

void PlayerController::useSkill(const skillengine::model::SkillTemplate* template_, int32_t targetType, float x, float y, float z,
	int32_t clientHitTime, int32_t skillLevel) {
	AION_UNPORTED();
}

void PlayerController::onStartMove() {
	AION_UNPORTED();
}

void PlayerController::onMove() {
	AION_UNPORTED();
}

void PlayerController::onStopMove() {
	AION_UNPORTED();
}

void PlayerController::notifyAIOnMove() {
	AION_UNPORTED();
}

void PlayerController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker) {
	AION_UNPORTED();
}

void PlayerController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker,
	network::aion::serverpackets::SM_SYSTEM_MESSAGE* message) {
	AION_UNPORTED();
}

void PlayerController::cancelUseItem() {
	AION_UNPORTED();
}

void PlayerController::onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
	int32_t extendedRewardIndex) {
	AION_UNPORTED();
}

void PlayerController::onLevelChange(int32_t oldLevel, int32_t newLevel) {
	AION_UNPORTED();
}

void PlayerController::upgradePlayer() {
	AION_UNPORTED();
}

void PlayerController::onChangedPlayerAttributes() {
	AION_UNPORTED();
}

// callbacks: com.aionemu.gameserver.controllers.PlayerController@L632:79 (scheduled stopProtectionActiveTask)
void PlayerController::startProtectionActiveTask() {
	AION_UNPORTED();
}

void PlayerController::stopProtectionActiveTask() {
	AION_UNPORTED();
}

void PlayerController::onFlyTeleportEnd() {
	AION_UNPORTED();
}

void PlayerController::startStance(int32_t skillId) {
	AION_UNPORTED();
}

void PlayerController::stopStance() {
	AION_UNPORTED();
}

int32_t PlayerController::getStanceSkillId() {
	AION_UNPORTED();
}

bool PlayerController::isUnderStance() {
	AION_UNPORTED();
}

void PlayerController::updateSoulSickness(int32_t skillId) {
	AION_UNPORTED();
}

bool PlayerController::isInCombat() {
	AION_UNPORTED();
}

int64_t PlayerController::getLastCombatTime() {
	AION_UNPORTED();
}

void PlayerController::enterCombat(bool attacking) {
	AION_UNPORTED();
}

void PlayerController::breakStanceObserver() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
