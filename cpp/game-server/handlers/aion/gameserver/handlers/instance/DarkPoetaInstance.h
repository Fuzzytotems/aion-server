#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Dark Poeta (300040000): the score of kills and gatherings from the start (door 33 or two minutes), the three generators that call
 * Brigade General Anuhart, the rank by points and time with its bonus boss, the exit after a bonus boss.
 * <p>
 * Java: data/handlers/instance/DarkPoetaInstance.java, @InstanceID(300040000) (the marker is in the .cpp). C++: the scheduled lambdas are
 * pinned to the handler.
 *
 * @author Hilgert, xTz, Tiger, Ritsu, Estrayl
 */
class DarkPoetaInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<int32_t> excludedNpcs{AION_LOCK_CLASS(DarkPoetaInstance::excludedNpcs)}; // Java: = new ArrayList<>()
	runtime::AtomicInteger killedGenerators{AION_LOCK_CLASS(DarkPoetaInstance::killedGenerators)}; // Java: = new AtomicInteger()
	runtime::Field<runtime::Ref<DarkPoetaScore>> instanceReward{};
	runtime::Field<runtime::FutureRef> instanceTimer{};
	runtime::Field<int64_t> startTime{};

protected:
	explicit DarkPoetaInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DarkPoetaInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;

private:
	int32_t getTime();

	/** Java's null npc is nullptr */
	void sendPacket(runtime::Ptr<Npc> npc, int32_t points);

	int32_t checkRank(int32_t totalPoints);

	void schedulePortalDespawn(Npc& portal);

	int32_t calculatePointsReward(Npc& npc);

	void onStart(bool manually);

public:
	void onEnterInstance(Player& player) override;

	void onOpenDoor(int32_t doorId) override;

	void onInstanceDestroy() override;

	void onInstanceCreate() override;

	void onGather(Player& player, Gatherable& gatherable) override;

	void leaveInstance(Player& player) override;

	void onEndCastSkill(Skill& skill) override;
};

} // namespace aion::gameserver::handlers::instance
