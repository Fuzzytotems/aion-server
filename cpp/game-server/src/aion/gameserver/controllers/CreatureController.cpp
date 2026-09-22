#include "aion/gameserver/controllers/CreatureController.h"

#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/observer/TerrainZoneCollisionMaterialActor.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillChargeData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/AttackHandAnimation.h"
#include "aion/gameserver/model/animations/AttackTypeAnimation.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/GodstoneInfo.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/item/ItemAttackTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/condition/SkillChargeCondition.h"
#include "aion/gameserver/skillengine/model/ChargeSkillEntry.h"
#include "aion/gameserver/skillengine/model/ChargedSkill.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/Skill_SkillMethod.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"
#include "aion/gameserver/taskmanager/tasks/MovementNotifyTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.CreatureController");

using ai::event::AIEventType;
using attack::AttackResult;
using attack::AttackStatus;
using attack::AttackUtil;
using model::TaskId;
using model::gameobjects::Creature;
using model::gameobjects::Item;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::FutureRef;
using runtime::Ptr;
using runtime::Ref;
using skillengine::model::Effect;
using skillengine::model::HopType;
using skillengine::model::Skill;
using utils::PacketSendUtility;

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
	target->getController().onAttack(*creature, finalDamage, attackStatus, criticalProcEffect.get());
	target = nullptr;
	creature = nullptr;
	criticalProcEffect = nullptr;
}

model::gameobjects::Creature& CreatureController::getOwner() const {
	return static_cast<model::gameobjects::Creature&>(VisibleObjectController::getOwner());
}

CreatureController::CreatureController() = default;

CreatureController::~CreatureController() = default;

void CreatureController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	VisibleObjectController::notSee(object, animation);
	Ptr<VisibleObject> target = getOwner().getTarget();
	if (target && object.equals(*target) && getOwner().getAi().getSubState() != ai::AISubState::TARGET_LOST)
		getOwner().setTarget(nullptr);
}

void CreatureController::notKnow(model::gameobjects::VisibleObject& object) {
	VisibleObjectController::notKnow(object);
	if (Ptr<Creature> creature = runtime::as<Creature>(object))
		getOwner().getAggroList().remove(*creature);
}

void CreatureController::onHide() {
	getOwner().getKnownList().forEachObject([this](VisibleObject& other) { other.getKnownList().updateVisibleObject(getOwner()); });
}

void CreatureController::onHideEnd() {
	getOwner().getKnownList().forEachObject([this](VisibleObject& other) { other.getKnownList().updateVisibleObject(getOwner()); });
}

void CreatureController::onStartMove() {
	getOwner().getMoveController()->setInMove(true);
	getOwner().getObserveController()->notifyMoveObservers();
	notifyAIOnMove();
}

void CreatureController::onMove() {
	getOwner().getObserveController()->notifyMoveObservers();
	notifyAIOnMove();
	updateZone();
}

void CreatureController::onStopMove() {
	getOwner().getMoveController()->setInMove(false);
	getOwner().getObserveController()->notifyMoveObservers();
	notifyAIOnMove();
}

void CreatureController::notifyAIOnMove() {
	taskmanager::tasks::MovementNotifyTask::getInstance().add(getOwner());
}

void CreatureController::updateZone() {
	world::zone::ZoneUpdateService::getInstance().add(getOwner());
}

void CreatureController::onDie(model::gameobjects::Creature& lastAttacker) {
	Creature& self = getOwner();
	self.getMoveController()->abortMove();
	self.setCasting(nullptr);
	self.getEffectController()->removeAllEffects();
	if (Ptr<Player> player = runtime::as<Player>(self); player && player->getIsFlyingBeforeDeath()) {
		self.unsetState(CreatureState::ACTIVE);
		self.setState(CreatureState::FLOATING_CORPSE);
	} else
		self.setState(CreatureState::DEAD);
	self.getObserveController()->notifyDeathObservers(lastAttacker);
	PacketSendUtility::broadcastPacketAndReceive(self, network::aion::serverpackets::SM_EMOTION(self, model::EmotionType::DIE, 0,
		self.equals(lastAttacker) ? 0 : lastAttacker.getObjectId()));
	self.getKnownList().forEachObject([this](VisibleObject& o) {
		if (Ptr<Creature> creature = runtime::as<Creature>(o))
			creature->getAggroList().stopHating(getOwner());
	});
}

void CreatureController::onAddHate(model::gameobjects::Creature& attacker, bool isNewInAggroList) {
	getOwner().getAi().onCreatureEvent(AIEventType::ATTACK, attacker);
}

void CreatureController::onAttack(model::gameobjects::Creature& creature, int32_t damage, std::optional<attack::AttackStatus> attackStatus) {
	onAttack(creature, nullptr, SM_ATTACK_STATUS_TYPE::REGULAR, damage, true, SM_ATTACK_STATUS_LOG::REGULAR, attackStatus, HopType::DAMAGE);
}

void CreatureController::onAttack(model::gameobjects::Creature& creature, int32_t damage, attack::AttackStatus attackStatus,
	runtime::Ptr<skillengine::model::Effect> criticalProcEffect) {
	onAttack(creature, nullptr, SM_ATTACK_STATUS_TYPE::REGULAR, damage, true, SM_ATTACK_STATUS_LOG::REGULAR, attackStatus, HopType::DAMAGE,
		criticalProcEffect, false);
}

void CreatureController::onAttack(skillengine::model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage,
	bool notifyAttack, network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, skillengine::model::HopType hopType) {
	onAttack(effect, type, damage, notifyAttack, logId, hopType, false);
}

void CreatureController::onAttack(skillengine::model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage,
	bool notifyAttack, network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, skillengine::model::HopType hopType, bool criticalHit) {
	onAttack(*effect.getEffector(), effect, type, damage, notifyAttack, logId, effect.getAttackStatus(), hopType, nullptr, criticalHit);
}

void CreatureController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> status,
	std::optional<skillengine::model::HopType> hopType) {
	onAttack(attacker, effect, type, damage, notifyAttack, logId, status, hopType, nullptr, false);
}

void CreatureController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> status,
	std::optional<skillengine::model::HopType> hopType, runtime::Ptr<skillengine::model::Effect> criticalProcEffect, bool criticalHit) {
	Creature& self = getOwner();
	if (!self.isSpawned())
		return;
	if (damage != 0 && notifyAttack) {
		Ptr<Skill> skill = self.getCastingSkill();
		if (skill) {
			if (skill->getSkillMethod() == skillengine::model::Skill_SkillMethod::ITEM) {
				cancelCurrentSkill(attacker);
			} else {
				int32_t cancelRate = skill->getSkillTemplate()->getCancelRate();
				if (cancelRate > 0) {
					int32_t concentration = self.getGameStats()->getStat(model::stats::container::StatEnum::CONCENTRATION, 0)->getCurrent();
					float maxHp = static_cast<float>(self.getGameStats()->getMaxHp()->getCurrent());
					// chance per mille, driven by the share of max HP the hit took. Skills with an extreme cancel rate, like Return, Bandage Heal and
					// Herb Treatment, exceed 1000 for any noticeable hit and are therefore always interrupted.
					int32_t cancelChance = detail::toInt(static_cast<float>(damage) / maxHp * static_cast<float>(cancelRate) * 100 *
						(static_cast<float>(self.getCancelLevel()) / 100.0f) - static_cast<float>(concentration));
					if (cancelChance > 0 && commons::utils::Rnd::get(1, 1000) <= cancelChance)
						cancelCurrentSkill(attacker);
				}
			}
		}
		self.getObserveController()->notifyAttackedObservers(attacker, !effect ? 0 : effect->getSkillId());
	}

	self.getAggroList().addDamage(attacker, damage, notifyAttack, hopType);

	// notify all NPC's around that creature is attacking me
	self.getKnownList().forEachNpc([this](Npc& npc) { npc.getAi().onCreatureEvent(AIEventType::CREATURE_NEEDS_SUPPORT, getOwner()); });
	self.getLifeStats()->reduceHp(type, damage, !effect ? 0 : effect->getSkillId(), logId, attacker, criticalHit);
	self.incrementAttackedCount();

	if (Ptr<Player> player = runtime::as<Player>(attacker); !self.isDead() && player) {
		if (criticalProcEffect) {
			criticalProcEffect->applyEffect();
		}
		if ((!effect || effect->tryActivateGodstone()) && status != AttackStatus::DODGE && status != AttackStatus::RESIST)
			calculateGodStoneEffects(*player);
	}
	if (effect && type == SM_ATTACK_STATUS_TYPE::DELAYDAMAGE)
		effect->broadcastHate();
}

void CreatureController::calculateGodStoneEffects(model::gameobjects::player::Player& attacker) {
	applyGodStoneEffect(attacker, attacker.getEquipment().getMainHandWeapon(), true);
	applyGodStoneEffect(attacker, attacker.getEquipment().getOffHandWeapon(), false);
}

