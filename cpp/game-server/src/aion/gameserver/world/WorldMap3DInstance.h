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
 * World map instance with 3D map regions (Reshanta).
 * <p>
 * C++ (spine S0B-042/118): created with create(), which calls initMapRegions() after construction (see WorldMap2DInstance). The regions are
 * created on the ForkJoin common pool like Java's parallel stream; `synchronized (regions)` is the PartMap Monitor.
 *
 * @author ATracer
 */
class WorldMap3DInstance : public WorldMapInstance {
	AION_MAKE_REF_FRIEND
protected:
	WorldMap3DInstance(WorldMap& parent, int32_t instanceId, int32_t maxPlayers,
		const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier);
	~WorldMap3DInstance() override;

public:
	/** Java: new WorldMap3DInstance(parent, instanceId, maxPlayers, instanceHandlerSupplier), including initMapRegions() */
	static runtime::Ref<WorldMap3DInstance> create(WorldMap& parent, int32_t instanceId, int32_t maxPlayers,
		const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier);

	using WorldMapInstance::getRegion;

	runtime::Ptr<MapRegion> getRegion(float x, float y, float z) override;

protected:
	void initMapRegions() override;

	std::unique_ptr<MapRegion> createMapRegion(int32_t regionId) override;

public:
	bool isPersonal() override;

	int32_t getOwnerId() override;
};

} // namespace aion::gameserver::world
