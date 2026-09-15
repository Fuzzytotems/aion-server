#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/models/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine {

/**
 * Loads the geo data of data/geo: the terrain PNGs, the models of models.mesh and the placements of the <mapId>.geo files (big endian).
 * <p>
 * K5 confined (fieldmap): a static utility class. C++ notes:
 * - The PNGs are decoded by utils::PngImage (Java ImageIO).
 * - Terrain images are decoded in parallel, but applied to the maps' terrains in file name order on the calling thread (DEVIATIONS: Java sets
 *   heightmap and materials of one terrain from parallel threads, racing with the size checks between them).
 * - The collision trees of all meshes of the loaded maps are built eagerly (ForkJoin pool) before load returns (DEVIATIONS: Java builds them
 *   asynchronously on the long-running pool while collision checks may already build them lazily).
 * - Material zones are created through GeoCallbacks::createMaterialZone (Java: ZoneName.createOrGet and ZoneService).
 * - The C++-only overload with a directory returns the load statistics (tests, M4 item 5).
 *
 * @author Mr. Poke, Neon, Yeats
 */
class GeoWorldLoader {
private:
	/** relative to the working directory (game-server/), like Java's Path.of("data/geo/") */
	static inline const std::filesystem::path GEO_DIR = "data/geo/";

public:
	/** C++ only: what load read and created. */
	struct Statistics {
		/** records of models.mesh */
		int32_t meshEntries = 0;
		/** Mesh objects read from models.mesh */
		int32_t meshes = 0;
		/** model names after expanding `a|b` names (Java: the "Loaded N meshes" value) */
		int32_t meshNames = 0;
		/** <mapId>.geo files read */
		int32_t geoFiles = 0;
		/** records of the .geo files */
		int32_t placements = 0;
		/** records naming a model that does not exist */
		int32_t missingMeshPlacements = 0;
		/** nodes attached to maps: placements with a model plus the higher town level nodes (Java: the "Loaded N entities" value) */
		int32_t attachedNodes = 0;
		/** geometries attached to maps (the children of the attached nodes) */
		int32_t geometries = 0;
		/** the geometries of the records' own nodes (without the higher town level nodes) */
		int32_t placementGeometries = 0;
		/** geometries with MATERIAL collision intention handed to GeoCallbacks::createMaterialZone */
		int32_t materialGeometries = 0;
		/** maps with a terrain heightmap */
		int32_t terrainMaps = 0;
		/** meshes whose collision tree was built */
		int32_t collisionTrees = 0;
	};

	GeoWorldLoader() = delete;

	/** Java: load(Collection<GeoMap> maps) from data/geo/ */
	static void load(const std::vector<runtime::Ptr<models::GeoMap>>& maps);

	/** C++ only: loads from geoDir and returns the statistics */
	static Statistics load(const std::vector<runtime::Ptr<models::GeoMap>>& maps, const std::filesystem::path& geoDir);

private:
	/**
	 * Loads heightmap and material data from PNG images and assigns it to all maps matching the file name. Since terrain data is static, it is
	 * safe to share it between maps.
	 */
	static void loadTerrains(const std::vector<runtime::Ptr<models::GeoMap>>& maps, const std::filesystem::path& geoDir, Statistics& statistics);

	static void load(const std::vector<runtime::Ptr<models::GeoMap>>& maps, const std::unordered_map<std::string, runtime::Ref<scene::Node>>& models,
		const std::filesystem::path& geoDir, Statistics& statistics);

	static std::unordered_map<std::string, runtime::Ref<scene::Node>> loadMeshes(const std::filesystem::path& geoDir, Statistics& statistics);

	static void loadWorld(models::GeoMap& map, const std::unordered_map<std::string, runtime::Ref<scene::Node>>& models,
		std::set<std::string>& missingMeshes, const std::filesystem::path& geoDir, Statistics& statistics);

	static runtime::Ptr<scene::Node> attachToMapAndCreateZones(models::GeoMap& map, scene::Node& node, const math::Matrix3f& matrix3f,
		const math::Vector3f& loc, const math::Vector3f& scale, Statistics& statistics);

	static runtime::Ptr<scene::Spatial> attachChild(models::GeoMap& map, scene::Spatial& node, const math::Matrix3f& matrix, const math::Vector3f& location,
		const math::Vector3f& scale);

	static void createZone(scene::Spatial& geometry, int32_t worldId, int32_t childNumber, Statistics& statistics);

public:
	/**
	 * Hash formula from paper "Optimized Spatial Hashing for Collision Detection of Deformable Objects". Hash table size is 700001. The higher
	 * value, the more precision (works most efficiently if it's a prime number). Public in C++ for the tests.
	 */
	static int32_t getVectorHash(const math::Vector3f& location);

	/**
	 * C++ only: the statistics of the last completed load (all zero before the first one). aion_game_server --check-static-data reports them after
	 * GeoService.init, which calls the Java overload (M4 item 5).
	 */
	static Statistics getLastLoadStatistics() noexcept { return lastLoadStatistics.get(); }

private:
	/** C++ only: written once by load before it returns (GameServer startup), read afterwards */
	static inline runtime::Field<Statistics> lastLoadStatistics{};
};

} // namespace aion::gameserver::geoEngine