void CreatureController::applyGodStoneEffect(model::gameobjects::player::Player& attacker, runtime::Ptr<model::gameobjects::Item> weapon,
	bool isMainHandWeapon) {
	if (!weapon || !weapon->hasGodStone())
		return;
	Ptr<model::items::GodStone> godStone = weapon->getGodStone();
	if (!godStone->tryActivate(isMainHandWeapon, getOwner()))
		return;

	const model::templates::item::GodstoneInfo* godstoneInfo = godStone->getGodstoneInfo();
	Ref<Skill> skill = skillengine::SkillEngine::getInstance().getSkill(attacker, godstoneInfo->getSkillId(), godstoneInfo->getSkillLevel(), getOwner(),
		godStone->getItemTemplate());
	skill->setFirstTargetRangeCheck(false);
	if (!skill->canUseSkill(skillengine::properties::Properties_CastState::CAST_START))
		return;
	Ref<Effect> effect = Effect::create(*skill, getOwner());
	effect->initialize();
	effect->applyEffect();
	PacketSendUtility::sendPacket(attacker, SM_SYSTEM_MESSAGE::STR_SKILL_PROC_EFFECT_OCCURRED(skill->getSkillTemplate()->getL10n()));
	// Illusion Godstones
	if (godstoneInfo->getBreakProb() > 0) {
		godStone->increaseActivatedCount();
		if (godStone->getActivatedCount() > godstoneInfo->getNonBreakCount() && commons::utils::Rnd::get(1, 1000) <= godstoneInfo->getBreakProb()) {
			// TODO: Delay 10 Minutes, send messages etc
			// PacketSendUtility.sendPacket(owner, SM_SYSTEM_MESSAGE.STR_MSG_BREAK_PROC_REMAIN_START(equippedItem.getL10n(),
			// itemTemplate.getL10nId()));
			weapon->setGodStone(nullptr);
			PacketSendUtility::sendPacket(attacker, SM_SYSTEM_MESSAGE::STR_MSG_BREAK_PROC(weapon->getL10n(), godStone->getL10n()));
			services::item::ItemPacketService::updateItemAfterInfoChange(attacker, *weapon);
		}
	}
}

void CreatureController::attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) {
	Creature& self = getOwner();
	bool addAttackObservers = true;
	if (!skipChecks && (!target || self.isDead() || self.getLifeStats()->isAboutToDie() || !self.canAttack() || !self.isSpawned())) {
		return;
	}

	// Calculate and apply damage
	model::animations::AttackHandAnimation attackHandAnimation = model::animations::AttackHandAnimation::MAIN_HAND;
	model::animations::AttackTypeAnimation attackTypeAnimation = model::animations::AttackTypeAnimation::MELEE;
	std::vector<Ref<AttackResult>> attackResult;

	std::unordered_set<utils::stats::CalculationType> calculationTypes{utils::stats::CalculationType::APPLY_POWER_SHARD_DAMAGE,
		utils::stats::CalculationType::REMOVE_POWER_SHARD};
	if (Ptr<Player> p = runtime::as<Player>(self); p && p->getEquipment().isDualWeaponEquipped())
		calculationTypes.insert(utils::stats::CalculationType::DUAL_WIELD);
	if (self.getAttackType() == model::templates::item::ItemAttackType::PHYSICAL)
		attackResult = AttackUtil::calculatePhysAttackResult(self, *target, calculationTypes);
	else {
		attackResult =
			standins::attackUtilCalculateMagAttackResult(self, *target, model::templates::item::getMagicalElement(self.getAttackType()), calculationTypes);
		attackHandAnimation = model::animations::AttackHandAnimation::OFF_HAND;
	}
	if (runtime::as<Npc>(self)) {
		attackHandAnimation = self.getAi().modifyAttackHandAnimation(attackHandAnimation);
		attackTypeAnimation = self.getAi().getAttackTypeAnimation(*target);
	}

	int32_t damage = 0;
	for (const Ref<AttackResult>& result : attackResult) {
		if (result->getAttackStatus() == AttackStatus::RESIST || result->getAttackStatus() == AttackStatus::DODGE)
			addAttackObservers = false;
		damage = detail::add(damage, result->getDamage());
	}

	AttackStatus firstAttackStatus = detail::getBaseStatus(detail::listGetFirst(attackResult)->getAttackStatus());
	Ref<Effect> criticalProcEffect;
	if (Ptr<Player> player = runtime::as<Player>(self); player && firstAttackStatus == AttackStatus::CRITICAL && commons::utils::Rnd::chance() < 10) {
		criticalProcEffect = skillengine::SkillEngine::getInstance().createCriticalProcEffect(*player, *target, 0);
		if (criticalProcEffect && (criticalProcEffect->getEffectResult() == skillengine::model::EffectResult::DODGE ||
			criticalProcEffect->getEffectResult() == skillengine::model::EffectResult::RESIST))
			criticalProcEffect = nullptr;
	}
	std::vector<Ptr<AttackResult>> attackList(attackResult.begin(), attackResult.end());
	PacketSendUtility::broadcastPacketAndReceive(self,
		network::aion::serverpackets::SM_ATTACK(self, *target, self.getGameStats()->getAttackCounter(), time, attackTypeAnimation, attackHandAnimation,
			attackList, criticalProcEffect),
		AIEventType::CREATURE_NEEDS_HELP);

	self.getGameStats()->increaseAttackCounter();
	if (addAttackObservers) {
		self.getObserveController()->notifyAttackObservers(*target, 0);
	}

	if (time == 0)
		target->getController().onAttack(self, damage, firstAttackStatus, criticalProcEffect);
	else
		utils::ThreadPoolManager::getInstance().schedule(runtime::bindTask([](DelayedOnAttack& task) { task.run(); },
			DelayedOnAttack::create(*target, self, damage, firstAttackStatus, criticalProcEffect)), time);
}

