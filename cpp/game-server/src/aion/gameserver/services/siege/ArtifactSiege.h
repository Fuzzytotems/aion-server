#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/siege/Siege.h"

namespace aion::gameserver::services::siege {

/**
 * Java: services.siege.ArtifactSiege (a Siege<ArtifactLocation>): the endless siege of an artifact - it runs until its protector dies, then the
 * killer race (and legion) captures the artifact and a new siege starts at once. The location is the base's SiegeLocation, narrowed by
 * getSiegeLocation() (Java's type parameter).
 *
 * @author SoulKeeper
 */
class ArtifactSiege : public Siege {
	AION_MAKE_REF_FRIEND
protected:
	explicit ArtifactSiege(model::siege::ArtifactLocation& siegeLocation);

public:
	static runtime::Ref<ArtifactSiege> create(model::siege::ArtifactLocation& siegeLocation);

	/** Java: Siege<ArtifactLocation>.getSiegeLocation() - the base's location as the ArtifactLocation it is */
	runtime::Ptr<model::siege::ArtifactLocation> getSiegeLocation() const;

protected:
	void onSiegeStart() override;

	void onSiegeFinish() override;

	void onCapture();

public:
	bool isEndless() override;

	void onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abysPoints) override;

protected:
	~ArtifactSiege() override;
};

} // namespace aion::gameserver::services::siege
