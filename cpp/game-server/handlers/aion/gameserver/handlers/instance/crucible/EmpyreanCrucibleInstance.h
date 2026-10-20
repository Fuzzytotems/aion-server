#pragma once

#include "aion/gameserver/handlers/instance/crucible/CrucibleInstance.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance::crucible {

/**
 * Empyrean Crucible (300300000), the group crucible: nine stages of rounds started by the record keepers and the elevators, the bonus
 * stages 2 to 4 and 6, the points of every kill for every player inside, the insignias by the points and the ready room for the defeated.
 * <p>
 * Java: data/handlers/instance/crucible/EmpyreanCrucibleInstance.java, @InstanceID(300300000) (the marker is in the .cpp). C++: the
 * scheduled lambdas are pinned to the handler; Java's `synchronized (npcs)` is SYNCHRONIZED on the list.
 *
 * @author xTz, Luzien
 */
class EmpyreanCrucibleInstance : public CrucibleInstance {
	AION_MAKE_REF_FRIEND
private:
	class EmpyreanStage;

	runtime::ArrayList<runtime::Ref<Npc>> npcs{AION_LOCK_CLASS(EmpyreanCrucibleInstance::npcs)}; // Java: = new ArrayList<>()
	runtime::ArrayList<runtime::Ref<EmpyreanStage>> empyreanStage{AION_LOCK_CLASS(EmpyreanCrucibleInstance::empyreanStage)}; // Java: = new ArrayList<>()
	runtime::Field<int8_t> stage{};
	runtime::Field<bool> isDoneStage4{}, isDoneStage6Round2{}, isDoneStage6Round1{};

	/**
	 * A round whose npcs are checked together: containsNpcs is true while one of them is still in the instance.
	 * <p>
	 * Java: the inner class EmpyreanStage (its outer instance is the captured handler).
	 */
	class EmpyreanStage : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		runtime::ArrayList<runtime::Ref<Npc>> stageNpcs{AION_LOCK_CLASS(EmpyreanCrucibleInstance::EmpyreanStage::stageNpcs)};
		const runtime::Ref<EmpyreanCrucibleInstance> empyreanCrucibleInstance; // Java: the captured this

	private:
		EmpyreanStage(EmpyreanCrucibleInstance& empyreanCrucibleInstance, const std::vector<runtime::Ptr<Npc>>& npcs);

	public:
		static runtime::Ref<EmpyreanStage> create(EmpyreanCrucibleInstance& empyreanCrucibleInstance, const std::vector<runtime::Ptr<Npc>>& npcs);

		bool containsNpcs();
	};

protected:
	explicit EmpyreanCrucibleInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<EmpyreanCrucibleInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onEnterInstance(Player& player) override;

private:
	void addPointsAndSendPacket(int32_t points, Npc& npc);

	void setStage(StageType type, int32_t time);

public:
	void onDie(Npc& npc) override;

private:
	void startBonusStage3();

	void startBonusStage4();

	void startStage4Round4_1();

	void startStage4Round3();

	void startStage2Round2();

	void startStage2Round3();

	void startStage2Round5();

	void rewardGroup();

public:
	void doReward(Player& player) override;

	void onInstanceDestroy() override;

	bool onDie(Player& player, Creature& lastAttacker) override;

	bool onReviveEvent(Player& player) override;

private:
	runtime::Ptr<EmpyreanStage> getEmpyreanStage(Npc& npc);

	bool isSpawn(const std::vector<int32_t>& round);

public:
	void onChangeStage(StageType type) override;

private:
	void startStage1Round2();

	void startStage1Round3();

	void startStage1Round4();

	void startStage1Round5();

	void teleport(float x, float y, float z, int8_t h);

	using CrucibleInstance::teleport;

	void moveToReadyRoom(Player& player);

public:
	void leaveInstance(Player& player) override;

	void onLeaveInstance(Player& player) override;

	void onStopTraining(Player& player) override;

private:
	void sp(int32_t npcId, float x, float y, float z, int8_t h, int32_t time);

	runtime::Ptr<Npc> sp(int32_t npcId, float x, float y, float z, int8_t h);

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onDropRegistered(Npc& npc, int32_t winnerObj) override;
};

} // namespace aion::gameserver::handlers::instance::crucible
