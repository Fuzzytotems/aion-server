#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/NoActionAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

#include <string>
#include <string_view>

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the wave portals ("wave_portal"): from 8 s after the spawn, every 22 s (the central one 40 s) the wave of its static id
 * comes out on its walker routes; gone, it cancels its tasks.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/WavePortalAI.java, @AIName("wave_portal") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class WavePortalAI : public NoActionAI {
private:
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(WavePortalAI::tasks)}; // Java: = new ArrayList<>()

public:
	explicit WavePortalAI(Npc& owner) : NoActionAI(owner) {}

protected:
	void handleSpawned() override;

private:
	using NoActionAI::spawn;

	void spawn(int32_t npcId, float x, float y, float z, int32_t heading, std::string_view walkerId);

	void spawn(int32_t npcId, float x, float y, float z, int32_t heading, std::string_view walkerId, int32_t delay);

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
