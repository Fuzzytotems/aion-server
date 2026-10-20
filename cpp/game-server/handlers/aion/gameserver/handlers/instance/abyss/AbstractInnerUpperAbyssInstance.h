#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * A basic handler for all versions of the siege-keep instances in the inner upper abyss. <br>
 * <br>
 * Currently, the chests will spawn after the boss died and will be reduced depending on the past time. Further, it is a security
 * mechanism to
 * avoid abuse due to geodata issues handling teleportation skills. <br>
 * <br>
 * On retail all chests are spawned on instance creation and will not be deleted due to the past time. At least, only the boss will be
 * deleted after ten minutes past. <br>
 * <br>
 * Original credits to keqi, xTz, Luzien & Everlight
 * <p>
 * Java: data/handlers/instance/abyss/AbstractInnerUpperAbyssInstance.java (abstract: not registered). C++: the chest reduction task's
 * lambda
 * is pinned to the handler; chestLocations is a Field. **Correction of the Java code (owner's decision 2026-10-19, both branches; was
 * J-P6I4-1):** spawnChests keeps chestLocations; Java cleared it and set it null, so the second boss death of a keep threw.
 *
 * @author Estrayl
 */
class AbstractInnerUpperAbyssInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	/** All final spawn locations behind the end boss (a safety mechanism: players can jump through doors) */
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<WorldPosition>>>> chestLocations; // Java: = new ArrayList<>()
	/** For final fight trigger */
	runtime::AtomicBoolean isFlyringPassed{AION_LOCK_CLASS(AbstractInnerUpperAbyssInstance::isFlyringPassed)};
	/** The instance starts in hard mode; killing the artifact in front of the end boss switches it to the easier version */
	runtime::AtomicBoolean isEasyMode{AION_LOCK_CLASS(AbstractInnerUpperAbyssInstance::isEasyMode)};
	/** Counts the passed time to reduce the chest spawns (a custom feature) */
	runtime::AtomicInteger failCounter{AION_LOCK_CLASS(AbstractInnerUpperAbyssInstance::failCounter)};
	runtime::Field<runtime::FutureRef> chestReductionTask{};

protected:
	explicit AbstractInnerUpperAbyssInstance(WorldMapInstance& instance);

	/** @return the awakened boss id */
	virtual int32_t getBossId() = 0;

	/** @return the ordinary chest id */
	virtual int32_t getChestId() = 0;

	/** @return the id of the door NPC */
	virtual int32_t getDoorId() = 0;

	/** @return the smallest id of the four key masters */
	virtual int32_t getKeymasterId() = 0;

	/** @return the id of the invisible NPC which is used to trigger the final timer event */
	virtual int32_t getTimerNpcId() = 0;

public:
	void onInstanceCreate() override;

	void onCreatureDetected(Npc& detector, Creature& detected) override;

protected:
	/** Called to delete static door NPCs. Java: final */
	void openDoor(int32_t staticId);

	/** Called after destruction of the artifact to replace the boss with his weaker version. Java: final */
	void switchToEasyMode();

	/** Called after death of the end boss to interrupt the time task and spawn the reward chests depending on the past time. Java: final */
	void spawnChests();

	/** Called after destruction of statues to distribute a certain amount of ap. */
	virtual void rewardStatueKill(int32_t ap);

public:
	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance::abyss
