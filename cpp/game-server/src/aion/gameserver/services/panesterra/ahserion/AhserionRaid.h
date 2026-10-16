#pragma once

#include <cstdint>
#include <functional>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"

namespace aion::gameserver::services::panesterra::ahserion {

/**
 * C++: a per-run service (fieldmap.toml per_run_services, RefCounted) with Java's singleton accessor: getInstance() creates one instance that
 * is never released. The ported constructor fills the immutable faction list (Java List.of).
 *
 * @author Yeats, Neon, Estrayl
 */
class AhserionRaid : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<PanesterraFaction> factions{AION_LOCK_CLASS(AhserionRaid::factions)}; // Java: = List.of(BELUS, ASPIDA, ATANATOS, DISILLON)
	runtime::AtomicBoolean isStarted_{AION_LOCK_CLASS(AhserionRaid::isStarted)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::Ref<PanesterraTeam>> winner{};
	runtime::Field<runtime::FutureRef> progressTask{};

protected:
	AhserionRaid();
	~AhserionRaid() override;

public:
	static AhserionRaid& getInstance(); // Java singleton
	void start();
	void stop();
private:
	void cleanUp();
	void startInstanceTimer();
	void checkForIllegalMovement();
	void spawnRaid();
public:
	void spawnStage(int32_t stage, PanesterraFaction faction);
	void handleCorridorShieldDestruction(int32_t npcId);
private:
	void sendConsolationReward(PanesterraTeam& eliminatedTeam);
public:
	void handleBossKilled(model::gameobjects::Npc& ahserion, PanesterraFaction winnerFaction);
private:
	void sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg);
	void deleteNpcs(PanesterraFaction eliminatedFaction, int32_t flagToDelete);
public:
	void forEachTeam(const std::function<void(PanesterraTeam&)>& consumer);
private:
	void cancelProgressTask();
public:
	bool isStarted();
};

} // namespace aion::gameserver::services::panesterra::ahserion
