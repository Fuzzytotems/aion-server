#include "aion/gameserver/controllers/ObserveController.h"

#include <utility>

#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/AttackShieldObserver.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/controllers/observer/ObserverTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::controllers {

using model::gameobjects::Creature;
using model::gameobjects::Item;
using model::gameobjects::player::Player;
using observer::ActionObserver;
using observer::AttackCalcObserver;
using observer::ObserverType;
using runtime::Ptr;
using runtime::Ref;

ObserveController::ObserveController() = default;

ObserveController::~ObserveController() = default;

runtime::Ref<ObserveController> ObserveController::create() {
	return runtime::makeRef<ObserveController>();
}

void ObserveController::attach(observer::ActionObserver& observer) {
	observer.makeOneTimeUse();
	addObserver(observer);
}

void ObserveController::addObserver(observer::ActionObserver& observer) {
	SYNCHRONIZED(observers) {
		observers.add(Ref<ActionObserver>(observer));
	}
}

void ObserveController::addAttackCalcObserver(observer::AttackCalcObserver& observer) {
	attackCalcObservers.add(Ref<AttackCalcObserver>(observer));
}

void ObserveController::removeObserver(observer::ActionObserver& observer) {
	bool removed = false;
	SYNCHRONIZED(observers) {
		removed = observers.remove(Ptr<ActionObserver>(observer));
	}
	if (removed)
		observer.onRemoved();
}

void ObserveController::removeAttackCalcObserver(observer::AttackCalcObserver& observer) {
	attackCalcObservers.remove(Ptr<AttackCalcObserver>(observer));
}

void ObserveController::notifyObservers(observer::ObserverType type, std::initializer_list<std::any> object) {
	std::vector<Ptr<ActionObserver>> notifiable; // Java: Collections.emptyList(), replaced by a new list on the first match
	SYNCHRONIZED(observers) {
		if (observers.isEmpty())
			return;
		for (auto iterator = observers.iterator(); iterator.hasNext();) {
			Ptr<ActionObserver> observer = iterator.next();
			if (observer::matchesObserver(observer->getObserverType(), type)) {
				notifiable.push_back(observer);
				if (observer->isOneTimeUse())
					iterator.remove();
			}
		}
	}

	// notify outside of lock
	std::span<const std::any> arguments(object.begin(), object.size());
	for (const Ptr<ActionObserver>& observer : notifiable) {
		notifyAction(type, *observer, arguments);
		if (observer->isOneTimeUse())
			observer->onRemoved();
	}
}

void ObserveController::notifyAction(observer::ObserverType type, observer::ActionObserver& observer, std::span<const std::any> object) {
	switch (type) {
		case ObserverType::ATTACK:
			observer.attack(*std::any_cast<const Ref<Creature>&>(object[0]), std::any_cast<int32_t>(object[1]));
			break;
		case ObserverType::ATTACKED:
			observer.attacked(*std::any_cast<const Ref<Creature>&>(object[0]), std::any_cast<int32_t>(object[1]));
			break;
		case ObserverType::DEATH:
			observer.died(*std::any_cast<const Ref<Creature>&>(object[0]));
			break;
		case ObserverType::EQUIP:
			observer.equip(*std::any_cast<const Ref<Item>&>(object[0]), *std::any_cast<const Ref<Player>&>(object[1]));
			break;
		case ObserverType::UNEQUIP:
			observer.unequip(*std::any_cast<const Ref<Item>&>(object[0]), *std::any_cast<const Ref<Player>&>(object[1]));
			break;
		case ObserverType::MOVE:
			observer.moved();
			break;
		case ObserverType::STARTSKILLCAST:
			observer.startSkillCast(*std::any_cast<const Ref<skillengine::model::Skill>&>(object[0]));
			break;
		case ObserverType::ENDSKILLCAST:
			observer.endSkillCast(*std::any_cast<const Ref<skillengine::model::Skill>&>(object[0]));
			break;
		case ObserverType::BOOSTSKILLCOST:
			observer.boostSkillCost(*std::any_cast<const Ref<skillengine::model::Skill>&>(object[0]));
			break;
		case ObserverType::DOT_ATTACKED:
			observer.dotattacked(*std::any_cast<const Ref<Creature>&>(object[0]), *std::any_cast<const Ref<skillengine::model::Effect>&>(object[1]));
			break;
		case ObserverType::ITEMUSE:
			observer.itemused(*std::any_cast<const Ref<Item>&>(object[0]));
			break;
		case ObserverType::ABNORMALSETTED:
			observer.abnormalsetted(std::any_cast<skillengine::effect::AbnormalState>(object[0]));
			break;
		case ObserverType::SUMMONRELEASE:
			observer.summonrelease();
			break;
		case ObserverType::SIT:
			observer.sit();
			break;
		case ObserverType::HP_CHANGED:
			observer.hpChanged(std::any_cast<int32_t>(object[0]));
			break;
		default:
			break;
	}
}