bool CreatureController::hasTask(model::TaskId taskId) {
	return tasks.containsKey(static_cast<int32_t>(taskId));
}

bool CreatureController::hasScheduledTask(model::TaskId taskId) {
	Ptr<runtime::Future> task = tasks.get(static_cast<int32_t>(taskId));
	return task && !task->isDone();
}

runtime::FutureRef CreatureController::getAndRemoveTask(model::TaskId taskId) {
	return FutureRef(tasks.remove(static_cast<int32_t>(taskId)));
}

runtime::FutureRef CreatureController::cancelTask(model::TaskId taskId) {
	FutureRef task = getAndRemoveTask(taskId);
	if (task) {
		task->cancel(false);
	}
	return task;
}

bool CreatureController::cancelTaskIfPresent(model::TaskId taskId, runtime::FutureRef task) {
	if (task && tasks.remove(static_cast<int32_t>(taskId), task)) {
		task->cancel(false);
		return true;
	}
	return false;
}

void CreatureController::addTask(model::TaskId taskId, runtime::FutureRef task) {
	tasks.compute(static_cast<int32_t>(taskId), [&](const Ptr<runtime::Future>& oldTask) -> FutureRef {
		if (oldTask) {
			oldTask->cancel(false);
			if (taskId == TaskId::DESPAWN) {
				log.warn("Despawn task for " + getOwner().toString() +
					" was cancelled and replaced with another one, possibly delaying the intended despawn time.");
			}
		}
		return task;
	});
}

void CreatureController::cancelAllTasks() {
	for (const auto& e : tasks.entrySet()) {
		Ptr<runtime::Future> task = e.getValue();
		if (task)
			task->cancel(false);
	}
	tasks.clear();
}

void CreatureController::onDelete() {
	cancelAllTasks();
	VisibleObjectController::onDelete();
}

bool CreatureController::die() {
	return die(std::nullopt, std::nullopt, getOwner());
}

bool CreatureController::die(model::gameobjects::Creature& lastAttacker) {
	return die(std::nullopt, std::nullopt, lastAttacker);
}

bool CreatureController::die(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> value, model::gameobjects::Creature& lastAttacker) {
	return getOwner().getLifeStats()->reduceHp(type, std::numeric_limits<int32_t>::max(), 0, value, lastAttacker) == 0;
}

bool CreatureController::useSkill(int32_t skillId) {
	return useSkill(skillId, 1);
}

bool CreatureController::useSkill(int32_t skillId, int32_t skillLevel) {
	try {
		Creature& creature = getOwner();
		Ref<Skill> skill = skillengine::SkillEngine::getInstance().getSkill(creature, skillId, skillLevel, creature.getTarget());
		if (skill) {
			return skill->useSkill();
		}
	} catch (const std::exception& ex) {
		log.error("Exception during skill use: " + std::to_string(skillId), ex);
	}
	return false;
}

