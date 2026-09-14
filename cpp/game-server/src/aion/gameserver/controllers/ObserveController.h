#pragma once

#include <any>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers {

/**
 * Hub header (docs/design/hub-headers.md). A RefCounted member of Creature (`const Ref<ObserveController>`), created with create().
 * notifyObservers(type, {args...}) carries its `Object...` payload as std::any (§7.4): Creature, Item, Player, Skill and Effect arguments are
 * stored as `runtime::Ref<C>` of exactly those classes, skill ids and HP values as `int32_t`, AbnormalState as the enum; notifyAction casts
 * them back the way ObserveController.java does (ATTACK/ATTACKED: Ref<Creature>, int32_t; DEATH: Ref<Creature>; EQUIP/UNEQUIP: Ref<Item>,
 * Ref<Player>; STARTSKILLCAST/ENDSKILLCAST/BOOSTSKILLCOST: Ref<Skill>; DOT_ATTACKED: Ref<Creature>, Ref<Effect>; ITEMUSE: Ref<Item>;
 * ABNORMALSETTED: AbnormalState; HP_CHANGED: int32_t).
 *
 * @author ATracer, Cura
 */
class ObserveController : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<observer::ActionObserver>> observers{AION_LOCK_CLASS(ObserveController::observers)};
	runtime::CopyOnWriteArrayList<runtime::Ref<observer::AttackCalcObserver>> attackCalcObservers{
		AION_LOCK_CLASS(ObserveController::attackCalcObservers)};

protected:
	ObserveController();
	~ObserveController() override;

public:
	/** Java: new ObserveController() */
	static runtime::Ref<ObserveController> create();

	/**
	 * Adds the observer for a single notification. It will be automatically removed from this controller after receiving the notification.
	 */
	void attach(observer::ActionObserver& observer);

	void addObserver(observer::ActionObserver& observer);

	void addAttackCalcObserver(observer::AttackCalcObserver& observer);

	void removeObserver(observer::ActionObserver& observer);

	void removeAttackCalcObserver(observer::AttackCalcObserver& observer);

	/** Java `Object... object`: the payload kinds are listed in the class comment. */
	void notifyObservers(observer::ObserverType type, std::initializer_list<std::any> object);

private:
	/** Java `Object... object` handed on as the array of notifyObservers */
	void notifyAction(observer::ObserverType type, observer::ActionObserver& observer, std::span<const std::any> object);

public:
	void notifyDeathObservers(model::gameobjects::Creature& lastAttacker);

	void notifyMoveObservers();

	void notifySitObservers();

	void notifyAttackObservers(model::gameobjects::Creature& creature, int32_t skillId);

	void notifyAttackedObservers(model::gameobjects::Creature& creature, int32_t skillId);

	void notifyDotAttackedObservers(model::gameobjects::Creature& creature, skillengine::model::Effect& effect);

	void notifyStartSkillCastObservers(skillengine::model::Skill& skill);

	void notifyEndSkillCastObservers(skillengine::model::Skill& skill);

	void notifyBoostSkillCostObservers(skillengine::model::Skill& skill);

	void notifyItemEquip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner);

	void notifyItemUnEquip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner);

	/**
	 * Aborts every attached {@link ItemUseObserver}, so each of them cancels its own item use and tells the player about it.
	 */
	void abortItemUseObservers();

	void notifyItemuseObservers(model::gameobjects::Item& item);

	void notifyAbnormalSettedObservers(skillengine::effect::AbnormalState state);

	void notifySummonReleaseObservers();

	void notifyHPChangeObservers(int32_t hpValue);

	bool checkAttackStatus(attack::AttackStatus status);

	bool checkAttackerStatus(attack::AttackStatus status);

	/** @return the first observer's matching status, or a new AttackerCriticalStatus(false) */
	runtime::Ref<observer::AttackerCriticalStatus> checkAttackerCriticalStatus(attack::AttackStatus status, bool isSkill);

	/** effect: null for auto attacks (AttackUtil) */
	void checkShieldStatus(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList, runtime::Ptr<skillengine::model::Effect> effect,
		model::gameobjects::Creature& attacker);

	/** effect: null for auto attacks; shieldType: null matches every AttackCalcObserver (checkShieldStatus without a shield type) */
	void checkShieldStatus(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList, runtime::Ptr<skillengine::model::Effect> effect,
		model::gameobjects::Creature& attacker, std::optional<skillengine::model::ShieldType> shieldType);

	float getBasePhysicalDamageMultiplier(bool isSkill);

	float getBaseMagicalDamageMultiplier();

	void clear();

	/**
	 * C++ only (LogoutBreakers L7/D2, cycles.toml ObserveController.observers/attackCalcObservers, zombie-safe): removes every observer and
	 * attack-calc observer without calling ActionObserver::onRemoved, so the observers' captures of other objects are released. Idempotent.
	 * Not noexcept: taking the monitor may throw (lock order); LogoutBreakers logs a throwing step.
	 */
	void clearWithoutNotify();
};

} // namespace aion::gameserver::controllers
