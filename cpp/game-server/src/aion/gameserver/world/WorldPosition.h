#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::world {

/**
 * Position of object in the world.
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted (fieldmap K4): created with `WorldPosition::create(...)` (Java `new WorldPosition(...)`),
 * held by `VisibleObject::position` and `WorldMapInstance::startPos` as `Field<Ref<WorldPosition>>`. `mapRegion` is a Ref to a part of a
 * WorldMapInstance and so retains that instance (the VisibleObject.position -> WorldPosition.mapRegion -> MapRegion.objects cycle and the
 * startPos cycle cut in destroyInstance are resolved in cycles.toml).
 *
 * @author -Nemesiss-
 */
class WorldPosition : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** Map id. */
	const int32_t mapId;
	/** Map Region. */
	runtime::Field<runtime::Ref<MapRegion>> mapRegion{};
	/** World position coordinate */
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	/** Value from 0 to 120 (120==0 actually) */
	runtime::Field<int8_t> heading{};
	/** indicating if object is spawned or not. (C++: trailing underscore, the name clashes with isSpawned()) */
	runtime::Field<bool> isSpawned_{false};

protected:
	explicit WorldPosition(int32_t mapId);
	WorldPosition(int32_t mapId, float x, float y, float z, int8_t h);
	WorldPosition(int32_t mapId, float x, float y, float z, int8_t h, runtime::Ptr<MapRegion> mapRegion);
	~WorldPosition() override;

public:
	/** Java: new WorldPosition(mapId) */
	static runtime::Ref<WorldPosition> create(int32_t mapId);
	/** Java: new WorldPosition(mapId, x, y, z, h) */
	static runtime::Ref<WorldPosition> create(int32_t mapId, float x, float y, float z, int8_t h);
	/** Java: new WorldPosition(mapId, x, y, z, h, mapRegion) */
	static runtime::Ref<WorldPosition> create(int32_t mapId, float x, float y, float z, int8_t h, runtime::Ptr<MapRegion> mapRegion);

	/** Return World map id. Logs a warning if it is 0. */
	int32_t getMapId();

	/** Return World position x */
	float getX() const { return x.get(); }

	/** Return World position y */
	float getY() const { return y.get(); }

	/** Return World position z */
	float getZ() const { return z.get(); }

	/**
	 * If you need the map region under any conditions use this method, but for most use cases you should use {@link VisibleObject#getMapRegion()},
	 * since it respects the isSpawned state.
	 *
	 * @return Map region (null if not set)
	 */
	runtime::Ptr<MapRegion> getMapRegion() const { return mapRegion.get(); }

	bool isMapRegionActive();

	int32_t getInstanceId();

	bool isInstanceMap();

	/** Return heading. */
	int8_t getHeading() const { return heading.get(); }

	/** @return worldMapInstance (NullPointerException without a map region, like Java) */
	runtime::Ptr<WorldMapInstance> getWorldMapInstance();

	/** Check if object is spawned. */
	bool isSpawned() const { return isSpawned_.get(); }

	/** Set isSpawned to given value. (Java package-private) */
	void setIsSpawned(bool val) { isSpawned_.set(val); }

	/** Set map region (Java package-private). Out of line: it releases the previous region. */
	void setMapRegion(runtime::Ptr<MapRegion> r);

	/**
	 * Set world position. Absent values (Java null) keep the current coordinate.
	 *
	 * @param newHeading
	 *          Value from 0 to 120 (120==0 actually)
	 */
	void setXYZH(std::optional<float> newX, std::optional<float> newY, std::optional<float> newZ, std::optional<int8_t> newHeading);

	void setZ(float value) { z.set(value); }

	void setH(int8_t h) { heading.set(h); }

	int32_t hashCode() const;

	bool equals(const WorldPosition& obj) const;

	std::string toString();

	std::string toCoordString();
};

} // namespace aion::gameserver::world
