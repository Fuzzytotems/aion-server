#include "aion/gameserver/controllers/SummonController.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/summons/SkillOrder.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/taskmanager/tasks/PlayerMoveTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers {

using model::gameobjects::Creature;
using model::gameobjects::Summon;
using model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;
using services::summons::SummonsService;
using utils::PacketSendUtility;

SummonController::SummonController() = default;

SummonController::~SummonController() = default;

model::gameobjects::Summon& SummonController::getOwner() const {
	return static_cast<model::gameobjects::Summon&>(CreatureController::getOwner());
}

void SummonController::notKnow(model::gameobjects::VisibleObject& object) {
	CreatureController::notKnow(object);
	if (getOwner().getMaster()->equals(object))
		SummonsService::release(getOwner(), model::summons::UnsummonType::DISTANCE);
}

void SummonController::release(model::summons::UnsummonType unsummonType) {
	SummonsService::release(getOwner(), unsummonType);
}

void SummonController::restMode() {
	SummonsService::restMode(getOwner());
}

void SummonController::setUnkMode() {
	SummonsService::setUnkMode(getOwner());
}

void SummonController::guardMode() {
	SummonsService::guardMode(getOwner());
}

void SummonController::attackMode(int32_t targetObjId) {
	if (canAttack(targetObjId))
		SummonsService::attackMode(getOwner());
}

bool SummonController::canAttack(int32_t targetObjId) {
	Ptr<Creature> creature = runtime::as<Creature>(getOwner().getKnownList().getObject(targetObjId));
	return creature && getOwner().isEnemy(*creature);
}

void SummonController::attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) {
	if (target->isDead() || target->getLifeStats()->isAboutToDie() || !getOwner().isEnemy(*target)) {
		PacketSendUtility::sendPacket(*getMaster(), network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}

	int32_t attackSpeed = getOwner().getGameStats()->getAttackSpeed()->getCurrent();
	int64_t now = commons::utils::currentTimeMillis();
	int64_t msSinceLastAttack = now - lastAttackMillis.get();
	if (msSinceLastAttack < attackSpeed && attackSpeed - msSinceLastAttack > 50) { // 50ms tolerance
		utils::audit::AuditLogger::log(*getMaster(), "possibly used hack to speed up summon auto-attack (" + std::to_string(msSinceLastAttack) +
			"ms instead of " + std::to_string(attackSpeed) + ")");
		return;
	}
	lastAttackMillis = now;
	CreatureController::attackTarget(target, time, false);
}

void SummonController::onAttack(model::gameobjects::Creature& creature, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack, network::aion::serverpackets::SM_ATTACK_STATUS_LOG log,
	std::optional<attack::AttackStatus> attackStatus, std::optional<skillengine::model::HopType> hopType) {
	if (getOwner().isDead())
		return;

	if (getOwner().isReleaseUncancelable())
		return;

	CreatureController::onAttack(creature, effect, type, damage, notifyAttack, log, attackStatus, hopType);
	PacketSendUtility::sendPacket(*runtime::cast<Player>(getOwner().getMaster()), network::aion::serverpackets::SM_SUMMON_UPDATE(getOwner()));
}

void SummonController::onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
	runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {
	CreatureController::onTargetChanged(oldTarget, newTarget);
	getOwner().clearSkillOrders();
}

void SummonController::onDespawn() {
	if (getOwner().getMode() == model::summons::SummonMode::RELEASE)
		getOwner().getEffectController()->removeAllEffects();
	CreatureController::onDespawn();
}

void SummonController::onDie(model::gameobjects::Creature& lastAttacker) {
	CreatureController::onDie(lastAttacker);
	SummonsService::release(getOwner(), model::summons::UnsummonType::SUMMON_DEATH);
}

void SummonController::useSkill(model::summons::SkillOrder& order) {
	Creature& creature = getOwner();
	if (!dataholders::DataManager::PET_SKILL_DATA->petHasSkill(getOwner().getObjectTemplate()->getTemplateId(), order.getSkillId())) {
		// hackers!)
		return;
	}
	Ref<skillengine::model::Skill> skill = skillengine::SkillEngine::getInstance().getSkill(creature, order.getSkillId(), 1, order.getTarget());
	skill->setHate(order.getHate());
	if (skill->useSkill() && order.isRelease()) {
		SummonsService::release(getOwner(), model::summons::UnsummonType::SKILL_ORDER);
	}
}

void SummonController::onStartMove() {
	CreatureController::onStartMove();
	taskmanager::tasks::PlayerMoveTaskManager::getInstance().addPlayer(getOwner());
	updateZone();
}

void SummonController::onStopMove() {
	CreatureController::onStopMove();
	taskmanager::tasks::PlayerMoveTaskManager::getInstance().removePlayer(getOwner());
}

runtime::Ptr<model::gameobjects::player::Player> SummonController::getMaster() {
	return runtime::cast<Player>(getOwner().getMaster());
}

} // namespace aion::gameserver::controllers
