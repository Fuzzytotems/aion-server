#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

/**
 * The AI of the base generators in Engulfed Ophidan Bridge ("engulfedophidiangens"): the flag of the base and owner (by the spawn's
 * static id) and its message at the spawn; the flag is deleted with the generator.
 * <p>
 * Java: data/handlers/ai/instance/engulfedOphidianBridgeInstance/GeneratorsAI.java, @AIName("engulfedophidiangens") (the marker is in the
 * .cpp).
 *
 * @author Cheatkiller
 */
class GeneratorsAI : public ActionItemNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> flag{}; // Java: Npc flag (null until spawned)

public:
	explicit GeneratorsAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
