#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

/**
 * The AI of the explosion devices in Engulfed Ophidan Bridge ("engulfedophidianexplosiondevice"): with a scroll (164000278) the use spawns
 * the race's bombs at the device's base; they cast 21178 after 5 s and are deleted after 20 s.
 * <p>
 * Java: data/handlers/ai/instance/engulfedOphidianBridgeInstance/ExplosionDeviceAI.java, @AIName("engulfedophidianexplosiondevice") (the
 * marker is in the .cpp). Java's bomb list is never cleared: a second use finds the first bombs in it; ported as written.
 *
 * @author cheatkiller
 */
class ExplosionDeviceAI : public ActionItemNpcAI {
private:
	runtime::ArrayList<runtime::Ref<Npc>> bomb{AION_LOCK_CLASS(ExplosionDeviceAI::bomb)}; // Java: = new ArrayList<>()

public:
	explicit ExplosionDeviceAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

private:
	void boom();

	bool checkScroll(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
