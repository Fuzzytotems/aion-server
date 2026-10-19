#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/NoActionAI.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include <functional>
#include <string_view>

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * The vortices of the Illuminary Obelisk's generators ("idf5_u3_vortex"): every two minutes two waves of walkers from their side.
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/IDF5_U3_VortexAI.java, @AIName("idf5_u3_vortex") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class IDF5_U3_VortexAI : public NoActionAI {
private:
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(IDF5_U3_VortexAI::tasks)}; // Java: = new ArrayList<>()
	runtime::ArrayList<int32_t> npcIds{AION_LOCK_CLASS(IDF5_U3_VortexAI::npcIds)}; // Java: = new ArrayList<>()

public:
	explicit IDF5_U3_VortexAI(Npc& owner) : NoActionAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void handlePhaseAttacks();

	void spawnWalker(int32_t npcId, float x, float y, float z, int8_t h, int32_t delay, std::string_view walkerId);

	void addTask(std::function<void()> task, int32_t delayMs);

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
