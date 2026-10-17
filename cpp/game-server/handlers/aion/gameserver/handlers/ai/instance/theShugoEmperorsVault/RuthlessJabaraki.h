#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Bosses.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of Ruthless Jabaraki in The Shugo Emperor's Vault ("ruthless_jabaraki"): his HP phases call waves of intruders on the room's four
 * corners, which take his most hated player as theirs; his death deletes him; back home his phases start over.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/RuthlessJabaraki.java, @AIName("ruthless_jabaraki") (the marker is in the .cpp).
 * **Java bug, ported (proposed correction J-I6-2):** the phases are 96, 80, 60, 45, 40 and 35 % but handleHpPhase switches on 95, 80, 65, 50, 35
 * and 20, so only the 80 % (wave 1) and 35 % (wave 4) phases call adds; proposed: the HpPhases of the switch, 95, 80, 65, 50, 35, 20.
 *
 * @author Yeats
 */
class RuthlessJabaraki : public IDSweep_Bosses, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(96, {80, 60, 45, 40, 35}); // Java: = new HpPhases(96, 80, 60, 45, 40, 35)
	/** Java: private List<Npc> spawnedAdds = new ArrayList<>() */
	runtime::ArrayList<runtime::Ref<Npc>> spawnedAdds{AION_LOCK_CLASS(RuthlessJabaraki::spawnedAdds)};

public:
	explicit RuthlessJabaraki(Npc& owner) : IDSweep_Bosses(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnAdds(int32_t stage);

	void startHate();

protected:
	void handleBackHome() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
