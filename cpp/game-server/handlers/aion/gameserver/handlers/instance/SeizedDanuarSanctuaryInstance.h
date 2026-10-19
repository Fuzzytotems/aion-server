#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Seized Danuar Sanctuary (301140000): Danuar Sanctuary with the first player's race's quest npcs and its own three bosses.
 * <p>
 * Java: data/handlers/instance/SeizedDanuarSanctuaryInstance.java, @InstanceID(301140000) (the marker is in the .cpp). C++: the scheduled lambdas are pinned to the
 * handler.
 *
 * @author Yeats
 */
class SeizedDanuarSanctuaryInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<std::optional<Race>> instanceRace{}; // fieldmap: Java null until the first entry; a nullable enum (hub-headers.md §6)
	runtime::AtomicBoolean started{AION_LOCK_CLASS(SeizedDanuarSanctuaryInstance::started), false}; // Java: = new AtomicBoolean(false)
	runtime::AtomicBoolean cannonUsed{AION_LOCK_CLASS(SeizedDanuarSanctuaryInstance::cannonUsed), false}; // Java: = new AtomicBoolean(false)

protected:
	explicit SeizedDanuarSanctuaryInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<SeizedDanuarSanctuaryInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onEnterInstance(Player& player) override;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	bool onReviveEvent(Player& player) override;

private:
	void spawnRndBoss();

	void spawnQuestNpcs();
};

} // namespace aion::gameserver::handlers::instance
