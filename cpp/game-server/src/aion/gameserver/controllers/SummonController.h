#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/summons/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The controller part of Summon (late-bound by setOwner). Binds CreatureController's
 * type variable to Summon: getOwner() returns `Summon&` (§8.2). The onAttack and useSkill overloads it does not override stay callable through
 * `SummonController&` by using-declarations (as in NpcController).
 *
 * @author ATracer, RotO (Attack-speed hack protection), Sippolo
 */
class SummonController : public CreatureController {
private:
	runtime::Field<int64_t> lastAttackMillis{0};

public:
	SummonController();
	~SummonController() override;

	/** Narrowing accessor (Java: CreatureController<Summon>.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::Summon& getOwner() const;

	using CreatureController::onAttack;
	using CreatureController::useSkill;

	void notKnow(model::gameobjects::VisibleObject& object) override;

	/**
	 * Release summon
	 */
	virtual void release(model::summons::UnsummonType unsummonType);

	/**
	 * Change to rest mode
	 */
	virtual void restMode();

	virtual void setUnkMode();

	/**
	 * Change to guard mode
	 */
	virtual void guardMode();

	/**
	 * Change to attackMode
	 */
	virtual void attackMode(int32_t targetObjId);

	bool canAttack(int32_t targetObjId);

	void attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) override;

	void onAttack(model::gameobjects::Creature& creature, runtime::Ptr<skillengine::model::Effect> effect,
		network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log, std::optional<attack::AttackStatus> attackStatus,
		std::optional<skillengine::model::HopType> hopType) override;

	void onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
		runtime::Ptr<model::gameobjects::VisibleObject> newTarget) override;

	void onDespawn() override;

	void onDie(model::gameobjects::Creature& lastAttacker) override;

	void useSkill(model::summons::SkillOrder& order);

	void onStartMove() override;

	void onStopMove() override;

protected:
	/** Java: Player getMaster() (the summon's master, null checked by SiegeWeaponController.isValidTarget) */
	runtime::Ptr<model::gameobjects::player::Player> getMaster();
};

} // namespace aion::gameserver::controllers
