#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/siege/Siege.h"

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.OutpostSiege (a Siege<OutpostLocation>): the siege of a Balaurea outpost - its agent (Enraged Mastarius or Veille)
 * spawns, and its death rewards the killer race with a world buff. The location is the base's SiegeLocation, narrowed by getSiegeLocation()
 * (Java's type parameter).
 *
 * @author SoulKeeper, Estrayl
 */
class OutpostSiege : public Siege {
	AION_MAKE_REF_FRIEND
protected:
	explicit OutpostSiege(model::siege::OutpostLocation& siegeLocation);

public:
	static runtime::Ref<OutpostSiege> create(model::siege::OutpostLocation& siegeLocation);

	/** Java: Siege<OutpostLocation>.getSiegeLocation() - the base's location as the OutpostLocation it is */
	runtime::Ptr<model::siege::OutpostLocation> getSiegeLocation() const;

protected:
	void onSiegeStart() override;

	void onSiegeFinish() override;

private:
	void onAgentDefeated();

public:
	void despawnSiegeNpcs();

	bool isEndless() override;

	void onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) override;

protected:
	~OutpostSiege() override;
};

} // namespace aion::gameserver::services::siege
