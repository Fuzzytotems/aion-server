#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Illuminary Obelisk (301230000): the door after a minute and the half-hour timer, the generators' charges and their waves, Dynatoum after
 * the four third charges, the wipe at the end of the time, the exit after Dynatoum.
 * <p>
 * Java: data/handlers/instance/IlluminaryObeliskInstance.java, @InstanceID(301230000) (the marker is in the .cpp). C++: the scheduled lambdas
 * are pinned to the handler.
 *
 * @author Estrayl
 */
class IlluminaryObeliskInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isRaceSet{AION_LOCK_CLASS(IlluminaryObeliskInstance::isRaceSet)}; // Java: = new AtomicBoolean()
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(IlluminaryObeliskInstance::tasks)}; // Java: = new ArrayList<>()

public:
	runtime::Field<bool> isInstanceDestroyed{};

protected:
	explicit IlluminaryObeliskInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<IlluminaryObeliskInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

private:
	void scheduleInstanceStart();

protected:
	void scheduleWipe(int32_t delay);

public:
	void onSpawn(VisibleObject& object) override;

protected:
	virtual void scheduleChargeAttacks(int32_t npcId);

private:
	void checkGenerators();

protected:
	virtual void spawnEndboss(int32_t npcId);

	void wipe();

	using GeneralInstanceHandler::spawn;

	void spawn(int32_t npcId, float x, float y, float z, int8_t h, int32_t delay, std::string_view walkerId);

private:
	void cancelTasks();

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onEnterInstance(Player& player) override;

	void onEndEffect(Effect& effect) override;

	void onDie(Npc& npc) override;

	bool onReviveEvent(Player& player) override;

	void leaveInstance(Player& player) override;

	void onInstanceDestroy() override;

	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
