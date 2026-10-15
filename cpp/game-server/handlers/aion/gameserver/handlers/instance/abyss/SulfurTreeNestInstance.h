#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <optional>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * Sulfur Tree Nest (300060000): the start ring begins a 15 minute run (its treasure goes when the time is up), the first player's race
 * chooses the gold
 * chest.
 * <p>
 * Java: data/handlers/instance/abyss/SulfurTreeNestInstance.java, @InstanceID(300060000). C++: the scheduled lambda is pinned to the
 * handler.
 *
 * @author xTz
 */
class SulfurTreeNestInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicLong startTime{AION_LOCK_CLASS(SulfurTreeNestInstance::startTime)}; // Java: = new AtomicLong()
	runtime::Field<std::optional<Race>> instanceRace{}; // fieldmap: Java null until the first entry; a nullable enum (hub-headers.md §6)

protected:
	explicit SulfurTreeNestInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<SulfurTreeNestInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	bool onPassFlyingRing(Player& player, std::string_view flyingRing) override;

	void onEnterInstance(Player& player) override;

private:
	void spawnGoldChest();
};

} // namespace aion::gameserver::handlers::instance::abyss
