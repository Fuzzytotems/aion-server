#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Beshmundir Temple (300170000): the soulwatchers' boatmen, Temadaro and the Macunbello whose strength the killed Lich's servants decide, the
 * specters' count, the guardians whose deaths open the doors, the super boss after Isbariya and Lupukin after Dorakiki.
 * <p>
 * Java: data/handlers/instance/BeshmundirInstance.java, @InstanceID(300170000).
 *
 * @author Gigi, nrg, oslo0322, xTz
 */
class BeshmundirInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicInteger macunbello{AION_LOCK_CLASS(BeshmundirInstance::macunbello)};
	runtime::AtomicInteger kills{AION_LOCK_CLASS(BeshmundirInstance::kills)};
	runtime::Field<std::optional<Race>> instanceRace{}; // fieldmap: Java null until the first entry; a nullable enum (hub-headers.md §6)

protected:
	explicit BeshmundirInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<BeshmundirInstance> create(WorldMapInstance& instance);

	void onEnterInstance(Player& player) override;

	float getExpMultiplier() override;

	using GeneralInstanceHandler::onDie;

	void onDie(Npc& npc) override;

private:
	void sendPacket(network::aion::AionServerPacket&& packet);

public:
	void onPlayMovieEnd(Player& player, int32_t movieId) override;

	void onInstanceCreate() override;
};

} // namespace aion::gameserver::handlers::instance
