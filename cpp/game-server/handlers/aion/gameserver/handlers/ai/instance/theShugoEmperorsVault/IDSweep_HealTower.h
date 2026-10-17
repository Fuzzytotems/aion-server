#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of the healing springs in The Shugo Emperor's Vault ("idsweep_healtower"): every 3 s from 2 s, while a living player who is not at
 * full HP stands within 3 m, it targets itself and casts 21837.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/IDSweep_HealTower.java, @AIName("idsweep_healtower") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class IDSweep_HealTower : public GeneralNpcAI {
private:
	runtime::Field<runtime::FutureRef> schedule{};

public:
	explicit IDSweep_HealTower(Npc& owner) : GeneralNpcAI(owner) {}

	void handleSpawned() override;

protected:
	void handleDespawned() override;

	void handleDied() override;

private:
	void cancelTask();

	void tryHeal();
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
