#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Padmarashka's Cave (320150000): her four protectors wake her, twenty eggs enrage her, her nest plays its movie once.
 * <p>
 * Java: data/handlers/instance/PadmarashkasCaveInstance.java, @InstanceID(320150000) (the marker is in the .cpp). C++: the scheduled lambdas are pinned to the
 * handler.
 *
 * @author Ritsu, Luzien
 */
class PadmarashkasCaveInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean moviePlayed{AION_LOCK_CLASS(PadmarashkasCaveInstance::moviePlayed)}; // Java: = new AtomicBoolean()
	runtime::AtomicInteger killedPadmarashkaProtector{AION_LOCK_CLASS(PadmarashkasCaveInstance::killedPadmarashkaProtector)}; // Java: = new AtomicInteger()
	runtime::AtomicInteger killedEggs{AION_LOCK_CLASS(PadmarashkasCaveInstance::killedEggs)}; // Java: = new AtomicInteger()

protected:
	explicit PadmarashkasCaveInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<PadmarashkasCaveInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;

	void onEnterZone(Player& player, world::zone::ZoneInstance& zone) override;

	void onInstanceDestroy() override;
};

} // namespace aion::gameserver::handlers::instance
