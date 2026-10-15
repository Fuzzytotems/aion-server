#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Nightmare Circus (301200000): the entry gives the race's transformation and the friends' greetings; the end of movie 983 starts the circus
 * once - fifteen waves on two sides walking their routes, then Mistress Viloa, Harlequin Lord Reshka and Nightmare Lord Heiramune (the first two
 * leave again after a while); Heiramune's death plays the last movie and opens the cage, the crates and the exit.
 * <p>
 * Java: data/handlers/instance/NightmareCircus.java, @InstanceID(301200000). C++: the scheduled lambdas are pinned to the handler.
 */
class NightmareCircus : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<runtime::Array<runtime::FutureRef>> despawnTasks = runtime::Array<runtime::FutureRef>::make(2); // Java: = new Future<?>[2]
	runtime::AtomicBoolean moviePlayed{AION_LOCK_CLASS(NightmareCircus::moviePlayed)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> isInstanceDestroyed{};

protected:
	explicit NightmareCircus(WorldMapInstance& instance);

public:
	static runtime::Ref<NightmareCircus> create(WorldMapInstance& instance);

	void onEnterInstance(Player& player) override;

private:
	void cancelDespawnBossTask();

public:
	void onPlayMovieEnd(Player& player, int32_t movieId) override;

private:
	void spawnPhase();

protected:
	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t msg, int32_t time);

	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkerId);

	/** Java: Integer msg (null: no message) */
	void sp(int32_t npcId, float x, float y, float z, int8_t h, std::optional<int32_t> msg, int32_t time, std::string_view walkerId);

private:
	void startDespawnBossTask();

public:
	void onDie(Npc& npc) override;

private:
	void sendMsg(int32_t npcId, int32_t msg);

public:
	bool onReviveEvent(Player& player) override;

	void onInstanceDestroy() override;
};

} // namespace aion::gameserver::handlers::instance
