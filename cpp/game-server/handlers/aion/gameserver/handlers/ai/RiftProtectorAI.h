#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The guard of a rift ("rift_protector"): an aggressive npc with a tenth of its maximum hp.
 * <p>
 * Java: data/handlers/ai/RiftProtectorAI.java, @AIName("rift_protector") (the marker is in the .cpp). M5i I-5, under P5-12b2's lease.
 *
 * @author Estrayl
 */
class RiftProtectorAI : public AggressiveNpcAI {
public:
	explicit RiftProtectorAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;
};

} // namespace aion::gameserver::handlers::ai
