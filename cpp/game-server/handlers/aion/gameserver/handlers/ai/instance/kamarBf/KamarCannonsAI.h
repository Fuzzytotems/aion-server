#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

/**
 * The AI of the Kamar Battlefield siege weapons ("kamarcannons"): the use applies the weapon effect (the siege weapons take a Siege
 * Power, 164000262) and deletes the weapon; a flag npc at the siege weapons.
 * <p>
 * Java: data/handlers/ai/instance/kamarBf/KamarCannonsAI.java, @AIName("kamarcannons") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class KamarCannonsAI : public ActionItemNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> flag{}; // Java: Npc flag (null until spawned)

public:
	explicit KamarCannonsAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
