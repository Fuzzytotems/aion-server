#include "aion/gameserver/controllers/attack/AggroList.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers::attack {

using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java: stream.map(AggroInfo::getAttacker) */
std::vector<Ptr<model::gameobjects::Creature>> attackersOf(const std::vector<Ptr<AggroInfo>>& infos) {
	std::vector<Ptr<model::gameobjects::Creature>> attackers;
	attackers.reserve(infos.size());
	for (const Ptr<AggroInfo>& info : infos)
		attackers.push_back(info->getAttacker());
	return attackers;
}

/**
 * Java MOST_HATED / SECOND_MOST_HATED / THIRD_MOST_HATED: for n == 1 `stream.max(comparingInt(AggroInfo::getHate))`, otherwise
 * `stream.sorted(comparingInt(AggroInfo::getHate).reversed()).limit(n).reduce((_, b) -> b).map(AggroInfo::getAttacker).orElse(null)`. `max`
 * (BinaryOperator.maxBy) keeps the earlier of two equal elements, so it answers the first maximum in encounter order, and `sorted` is stable,
 * so equal hates keep their encounter order; the encounter order of a ConcurrentHashMap's values is unspecified in Java as in C++.
 * <p>
 * C++: each entry's hate is read once into a snapshot, and the ranking reads only the snapshot (docs/deviations/P5-01.md, audit M-1). Java's
 * comparator re-reads the live hate at every comparison while packet threads, AggroNotifier and the hate reduction task change it; TimSort
 * survives that (a wrong order, at worst an IllegalArgumentException), but a C++ sort whose comparator answers inconsistently is undefined
 * behaviour, and MSVC's unguarded insertion sort then walks before begin(). For a list nobody changes during the call the result is Java's.
 */
Ptr<model::gameobjects::Creature> nthMostHated(const std::vector<Ptr<AggroInfo>>& infos, size_t n) {
	if (infos.empty())
		return {};
	std::vector<std::pair<Ptr<AggroInfo>, int32_t>> snapshot; // (entry, its hate read once), in encounter order
	snapshot.reserve(infos.size());
	for (const Ptr<AggroInfo>& info : infos)
		snapshot.emplace_back(info, info->getHate());
	if (n == 1) {
		// Java: stream.max(comparingInt(AggroInfo::getHate)) - reduce((a, b) -> compare(a, b) >= 0 ? a : b): a later element replaces the
		// candidate only with a strictly greater hate
		size_t most = 0;
		for (size_t i = 1; i < snapshot.size(); ++i) {
			if (snapshot[i].second > snapshot[most].second)
				most = i;
		}
		return snapshot[most].first->getAttacker();
	}
	// Java: comparingInt(AggroInfo::getHate).reversed() - a before b iff Integer.compare(b.hate, a.hate) < 0, i.e. a.hate > b.hate; stable
	std::stable_sort(snapshot.begin(), snapshot.end(),
		[](const std::pair<Ptr<AggroInfo>, int32_t>& a, const std::pair<Ptr<AggroInfo>, int32_t>& b) { return a.second > b.second; });
	// Java: limit(n).reduce((_, b) -> b) - the last of the first n elements, so a shorter list yields its last element
	size_t index = std::min(n, snapshot.size()) - 1;
	return snapshot[index].first->getAttacker();
}

} // namespace

