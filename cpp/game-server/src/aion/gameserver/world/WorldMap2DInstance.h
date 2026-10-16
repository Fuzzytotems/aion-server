#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::world {

/**
 * World map instance with 2D map regions (every map except Reshanta).
 * <p>
 * C++ (spine S0B-042/118): created with create(), which constructs the instance and then calls initMapRegions() before anyone else can see
 * it (Java calls it from the WorldMapInstance constructor). createMapRegion returns the new region part, stored into `regions` by
 * initMapRegions. `using WorldMapInstance::getRegion` keeps the VisibleObject overload visible next to the override.
 *
 * @author ATracer
 */
class WorldMap2DInstance : public WorldMapInstance {
	AION_MAKE_REF_FRIEND
private:
	const int32_t ownerId;

protected:
	WorldMap2DInstance(WorldMap& parent, int32_t instanceId, int32_t ownerId, int32_t maxPlayers,
		const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier);
	~WorldMap2DInstance() override;

public:
	/** Java: new WorldMap2DInstance(parent, instanceId, ownerId, maxPlayers, instanceHandlerSupplier), including initMapRegions() */
	static runtime::Ref<WorldMap2DInstance> create(WorldMap& parent, int32_t instanceId, int32_t ownerId, int32_t maxPlayers,
		const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier);

protected:
	std::unique_ptr<MapRegion> createMapRegion(int32_t regionId) override;

	void initMapRegions() override;

public:
	using WorldMapInstance::getRegion;

	runtime::Ptr<MapRegion> getRegion(float x, float y, float z) override;

	int32_t getOwnerId() override;

	bool isPersonal() override;
};

} // namespace aion::gameserver::world