void ObserveController::notifyDeathObservers(model::gameobjects::Creature& lastAttacker) {
	notifyObservers(ObserverType::DEATH, {Ref<Creature>(lastAttacker)});
}

void ObserveController::notifyMoveObservers() {
	notifyObservers(ObserverType::MOVE, {});
}

void ObserveController::notifySitObservers() {
	notifyObservers(ObserverType::SIT, {});
}

void ObserveController::notifyAttackObservers(model::gameobjects::Creature& creature, int32_t skillId) {
	notifyObservers(ObserverType::ATTACK, {Ref<Creature>(creature), skillId});
}

void ObserveController::notifyAttackedObservers(model::gameobjects::Creature& creature, int32_t skillId) {
	notifyObservers(ObserverType::ATTACKED, {Ref<Creature>(creature), skillId});
}

void ObserveController::notifyDotAttackedObservers(model::gameobjects::Creature& creature, skillengine::model::Effect& effect) {
	notifyObservers(ObserverType::DOT_ATTACKED, {Ref<Creature>(creature), Ref<skillengine::model::Effect>(effect)});
}

void ObserveController::notifyStartSkillCastObservers(skillengine::model::Skill& skill) {
	notifyObservers(ObserverType::STARTSKILLCAST, {Ref<skillengine::model::Skill>(skill)});
}

void ObserveController::notifyEndSkillCastObservers(skillengine::model::Skill& skill) {
	notifyObservers(ObserverType::ENDSKILLCAST, {Ref<skillengine::model::Skill>(skill)});
}

void ObserveController::notifyBoostSkillCostObservers(skillengine::model::Skill& skill) {
	notifyObservers(ObserverType::BOOSTSKILLCOST, {Ref<skillengine::model::Skill>(skill)});
}

void ObserveController::notifyItemEquip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {
	notifyObservers(ObserverType::EQUIP, {Ref<Item>(item), Ref<Player>(owner)});
}

void ObserveController::notifyItemUnEquip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) {
	notifyObservers(ObserverType::UNEQUIP, {Ref<Item>(item), Ref<Player>(owner)});
}

void ObserveController::abortItemUseObservers() {
	std::vector<Ptr<observer::ItemUseObserver>> itemUseObservers; // Java: Collections.emptyList(), replaced on the first match
	SYNCHRONIZED(observers) {
		for (auto iterator = observers.iterator(); iterator.hasNext();) {
			if (Ptr<observer::ItemUseObserver> itemUseObserver = runtime::as<observer::ItemUseObserver>(iterator.next())) {
				itemUseObservers.push_back(itemUseObserver);
				iterator.remove();
			}
		}
	}

	for (const Ptr<observer::ItemUseObserver>& itemUseObserver : itemUseObservers) {
		itemUseObserver->onRemoved();
		itemUseObserver->abort();
	}
}

void ObserveController::notifyItemuseObservers(model::gameobjects::Item& item) {
	notifyObservers(ObserverType::ITEMUSE, {Ref<Item>(item)});
}

