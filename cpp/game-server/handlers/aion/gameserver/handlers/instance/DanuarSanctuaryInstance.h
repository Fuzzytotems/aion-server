#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Danuar Sanctuary (301380000): one of three bosses by Rnd, the emergency exit after it, the cannon against the barricade, the teleports.
 * <p>
 * Java: data/handlers/instance/DanuarSanctuaryInstance.java, @InstanceID(301380000) (the marker is in the .cpp). C++: the scheduled lambdas are pinned to the
 * handler.
 *
 * @author Ritsu, Tibald
 */
class DanuarSanctuaryInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean cannonUsed{AION_LOCK_CLASS(DanuarSanctuaryInstance::cannonUsed), false}; // Java: = new AtomicBoolean(false)

protected:
	explicit DanuarSanctuaryInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DanuarSanctuaryInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	bool onReviveEvent(Player& player) override;

private:
	void spawnRndBoss();
};

} // namespace aion::gameserver::handlers::instance