AggroList::AggroList(model::gameobjects::Creature& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

AggroList::~AggroList() = default;

void AggroList::addDamage(model::gameobjects::Creature& attacker, int32_t damage, bool notifyAttack,
	std::optional<skillengine::model::HopType> hopType) {
	if (!isAware(Ptr<model::gameobjects::Creature>(attacker)))
		return;
	// If the incoming damage is higher than the rest life it will decreased to the rest life
	if (damage >= owner.getLifeStats()->getCurrentHp()) {
		damage = owner.getLifeStats()->getCurrentHp();
		// java-race: hateReductionTask is read here without the monitor clear() writes it under (AggroList.java:43 vs :129-137)
	} else if (!hateReductionTask.get()) {
		startHateReductionTask();
	}
	int32_t hate = 0;
	if (notifyAttack && hopType == skillengine::model::HopType::DAMAGE && damage > 0) {
		// damage caused by auto attacks and skills with HopType.DAMAGE is multiplied by 10 and added as hate on retail
		hate = utils::stats::StatFunctions::calculateHate(attacker, mulInt(damage, 10));
	}
	addDamageAndHate(attacker, damage, hate);
}

void AggroList::addHate(model::gameobjects::Creature& creature, int32_t hate) {
	Ptr<model::gameobjects::Creature> target(creature);
	if (shouldAddHateToMaster(*target))
		target = target->getMaster(); // Java reassigns the parameter: creature = creature.getMaster()
	if (!isAware(target))
		return;
	if (hate < 0 && !aggroList.containsKey(target->getObjectId()))
		return;
	addDamageAndHate(*target, 0, hate);
}

void AggroList::addDamageAndHate(model::gameobjects::Creature& creature, int32_t damage, int32_t hate) {
	Ptr<AggroInfo> ai = aggroList.computeIfAbsent(creature.getObjectId(), [&creature] { return AggroInfo::create(creature); });
	// java-race: the hate is read after computeIfAbsent and before addDamage/addHate, so isNewInAggroList can be wrong under concurrent
	// attackers and onAddHate then fires the AI event twice (AggroList.java:68-72)
	bool isNewInAggroList = ai->getHate() == 0;
	ai->addDamage(damage);
	ai->addHate(hate);
	owner.getController().onAddHate(creature, isNewInAggroList);
}

bool AggroList::shouldAddHateToMaster(model::gameobjects::Creature& creature) {
	// ice sheet, threatening wave, etc. generate hate for their master. taunting spirit does not!
	Ptr<model::gameobjects::SummonedObject> summonedObject = runtime::as<model::gameobjects::SummonedObject>(creature);
	return summonedObject && !isTauntingSpirit(*summonedObject);
}

bool AggroList::isTauntingSpirit(model::gameobjects::SummonedObject& npc) {
	switch (npc.getNpcId()) {
		case 833403:
		case 833404:
		case 833478:
		case 833479:
		case 833480:
		case 833481:
			return true; // spawned by Summon Vexing Energy
		default:
			return false;
	}
}

runtime::Ptr<model::gameobjects::player::Player> AggroList::getMostPlayerDamage() {
	// Use final damage list to get pet damage as well.
	std::optional<DamageInfo> mostDamage = getFinalDamageList().getMostDamage();
	if (!mostDamage)
		return {};
	return runtime::as<model::gameobjects::player::Player>(mostDamage->getAttacker());
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
	return aggroList.values().toVector();
}

runtime::Ptr<model::gameobjects::Creature> AggroList::getTarget(AggroTarget targetType) {
	return getTarget(targetType, static_cast<float>(std::numeric_limits<int32_t>::max()));
}

runtime::Ptr<model::gameobjects::Creature> AggroList::getTarget(AggroTarget targetType, float range) {
	std::vector<Ptr<AggroInfo>> infos = streamValidTargetInfo(range);
	switch (targetType) {
		case AggroTarget::RANDOM: {
			std::vector<Ptr<model::gameobjects::Creature>> attackers = attackersOf(infos);
			// Java: Rnd.get(List) - null for an empty list
			Ptr<model::gameobjects::Creature>* chosen = commons::utils::Rnd::get(attackers);
			return chosen ? *chosen : Ptr<model::gameobjects::Creature>();
		}
		case AggroTarget::RANDOM_EXCEPT_CURRENT_TARGET: {
			std::vector<Ptr<model::gameobjects::Creature>> attackers;
			Ptr<model::gameobjects::VisibleObject> currentTarget = owner.getTarget();
			for (const Ptr<model::gameobjects::Creature>& attacker : attackersOf(infos)) {
				// Java: !c.equals(owner.getTarget()) - AionObject.equals is identity (the target is re-read on every element)
				if (!(attacker.get() == owner.getTarget().get()))
					attackers.push_back(attacker);
			}
			Ptr<model::gameobjects::Creature>* chosen = commons::utils::Rnd::get(attackers);
			return chosen ? *chosen : Ptr<model::gameobjects::Creature>();
		}
		case AggroTarget::MOST_HATED:
			return nthMostHated(infos, 1);
		case AggroTarget::SECOND_MOST_HATED:
			return nthMostHated(infos, 2);
		case AggroTarget::THIRD_MOST_HATED:
			return nthMostHated(infos, 3);
	}
	// Java's switch expression is exhaustive over the enum; an unknown constant cannot occur
	throw runtime::IllegalArgumentException("Unknown AggroTarget " + std::to_string(static_cast<int32_t>(targetType)));
}

std::vector<runtime::Ptr<model::gameobjects::Creature>> AggroList::streamValidTargets(float range) {
	return attackersOf(streamValidTargetInfo(range));
}

std::vector<runtime::Ptr<AggroInfo>> AggroList::streamValidTargetInfo(float range) {
	std::vector<Ptr<AggroInfo>> valid;
	for (const Ptr<AggroInfo>& ai : stream()) {
		Ptr<model::gameobjects::Creature> attacker = ai->getAttacker();
		if (ai->getHate() > 0 && !attacker->isDead() && !attacker->getLifeStats()->isAboutToDie() && owner.getKnownList().sees(*attacker)
			&& (range == static_cast<float>(std::numeric_limits<int32_t>::max())
				|| utils::PositionUtil::isInRange(owner, *attacker, range, false))
			&& world::geo::GeoService::getInstance().canSee(owner, *attacker))
			valid.push_back(ai);
	}
	return valid;
}

DamageList AggroList::getFinalDamageList() {
	return DamageList(aggroList.values().toVector(), owner);
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
	SYNCHRONIZED(*this) {
		// Java assigns the scheduleAtFixedRate result inside the synchronized block too (AggroList.java:204-214)
		// lockdep: hateReductionTask.get() reads the Field<FutureRef>, it does not wait for the task
		if (!hateReductionTask.get()) {
			// the captured Ref<AggroList> is the `this` of the Java lambda (fieldmap: `const Ref<AggroList>`); it pins the aggro list and, through
			// its owner reference, the creature, until clear() cancels the task (cycles.toml AggroList@L206:77#this)
			hateReductionTask = utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin(),
				[self = Ref<AggroList>(*this)] {
					for (const Ptr<AggroInfo>& info : self->aggroList.values()) {
						if (info->getLastInteractionTime() != 0 && commons::utils::currentTimeMillis() - info->getLastInteractionTime() > 5000) {
							info->reduceHate();
						}
					}
				},
				10000, 10000); // every 10 sec reduce hate of not attacking creatures
		}
	}
}

} // namespace aion::gameserver::controllers::attack
