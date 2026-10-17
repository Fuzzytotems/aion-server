#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of the intruders in The Shugo Emperor's Vault ("IDSweep_shugos"): spawned after the run's end it is deleted; its auto attacks deal
 * its template attack -20 % to +25 %.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Shugos.java, @AIName("IDSweep_shugos") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class IDSweep_Shugos : public AggressiveNoLootNpcAI {
private:
	runtime::Field<int32_t> baseDamage{};

public:
	explicit IDSweep_Shugos(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
