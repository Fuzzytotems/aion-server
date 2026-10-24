#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Eternal Bastion (300540000): defend the fortress commander while killing the camp commanders. The first three phases progress by killing
 * three specific commanders, the fourth when all five siege towers are killed. A barricade, a dredgion signal tower or the siege cannon bring
 * more assault pods. Every minute an assault wave comes, by the assault power the commanders' deaths reduce; pods, towers and a broken wall
 * or gate add waves. The run ends with Pashid's or the fortress commander's death, or after 30 minutes; the points give the rank.
 * <p>
 * Java: data/handlers/instance/EternalBastionInstance.java, @InstanceID(300540000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author Cheatkiller, Estrayl
 */
class EternalBastionInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
	/** C++: the unit tests read the score through it (Java exposes no getInstanceScore here); defined by tests/handlers_inst_3 only */
	friend struct EternalBastionInstanceTestAccess;

private:
	static constexpr int32_t START_DELAY = 180 * 1000;
	runtime::AtomicInteger assaultPower{AION_LOCK_CLASS(EternalBastionInstance::assaultPower), 12}; // Retail
	runtime::AtomicInteger progressionKills{AION_LOCK_CLASS(EternalBastionInstance::progressionKills)};
	runtime::AtomicBoolean isRaceSet{AION_LOCK_CLASS(EternalBastionInstance::isRaceSet)};
	runtime::ArrayList<runtime::FutureRef> spawnTasks{AION_LOCK_CLASS(EternalBastionInstance::spawnTasks)}; // Java: = new ArrayList<>()
	runtime::Field<runtime::FutureRef> instanceTimerTask{};
	runtime::Field<runtime::FutureRef> assaultWaveTask{};
	runtime::Field<int32_t> waveCount{};
	runtime::Field<int64_t> startTime{};
	runtime::Field<runtime::Ref<NormalScore>> instanceReward{};

protected:
	explicit EternalBastionInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<EternalBastionInstance> create(WorldMapInstance& instance);

	using GeneralInstanceHandler::onDie;

	void onDie(Npc& npc) override;

private:
	void checkProgress(int32_t kills);

public:
	void onEnterInstance(Player& player) override;

private:
	void spawnRaceGuards(Race race);

public:
	void onInstanceCreate() override;

private:
	void onStart();

	void spawnAssaultWave();

	void spawnEasternWaveOne();

	void spawnEasternWaveTwo();

	void spawnEasternWaveThree();

	void spawnNorthernWaveOne();

	void spawnNorthernWaveTwo();

	void spawnWesternWave();

	void spawnSouthernWave();

	void spawnCanalWave();

	void spawnAssaultPodWave();

	void spawnSiegeTowerWave();

	void onTimeOut();

	void endInstance();

	int32_t getFinalRank();

	void spawnFinalChest(int32_t rank);

	void distributeRewards(Player& player);

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onEndEffect(Effect& effect) override;

	void onInstanceDestroy() override;

	bool onReviveEvent(Player& player) override;

private:
	void cancelTasks();

	void spawnWithDelay(int32_t npcId, float x, float y, float z, int8_t h, int32_t delay);

	void spawnWithWalker(int32_t npcId, float x, float y, float z, int8_t h, std::string_view walker);

	void addPoints(Npc& npc, int32_t points);

	int32_t getTime();

	void sendPacket();

public:
	void leaveInstance(Player& player) override;

private:
	std::string getRankNameById(int32_t rank);

public:
	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
