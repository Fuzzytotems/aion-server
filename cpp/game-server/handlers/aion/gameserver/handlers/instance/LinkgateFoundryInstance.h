#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Linkgate Foundry (301270000): the first player's race picks the entrance npc and Belsagos' reward; the opened door starts the 20 minute
 * timer, which clears the foundry when it runs out unless the boss area is entered first; the secret lab's three entries.
 * <p>
 * Java: data/handlers/instance/LinkgateFoundryInstance.java, @InstanceID(301270000). C++: the scheduled lambda is pinned to the handler.
 *
 * @author Cheatkiller
 */
class LinkgateFoundryInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::FutureRef> timeCheckTask{};
	runtime::Field<int8_t> timeInMin{static_cast<int8_t>(-1)};
	runtime::Field<int8_t> secretLabEntranceCount{};
	runtime::Field<std::optional<Race>> instanceRace{}; // fieldmap: Java null until the first entry; a nullable enum (hub-headers.md §6)

protected:
	explicit LinkgateFoundryInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<LinkgateFoundryInstance> create(WorldMapInstance& instance);

	void onEnterInstance(Player& player) override;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void startTimeCheck();

public:
	void onEnterZone(Player& player, ZoneInstance& zone) override;

	void onInstanceDestroy() override;
};

} // namespace aion::gameserver::handlers::instance
