#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * Hub header (docs/design/hub-headers.md). A part of Creature (`PartSlot<AggroList>`, Java `createAggroList()` returns `new AggroList(this)`),
 * bound to its owner in the constructor.
 * <p>
 * C++ notes: the `Stream` returns (stream, streamValidTargets, streamValidTargetInfo) are `std::vector<Ptr<X>>` snapshots (§7.2). The HopType of
 * addDamage is `std::optional`, because CreatureController.onAttack forwards AttackShieldObserver's null HopType.
 *
 * @author ATracer, KKnD
 */
class AggroList : public runtime::OwnedPart {
protected:
	runtime::OwnerRef<model::gameobjects::Creature> owner;

private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<AggroInfo>> aggroList{};
	runtime::Field<runtime::FutureRef> hateReductionTask{};

public:
	explicit AggroList(model::gameobjects::Creature& owner);
	~AggroList() override;

	/**
	 * Only add damage from enemies. (Verify this includes summons, traps, pets, and excludes fall damage.)
	 * <p>
	 * hopType: null when CreatureController.onAttack received null (AttackShieldObserver's reflected damage)
	 */
	void addDamage(model::gameobjects::Creature& attacker, int32_t damage, bool notifyAttack, std::optional<skillengine::model::HopType> hopType);

	/**
	 * Hate that is received without dealing damage
	 */
	void addHate(model::gameobjects::Creature& creature, int32_t hate);

private:
	void addDamageAndHate(model::gameobjects::Creature& creature, int32_t damage, int32_t hate);

	bool shouldAddHateToMaster(model::gameobjects::Creature& creature);

	bool isTauntingSpirit(model::gameobjects::SummonedObject& npc);

public:
	/**
	 * @return player with most damage, if no other creatures like NPCs dealt more damage
	 */
	runtime::Ptr<model::gameobjects::player::Player> getMostPlayerDamage();

	void stopHating(model::gameobjects::VisibleObject& creature);

	/**
	 * Remove creature from aggro list and transfer its damages to the master
	 */
	void remove(model::gameobjects::Creature& creature);

	void remove(model::gameobjects::Creature& creature, bool transferDamagesToMaster);

private:
	void transferDamagesToMaster(AggroInfo& aggroInfo);

public:
	void clear();

	bool isHating(model::gameobjects::Creature& creature);

	int32_t getHate(model::gameobjects::Creature& creature);

	/** Java: Stream<AggroInfo> stream() - a snapshot of the aggro list values (§7.2) */
	std::vector<runtime::Ptr<AggroInfo>> stream();

	/**
	 * @return a living creature with no obstacle between it and the owner
	 */
	runtime::Ptr<model::gameobjects::Creature> getTarget(AggroTarget targetType);

	/**
	 * @return a living creature that is within the given range with no obstacle between it and the owner
	 */
	runtime::Ptr<model::gameobjects::Creature> getTarget(AggroTarget targetType, float range);

	/**
	 * @return living creatures that are within the given range with no obstacle between them and the owner (Java: Stream<Creature>, §7.2)
	 */
	std::vector<runtime::Ptr<model::gameobjects::Creature>> streamValidTargets(float range);

private:
	/** Java: Stream<AggroInfo> (§7.2) */
	std::vector<runtime::Ptr<AggroInfo>> streamValidTargetInfo(float range);

public:
	/**
	 * Damages of summons and summoned objects are added to those of their masters.
	 *
	 * @return list of DamageInfo with npc and player damages
	 */
	DamageList getFinalDamageList();

protected:
	/** creature: null-checked (Java `creature != null`), e.g. the master of addHate */
	virtual bool isAware(runtime::Ptr<model::gameobjects::Creature> creature);

private:
	void startHateReductionTask();
};

} // namespace aion::gameserver::controllers::attack
