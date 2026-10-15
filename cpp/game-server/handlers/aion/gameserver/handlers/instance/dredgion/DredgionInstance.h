#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance::dredgion {

/**
 * The base of the three Dredgions (Baranath, Chantra, Terath): the PvP score of two races of six, the 14 rooms the Surkanas guard, the
 * start
 * after two minutes and the end after 42, the rewards.
 * <p>
 * Java: data/handlers/instance/dredgion/DredgionInstance.java (no @InstanceID: not registered). C++: the scheduled lambdas are pinned to
 * the
 * handler.
 *
 * @author xTz
 */
class DredgionInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	runtime::ArrayList<runtime::Ref<DredgionRoom>> dredgionRooms{AION_LOCK_CLASS(DredgionInstance::dredgionRooms)}; // Java: = new ArrayList<>()
	runtime::AtomicInteger killedSurkanas{AION_LOCK_CLASS(DredgionInstance::killedSurkanas)}; // Java: = new AtomicInteger()
	runtime::Field<runtime::Ref<runtime::Rc<runtime::AtomicBoolean>>> isInstanceStarted; // Java: = new AtomicBoolean()
	runtime::Field<runtime::Ref<PvpInstanceScore>> instanceScore{};

private:
	static constexpr int32_t MAX_PLAYERS_PER_FACTION = 6;
	const int32_t raceStartPosition;
	runtime::Field<runtime::FutureRef> instanceTask{};
	runtime::Field<int64_t> instanceTime{};
	runtime::Field<float> losingGroupMultiplier{1};
	runtime::Field<bool> isInstanceDestroyed{};

protected:
	explicit DredgionInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DredgionInstance> create(WorldMapInstance& instance);

protected:
	runtime::Ptr<PvpInstancePlayerReward> getPlayerReward(Player& player);

	void captureRoom(Race race, int32_t roomId);

private:
	void addPlayerToReward(Player& player);

protected:
	void startInstanceTask();

public:
	void onEnterInstance(Player& player) override;

	void onInstanceCreate() override;

protected:
	void initializeInstance(int32_t winnerAp, int32_t loserAp, int32_t drawAp);

	void stopInstance(Race winnerRace);

public:
	virtual void doReward(Player& player, PvpInstancePlayerReward& reward, Race winningRace);

private:
	void distributeRewards(Player& player, PvpInstancePlayerReward& reward);

public:
	bool onReviveEvent(Player& player) override;

protected:
	void revivePlayerOnEnd(Player& player);

public:
	bool onDie(Player& player, Creature& lastAttacker) override;

private:
	void addPointToPlayer(Player& player, int32_t points);

	void addPvPKillToPlayer(Player& player);

	void addBalaurKillToPlayer(Player& player);

protected:
	void updateScore(Player& player, runtime::Ptr<Creature> target, int32_t points, bool pvpKill);

public:
	void onDie(Npc& npc) override;

	void onInstanceDestroy() override;

protected:
	virtual void openFirstDoors();

private:
	void sendPacket();

	int32_t getTime();

protected:
	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time);

	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t staticId, int32_t time);

	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time, std::string_view walkerId);

	void sendMsgByRace(int32_t msg, Race race, int32_t time);

private:
	void stopInstanceTask();

public:
	runtime::Ptr<InstanceScore> getInstanceScore() override;

	void leaveInstance(Player& player) override;

	void portToStartPosition(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance::dredgion
