#pragma once

#include "aion/gameserver/handlers/instance/crucible/CrucibleInstance.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance::crucible {

/**
 * Crucible Challenge (300320000), the solo crucible: six stages started by the record keepers, the bonus stages 2 (Poppy) and 4 (the
 * chests), the points per kill and Crucible Insignias of Valor (186000130) by the points.
 * <p>
 * Java: data/handlers/instance/crucible/CrucibleChallengeInstance.java, @InstanceID(300320000) (the marker is in the .cpp). C++: the
 * scheduled lambdas are pinned to the handler.
 *
 * @author xTz
 */
class CrucibleChallengeInstance : public CrucibleInstance {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> stage1Count{};
	runtime::Field<runtime::FutureRef> bonusTimer{};
	runtime::Field<int32_t> spawnCount{};
	runtime::Field<int32_t> dieCount{};
	runtime::Field<int32_t> rewardCount{};

protected:
	explicit CrucibleChallengeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<CrucibleChallengeInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onEnterInstance(Player& player) override;

private:
	/** Java's null npcL10n is std::nullopt */
	void sendPacket(std::optional<std::string_view> npcL10n, int32_t points);

	void sendEventPacket();

public:
	void onDie(Npc& npc) override;

private:
	void increasePointsForKill(Npc& npc, runtime::Ptr<Player> killer);

	bool allDeadOrDespawned(std::initializer_list<int32_t> npcIds);

	void startStage3Round1_1(int32_t npcId);

public:
	void doReward(Player& player) override;

private:
	void startBonusStage2();

	void spawnBonus2();

public:
	void onInstanceDestroy() override;

private:
	void passStage2();

public:
	bool onDie(Player& player, Creature& lastAttacker) override;

	bool onReviveEvent(Player& player) override;

	void leaveInstance(Player& player) override;

	void onStopTraining(Player& player) override;

	void onChangeStage(StageType type) override;

private:
	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time);

	bool changeStage(StageType type, int32_t time);

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onDropRegistered(Npc& npc, int32_t winnerObj) override;
};

} // namespace aion::gameserver::handlers::instance::crucible
