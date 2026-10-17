#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Shugos.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of the intruder bosses in The Shugo Emperor's Vault ("IDSweep_Boss"): an intruder immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Bosses.java, @AIName("IDSweep_Boss") (the marker is in the .cpp).
 */
class IDSweep_Bosses : public IDSweep_Shugos {
public:
	explicit IDSweep_Bosses(Npc& owner) : IDSweep_Shugos(owner) {}

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
