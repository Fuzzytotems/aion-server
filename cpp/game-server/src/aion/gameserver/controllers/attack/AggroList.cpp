#include "aion/gameserver/controllers/attack/AggroList.h"

#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers::attack {

using runtime::Ptr;
using runtime::Ref;

AggroList::AggroList(model::gameobjects::Creature& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

AggroList::~AggroList() = default;

void AggroList::addDamage(model::gameobjects::Creature& attacker, int32_t damage, bool notifyAttack,
	std::optional<skillengine::model::HopType> hopType) {
	AION_UNPORTED();
}

void AggroList::addHate(model::gameobjects::Creature& creature, int32_t hate) {
	AION_UNPORTED();
}

void AggroList::addDamageAndHate(model::gameobjects::Creature& creature, int32_t damage, int32_t hate) {
	AION_UNPORTED();
}

bool AggroList::shouldAddHateToMaster(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool AggroList::isTauntingSpirit(model::gameobjects::SummonedObject& npc) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> AggroList::getMostPlayerDamage() {
	AION_UNPORTED();
}

void AggroList::stopHating(model::gameobjects::VisibleObject& creature) {
	Ptr<AggroInfo> aggroInfo = aggroList.get(creature.getObjectId());
	if (aggroInfo)
		aggroInfo->setHate(0);
}

void AggroList::remove(model::gameobjects::Creature& creature) {
	remove(creature, true);
}

void AggroList::remove(model::gameobjects::Creature& creature, bool transferDamagesToMaster) {
	Ptr<AggroInfo> aggroInfo = aggroList.remove(creature.getObjectId());
	if (transferDamagesToMaster && aggroInfo)
		this->transferDamagesToMaster(*aggroInfo);
}

void AggroList::transferDamagesToMaster(AggroInfo& aggroInfo) {
	Ptr<model::gameobjects::Creature> attacker = aggroInfo.getAttacker();
	Ptr<model::gameobjects::Creature> master = attacker->getMaster();
	if (!master) // Java: master.equals(...) on a null master
		throw runtime::NullPointerException("master is null");
	if (master->equals(*attacker) || !isAware(master))
		return;
	int32_t damage = aggroInfo.getDamage();
	aggroList.compute(master->getObjectId(), [&master, damage](const Ptr<AggroInfo>& existing) -> Ref<AggroInfo> {
		Ref<AggroInfo> masterAggroInfo(existing);
		if (!masterAggroInfo) {
			masterAggroInfo = AggroInfo::create(*master);
			masterAggroInfo->setHate(1);
		}
		masterAggroInfo->addDamage(damage);
		return masterAggroInfo;
	});
}

void AggroList::clear() {
	SYNCHRONIZED(*this) {
		// lockdep: hateReductionTask.get() reads the Field<FutureRef>; cancel(true) does not wait for the task
		if (Ptr<runtime::Future> task = hateReductionTask.get()) {
			task->cancel(true);
			hateReductionTask = nullptr;
		}
	}
	aggroList.clear();
}

bool AggroList::isHating(model::gameobjects::Creature& creature) {
	Ptr<AggroInfo> aggroInfo = aggroList.get(creature.getObjectId());
	return aggroInfo && aggroInfo->getHate() > 0;
}

int32_t AggroList::getHate(model::gameobjects::Creature& creature) {
	Ptr<AggroInfo> aggroInfo = aggroList.get(creature.getObjectId());
	return aggroInfo ? aggroInfo->getHate() : 0;
}

std::vector<runtime::Ptr<AggroInfo>> AggroList::stream() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Creature> AggroList::getTarget(AggroTarget targetType) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Creature> AggroList::getTarget(AggroTarget targetType, float range) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::Creature>> AggroList::streamValidTargets(float range) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<AggroInfo>> AggroList::streamValidTargetInfo(float range) {
	AION_UNPORTED();
}

DamageList AggroList::getFinalDamageList() {
	AION_UNPORTED();
}

bool AggroList::isAware(runtime::Ptr<model::gameobjects::Creature> creature) {
	if (!creature || !owner.getKnownList().knows(*creature) || owner.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::SANCTUARY))
		return false;
	if (aggroList.containsKey(creature->getObjectId()) || creature->isEnemy(owner))
		return true;
	// Java: TRIBE_RELATIONS_DATA.isHostileRelation(owner.getTribe(), creature.getTribe()) - a null tribe is not in the tribe map (false)
	std::optional<model::TribeClass> ownerTribe = owner.getTribe();
	std::optional<model::TribeClass> creatureTribe = creature->getTribe();
	return ownerTribe && creatureTribe && dataholders::DataManager::TRIBE_RELATIONS_DATA->isHostileRelation(*ownerTribe, *creatureTribe);
}

// callbacks: com.aionemu.gameserver.controllers.attack.AggroList@L206:77 (hate reduction task, stored as hateReductionTask)
void AggroList::startHateReductionTask() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