bool CreatureController::useChargeSkill(skillengine::model::Skill& startSkill, int64_t chargeTimeMillis) {
	const skillengine::condition::SkillChargeCondition* chargeCondition = startSkill.getSkillTemplate()->getSkillChargeCondition();
	const skillengine::model::ChargeSkillEntry* chargeSkill =
		chargeCondition == nullptr ? nullptr : dataholders::DataManager::SKILL_CHARGE_DATA->getChargedSkillEntry(chargeCondition->getValue());
	if (chargeSkill == nullptr ||
		static_cast<float>(chargeTimeMillis) < static_cast<float>(chargeSkill->getMinTime()) * startSkill.getCastSpeedForAnimationBoostAndChargeSkills()) {
		if (Ptr<Player> player = runtime::as<Player>(getOwner()))
			utils::audit::AuditLogger::log(*player,
				"tried to use charge skill " + std::to_string(startSkill.getSkillId()) + " after " + std::to_string(chargeTimeMillis));
		return false;
	}
	bool result = false;
	try {
		int32_t index = 0, chargeTimeSum = 0;
		const std::vector<skillengine::model::ChargedSkill>& skills = chargeSkill->getSkills();
		for (const skillengine::model::ChargedSkill& skill : skills) {
			chargeTimeSum = detail::add(chargeTimeSum,
				detail::toInt(static_cast<float>(skill.getTime()) * startSkill.getCastSpeedForAnimationBoostAndChargeSkills()));
			if (chargeTimeSum >= chargeTimeMillis || ++index == static_cast<int32_t>(skills.size()) - 1)
				break;
		}
		int32_t skillId = detail::listGet(skills, index).getId();
		// Java: ChargeSkill skill = SkillEngine.getInstance().getChargeSkill(getOwner(), skillId, startSkill.getSkillLevel(), index + 1, startSkill);
		// if (skill != null) return skill.useSkill();
		result = standins::chargeSkillGetAndUse(getOwner(), skillId, startSkill.getSkillLevel(), index + 1, startSkill);
	} catch (const std::exception& ex) {
		log.error("Could not use charge skill " + std::to_string(startSkill.getSkillId()) + " with charge time " + std::to_string(chargeTimeMillis), ex);
	} catch (...) {
		startSkill.cancelCast(); // Java finally before a Throwable that is no Exception propagates
		throw;
	}
	startSkill.cancelCast(); // Java finally
	return result;
}

runtime::Ptr<skillengine::model::Skill> CreatureController::abortCast() {
	Creature& creature = getOwner();
	Ptr<Skill> castingSkill = creature.getCastingSkill();
	if (castingSkill) {
		castingSkill->cancelCast();
		creature.setCasting(nullptr);
	}
	if (Ptr<Npc> npc = runtime::as<Npc>(creature)) {
		creature.getAi().setSubStateIfNot(ai::AISubState::NONE);
		npc->getGameStats()->setLastSkill(nullptr);
	}
	return castingSkill;
}

void CreatureController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker) {
	cancelCurrentSkill(lastAttacker, nullptr);
}

void CreatureController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker,
	network::aion::serverpackets::SM_SYSTEM_MESSAGE* msg) {
	Ptr<Skill> castingSkill = abortCast();
	if (!castingSkill)
		return;

	PacketSendUtility::broadcastPacketAndReceive(getOwner(),
		network::aion::serverpackets::SM_SKILL_CANCEL(getOwner(), castingSkill->getSkillTemplate()->getSkillId()));
	if (Ptr<ai::NpcAI> npcAI = runtime::as<ai::NpcAI>(getOwner().getAi())) {
		npcAI->onGeneralEvent(AIEventType::ATTACK_COMPLETE);
	}
	if (Ptr<Player> player = runtime::as<Player>(lastAttacker)) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED());
	}
}

void CreatureController::onAfterSpawn() {
	VisibleObjectController::onAfterSpawn();
	getOwner().revalidateZones();
	// java-race: check-then-act on the volatile actor; two concurrent onAfterSpawn calls can both create and register an actor, and the second
	// assignment loses the first one (it stays registered as an observer until the observers are cleared)
	if (!actor.get() && getOwner().getMoveController() && world::geo::GeoService::getInstance().worldHasTerrainMaterials(getOwner().getWorldId())) {
		actor = observer::TerrainZoneCollisionMaterialActor::create(getOwner());
		getOwner().getObserveController()->addObserver(*actor);
	}
}

void CreatureController::onDespawn() {
	VisibleObjectController::onDespawn();
	// java-race: read-then-clear of the volatile actor; an actor created by a concurrent onAfterSpawn between the read and `actor = null` is dropped
	// from the field while it stays registered
	if (Ptr<observer::TerrainZoneCollisionMaterialActor> currentActor = actor.get()) {
		currentActor->abort();
		getOwner().getObserveController()->removeObserver(*currentActor);
		actor = nullptr;
	}
	cancelTask(TaskId::DECAY);
	getOwner().getMoveController()->abortMove();
	getOwner().getAggroList().clear();
}

} // namespace aion::gameserver::controllers