void ObserveController::notifyAbnormalSettedObservers(skillengine::effect::AbnormalState state) {
	notifyObservers(ObserverType::ABNORMALSETTED, {state});
}

void ObserveController::notifySummonReleaseObservers() {
	notifyObservers(ObserverType::SUMMONRELEASE, {});
}

void ObserveController::notifyHPChangeObservers(int32_t hpValue) {
	notifyObservers(ObserverType::HP_CHANGED, {hpValue});
}

bool ObserveController::checkAttackStatus(attack::AttackStatus status) {
	if (attackCalcObservers.size() > 0) {
		for (Ptr<AttackCalcObserver> observer : attackCalcObservers) {
			if (observer->checkStatus(status)) {
				return true;
			}
		}
	}
	return false;
}

bool ObserveController::checkAttackerStatus(attack::AttackStatus status) {
	if (attackCalcObservers.size() > 0) {
		for (Ptr<AttackCalcObserver> observer : attackCalcObservers) {
			if (observer->checkAttackerStatus(status)) {
				return true;
			}
		}
	}
	return false;
}

runtime::Ref<observer::AttackerCriticalStatus> ObserveController::checkAttackerCriticalStatus(attack::AttackStatus status, bool isSkill) {
	if (attackCalcObservers.size() > 0) {
		for (Ptr<AttackCalcObserver> observer : attackCalcObservers) {
			Ref<observer::AttackerCriticalStatus> acStatus = observer->checkAttackerCriticalStatus(status, isSkill);
			if (acStatus->isResult()) {
				return acStatus;
			}
		}
	}
	return observer::AttackerCriticalStatus::create(false);
}

void ObserveController::checkShieldStatus(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList,
	runtime::Ptr<skillengine::model::Effect> effect, model::gameobjects::Creature& attacker) {
	checkShieldStatus(attackList, effect, attacker, std::nullopt);
}

void ObserveController::checkShieldStatus(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList,
	runtime::Ptr<skillengine::model::Effect> effect, model::gameobjects::Creature& attacker,
	std::optional<skillengine::model::ShieldType> shieldType) {
	if (attackCalcObservers.size() > 0) {
		for (Ptr<AttackCalcObserver> observer : attackCalcObservers) {
			Ptr<observer::AttackShieldObserver> shieldObserver;
			if (!shieldType || ((shieldObserver = runtime::as<observer::AttackShieldObserver>(observer)) && shieldObserver->getShieldType() == *shieldType))
				observer->checkShield(attackList, effect, attacker);
		}
	}
}

float ObserveController::getBasePhysicalDamageMultiplier(bool isSkill) {
	float multiplier = 1;
	if (attackCalcObservers.size() > 0) {
		for (Ptr<AttackCalcObserver> observer : attackCalcObservers) {
			multiplier *= observer->getBasePhysicalDamageMultiplier(isSkill);
		}
	}
	return multiplier;
}

float ObserveController::getBaseMagicalDamageMultiplier() {
	float multiplier = 1;
	if (attackCalcObservers.size() > 0) {
		for (Ptr<AttackCalcObserver> observer : attackCalcObservers) {
			multiplier *= observer->getBaseMagicalDamageMultiplier();
		}
	}
	return multiplier;
}

void ObserveController::clear() {
	std::vector<Ptr<ActionObserver>> removed;
	SYNCHRONIZED(observers) {
		removed = observers.snapshot();
		observers.clear();
	}
	for (const Ptr<ActionObserver>& observer : removed)
		observer->onRemoved();
	attackCalcObservers.clear();
}

void ObserveController::clearWithoutNotify() {
	observers.clear(); // the shim's clear takes the list Monitor, the same lock Java's synchronized (observers) blocks take
	attackCalcObservers.clear();
}

bool ObserveController::hasObservers() {
	return !observers.isEmpty() || !attackCalcObservers.isEmpty();
}

} // namespace aion::gameserver::controllers
