#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Draupnir Cave (320080000): the start ring's message, one of four random named monsters, Commander Bakarma after the four adjutants,
 * and the revive at the entrance.
 * <p>
 * Java: data/handlers/instance/DraupnirCaveInstance.java.
 *
 * @author Sykra
 */
class DraupnirCaveInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isInstanceStartMessageSend{AION_LOCK_CLASS(DraupnirCaveInstance::isInstanceStartMessageSend)}; // Java: = new AtomicBoolean()
	runtime::Field<int32_t> adjutantsKilled{};

protected:
	explicit DraupnirCaveInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DraupnirCaveInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	bool onPassFlyingRing(Player& player, std::string_view flyingRing) override;

	void onDie(Npc& npc) override;

	bool onReviveEvent(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance
