#include "aion/gameserver/handlers/admincommands/Dispel.h"

#include "aion/gameserver/controllers/effect/EffectController.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Dispel);

Dispel::Dispel() : AdminCommand("dispel", "Removes all effects including transformations.") {
}

// Java Dispel.java:18-27
void Dispel::execute(Player& admin, std::span<const std::string> /*params*/) {
	runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (target == nullptr)
		target = runtime::Ptr<VisibleObject>(&admin); // parity= target = admin;
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(target)) { // parity= if (target instanceof Creature creature) {
		creature->getEffectController()->removeAllEffects();
		creature->getEffectController()->removeTransformEffects();
		sendInfo(admin, "Removed all effects of " + target->toString() + "."); // parity= sendInfo(admin, "Removed all effects of " + target + ".");
	}
}

} // namespace aion::gameserver::handlers::admincommands
