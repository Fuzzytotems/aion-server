#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/AbstractSiegeProtectorAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The protector of an artifact ("artifact_protector"): from level 65 its maximum hp takes the artifact protector multiplier of SiegeConfig.
 * <p>
 * Java: data/handlers/ai/siege/ArtifactProtectorAI.java, @AIName("artifact_protector") (the marker is in the .cpp). M5i I-2, an A1 lease.
 *
 * @author ATracer
 */
class ArtifactProtectorAI : public AbstractSiegeProtectorAI {
public:
	explicit ArtifactProtectorAI(Npc& owner) : AbstractSiegeProtectorAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;
};

} // namespace aion::gameserver::handlers::ai::siege
