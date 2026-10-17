#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

struct DrakenspireDepthsInstance_Runnable;
struct DrakenspireDepthsInstance_Runnable_2;

/**
 * Drakenspire Depths (301390000) and its difficulty: the twin protectors (5 minutes, 15 s apart), Orissan's immortality phases, the
 * siege weapon's waves and Beritra's form; each failed stage lowers the difficulty (5 hard, 3 normal, 1 easy, 0 failed).
 * <p>
 * Java: data/handlers/instance/DrakenspireDepthsInstance.java, @InstanceID(301390000). Java's AtomicReference<Race> starts null: the port
 * starts it at Race::NONE, which no player has (compareAndSet(null, race) is compareAndSet(NONE, race); a null race reads as "not ELYOS"
 * like NONE). Java's synchronized methods hold the monitor member.
 *
 * @author Estrayl
 */
class DrakenspireDepthsInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
	friend struct DrakenspireDepthsInstance_Runnable;
	friend struct DrakenspireDepthsInstance_Runnable_2;

private:
	static constexpr int32_t HARD_MODE = 5;
	static constexpr int32_t NORMAL_MODE = 3;
	static constexpr int32_t EASY_MODE = 1;
	static constexpr int32_t FAILED = 0;
	runtime::AtomicInteger difficulty{AION_LOCK_CLASS(DrakenspireDepthsInstance::difficulty), HARD_MODE};
	runtime::AtomicBoolean isStageActive{AION_LOCK_CLASS(DrakenspireDepthsInstance::isStageActive)};
	// increases the final boss time by 15s for each survivor
	runtime::AtomicInteger waveSurvivors{AION_LOCK_CLASS(DrakenspireDepthsInstance::waveSurvivors), 8};
	runtime::AtomicInteger eventCounter{AION_LOCK_CLASS(DrakenspireDepthsInstance::eventCounter)};
	runtime::AtomicReference<Race> race{AION_LOCK_CLASS(DrakenspireDepthsInstance::race), Race::NONE}; // Java: null until the first entry
	runtime::Field<runtime::FutureRef> currentEventTask{}; // re-usable for all stages
	runtime::Field<runtime::FutureRef> additionalEventTask{};
	runtime::Monitor monitor{AION_LOCK_CLASS(DrakenspireDepthsInstance::monitor)}; // fieldmap: Java's synchronized methods

protected:
	explicit DrakenspireDepthsInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DrakenspireDepthsInstance> create(WorldMapInstance& instance);

	void onInstanceDestroy() override;

	void onEnterInstance(Player& player) override;

	void onSpawn(VisibleObject& object) override;

	using GeneralInstanceHandler::onDie;

	void onDie(Npc& npc) override;

	void onAggro(Npc& npc) override;

private:
	void onTwinsFightStart();

	void onTwinRespawn();

	void onTwinsComplete();

	void onTwinsFail();

	void scheduleOrissansNextImmortality();

	void onOrissanComplete();

	void onWaveEventStart();

	void scheduleWaveStarts();

	void scheduleCommanderWave(int32_t waveCount);

	void onWaveEventComplete(bool isSuccessful);

	void spawnExit();

public:
	void onCreatureDetected(Npc& detector, Creature& detected) override;

	void onStartEffect(runtime::Ptr<Effect> effect) override;

	void onEndEffect(Effect& effect) override;

	void leaveInstance(Player& player) override;

	bool onReviveEvent(Player& player) override;

private:
	void setCurrentEventTask(runtime::FutureRef task);

	void setAdditionalEventTask(runtime::FutureRef task);

	void cancelTasks(std::initializer_list<runtime::FutureRef> tasks);

	void sp(int32_t id, float x, float y, float z, int8_t h, std::string_view walkerId, int32_t delay);

	void sp(int32_t id, float x, float y, float z, int8_t h, std::string_view walkerId, CreatureState state, int32_t delay);

	void startWalking(Npc& npc, CreatureState state);

public:
	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
