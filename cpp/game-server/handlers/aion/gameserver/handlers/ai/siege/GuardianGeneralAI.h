#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * The guardian general ("guardian_general"): a siege npc with no behaviour of its own yet.
 * <p>
 * Java: data/handlers/ai/siege/GuardianGeneralAI.java, @AIName("guardian_general") (the marker is in the .cpp). M5i I-2, an A1 lease.
 *
 * @author Estrayl
 */
class GuardianGeneralAI : public SiegeNpcAI {
public:
	explicit GuardianGeneralAI(Npc& owner) : SiegeNpcAI(owner) {}

	// TODO: Working AI for BA
};

} // namespace aion::gameserver::handlers::ai::siege
