#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::controllers {

/**
 * This class is for controlling Npc's
 * <p>
 * Hub header (docs/design/hub-headers.md). Binds CreatureController's type variable to Npc: getOwner() returns `Npc&` (§8.2). The overloads
 * of onAttack and useSkill that NpcController does not override stay callable through `NpcController&` by using-declarations (C++ hides
 * base overloads of an overridden name, Java does not).
 *
 * @author -Nemesiss-, ATracer (2009-09-29), Sarynth, Wakizashi
 */
class NpcController : public CreatureController {
public:
	NpcController();
	~NpcController() override;

	/** Narrowing accessor (Java: CreatureController<Npc>.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::Npc& getOwner() const;

	using CreatureController::onAttack;
	using CreatureController::useSkill;

	void see(model::gameobjects::VisibleObject& object) override;

	void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) override;

	void onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
		runtime::Ptr<model::gameobjects::VisibleObject> newTarget) override;

	void onBeforeSpawn() override;

	void onAfterSpawn() override;

	void onDespawn() override;

	void onDie(model::gameobjects::Creature& lastAttacker) override;

private:
	void petLoot(model::gameobjects::Npc& owner);

	/** @return the pet of the only allowed looter if it auto-loots, otherwise null */
	runtime::Ptr<model::gameobjects::Pet> findPetForLooting(model::gameobjects::Npc& npc);

public:
	void doReward() override;

	void onDialogRequest(model::gameobjects::player::Player& player) override;

	void onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
		int32_t extendedRewardIndex) override;

	void onAddHate(model::gameobjects::Creature& attacker, bool isNewInAggroList) override;

	void onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
		network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> attackStatus,
		std::optional<skillengine::model::HopType> hopType) override;

	void onStartMove() override;

	void onStopMove() override;

	void onEnterZone(world::zone::ZoneInstance& zoneInstance) override;

	bool useSkill(int32_t skillId, int32_t skillLevel) override;

	void loseAggro(bool restoreHp);
};

} // namespace aion::gameserver::controllers
