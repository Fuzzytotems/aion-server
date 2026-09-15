#include "aion/gameserver/geoEngine/GeoWorldLoader.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/models/Terrain.h"
#include "aion/gameserver/geoEngine/scene/CloneNotSupportedException.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode_DespawnableTypeInfo.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/geoEngine/utils/PngImage.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"

namespace aion::gameserver::geoEngine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.GeoWorldLoader");

using commons::utils::ByteBuffer;
using commons::utils::ByteOrder;
using commons::utils::IOException;
using scene::DespawnableNode;
using NodeMap = std::unordered_map<std::string, runtime::Ref<scene::Node>>;

namespace {

std::vector<uint8_t> readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary | std::ios::ate);
	if (!in)
		throw IOException(file.string());
	// one read of the whole file (models.mesh has 70 MB; a stream iterator reads byte by byte)
	std::streamoff size = in.tellg();
	std::vector<uint8_t> bytes(size > 0 ? static_cast<size_t>(size) : 0);
	in.seekg(0);
	if (!bytes.empty() && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw IOException(file.string());
	return bytes;
}

/** Java: String.split(regexOfOneLiteralCharacter) - trailing empty strings are removed */
std::vector<std::string> split(std::string_view text, char separator) {
	std::vector<std::string> parts;
	size_t start = 0;
	for (;;) {
		size_t end = text.find(separator, start);
		parts.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		if (end == std::string_view::npos)
			break;
		start = end + 1;
	}
	while (!parts.empty() && parts.back().empty())
		parts.pop_back();
	if (parts.empty() && text.empty())
		parts.emplace_back(); // Java: "".split(",") is [""]
	return parts;
}

/** Java: String.lastIndexOf(char) for an ASCII character, as a UTF-16 index (-1 if absent) */
int32_t lastIndexOf(std::string_view text, char c) {
	size_t position = text.rfind(c);
	return position == std::string_view::npos ? -1 : commons::utils::StringUtils::utf16Length(text.substr(0, position));
}

/** Java: String.replace(CharSequence, CharSequence) - every occurrence, left to right */
std::string replaceAll(std::string_view text, std::string_view target, std::string_view replacement) {
	std::string result;
	size_t start = 0;
	for (size_t found; (found = text.find(target, start)) != std::string_view::npos; start = found + target.size()) {
		result.append(text.substr(start, found - start));
		result.append(replacement);
	}
	result.append(text.substr(start));
	return result;
}

/** Java: DataManager.MATERIAL_DATA.getTemplate(materialId) != null (NullPointerException while the material data is not published) */
bool hasMaterialTemplate(int32_t materialId) {
	return dataholders::DataManager::MATERIAL_DATA->getTemplate(materialId) != nullptr;
}

/** The decoded terrain PNG of one file and the maps it belongs to. */
struct TerrainImage {
	std::filesystem::path path;
	std::vector<runtime::Ptr<models::GeoMap>> maps;
	std::vector<std::string> unassociatedMapIds;
	std::optional<utils::PngImage> image;
};

} // namespace

void GeoWorldLoader::load(const std::vector<runtime::Ptr<models::GeoMap>>& maps) {
	static_cast<void>(load(maps, GEO_DIR));
}

GeoWorldLoader::Statistics GeoWorldLoader::load(const std::vector<runtime::Ptr<models::GeoMap>>& maps, const std::filesystem::path& geoDir) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	Statistics statistics;
	loadTerrains(maps, geoDir, statistics);
	load(maps, loadMeshes(geoDir, statistics), geoDir, statistics);
	// Deviation: Java preloads the mesh collision data on the long-running pool "for responsive initial collision checks and predictable memory
	// usage" (ThreadPoolManager.executeLongRunning) while collision checks may already build the same trees lazily (Mesh.createCollisionData is
	// unsynchronized and sorts the mesh's triangles in place). C++ builds every tree of the loaded maps before load returns.
	std::vector<runtime::Ref<scene::Mesh>> meshes;
	std::unordered_set<const scene::Mesh*> seen;
	for (const runtime::Ptr<models::GeoMap>& map : maps) {
		for (const runtime::Ptr<scene::Geometry>& geometry : map->getGeometries()) {
			runtime::Ptr<scene::Mesh> mesh = geometry->getMesh();
			if (seen.insert(mesh.get()).second)
				meshes.emplace_back(mesh);
		}
	}
	runtime::ForkJoinPool::commonPool().parallelForEach(meshes, [](runtime::Ref<scene::Mesh>& mesh) { mesh->createCollisionData(); });
	statistics.collisionTrees = static_cast<int32_t>(meshes.size());
	lastLoadStatistics.set(statistics);
	return statistics;
}

void GeoWorldLoader::loadTerrains(const std::vector<runtime::Ptr<models::GeoMap>>& maps, const std::filesystem::path& geoDir, Statistics& statistics) {
	std::vector<TerrainImage> images;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(geoDir)) {
		std::string fileName = entry.path().filename().string();
		if (!entry.is_regular_file() || !commons::utils::StringUtils::toLowerCase(fileName).ends_with(".png"))
			continue;
		TerrainImage terrainImage{entry.path(), {}, {}, std::nullopt};
		std::vector<std::string> mapIds = split(fileName, ',');
		// Java: a Set of the parts (duplicates collapse); maps are matched in the order of the maps collection
		std::sort(mapIds.begin(), mapIds.end());
		mapIds.erase(std::unique(mapIds.begin(), mapIds.end()), mapIds.end());
		for (const runtime::Ptr<models::GeoMap>& map : maps) {
			if (mapIds.empty())
				break;
			std::string prefix = std::to_string(map->getMapId());
			if (std::erase_if(mapIds, [&](const std::string& mapId) { return mapId.starts_with(prefix); }) > 0)
				terrainImage.maps.push_back(map);
		}
		terrainImage.unassociatedMapIds = std::move(mapIds);
		images.push_back(std::move(terrainImage));
	}
	std::sort(images.begin(), images.end(), [](const TerrainImage& a, const TerrainImage& b) { return a.path < b.path; });

	// Java: ImageIO.read runs in the parallel stream; the images are only decoded for files that belong to a map
	runtime::ForkJoinPool::commonPool().parallelForEach(images, [](TerrainImage& terrainImage) {
		if (!terrainImage.maps.empty())
			terrainImage.image = utils::PngImage::read(terrainImage.path);
	});

	// Deviation: Java sets the heightmap and the materials of a map's Terrain from the parallel stream threads, so the size checks of
	// setHeightmap/setMaterials race with the other file's fields; here the decoded images are applied on this thread in file name order.
	// The terrains are kept in the order of the maps collection (Java: a ConcurrentHashMap keyed by map).
	std::vector<std::pair<runtime::Ptr<models::GeoMap>, runtime::Ref<models::Terrain>>> terrainByMap;
	for (TerrainImage& terrainImage : images) {
		const std::string path = terrainImage.path.string();
		for (const runtime::Ptr<models::GeoMap>& map : terrainImage.maps) {
			auto it = std::find_if(terrainByMap.begin(), terrainByMap.end(), [&](const auto& entry) { return entry.first == map; });
			if (it == terrainByMap.end()) {
				terrainByMap.emplace_back(map, models::Terrain::create());
				it = terrainByMap.end() - 1;
			}
			const utils::PngImage& image = *terrainImage.image;
			switch (image.dataBufferType) {
				case utils::PngImage::DataBufferType::USHORT:
					it->second->setHeightmap(image.shorts, image.width, image.height);
					break;
				case utils::PngImage::DataBufferType::BYTE:
					it->second->setMaterials(image.bytes, image.width, image.height);
					break;
			}
		}
		for (const std::string& mapId : terrainImage.unassociatedMapIds)
			log.warn(mapId + " of " + path + " could not be associated with a map");
	}
	for (const runtime::Ptr<models::GeoMap>& map : maps) {
		auto it = std::find_if(terrainByMap.begin(), terrainByMap.end(), [&](const auto& entry) { return entry.first == map; });
		if (it == terrainByMap.end())
			continue;
		if (it->second->hasHeightmap())
			map->setTerrain(it->second);
		else
			log.warn("Missing terrain heightmap for " + std::to_string(map->getMapId()));
	}
	int64_t terrainMapCount = std::ranges::count_if(maps, [](const runtime::Ptr<models::GeoMap>& map) { return map->hasTerrain(); });
	statistics.terrainMaps = static_cast<int32_t>(terrainMapCount);
	if (terrainMapCount == 0)
		log.warn("No terrains were loaded");
	else
		log.info("Loaded terrains for " + std::to_string(terrainMapCount) + " maps");
	if (std::ranges::none_of(maps, [](const runtime::Ptr<models::GeoMap>& map) { return map->hasTerrainMaterials(); }))
		log.warn("No terrain materials were loaded");
}

void GeoWorldLoader::load(const std::vector<runtime::Ptr<models::GeoMap>>& maps, const NodeMap& modelNodes, const std::filesystem::path& geoDir,
	Statistics& statistics) {
	// Java: a concurrent key set filled by the parallel stream; here one set and statistics object per map, merged afterwards
	std::vector<std::set<std::string>> missingMeshesByMap(maps.size());
	std::vector<Statistics> statisticsByMap(maps.size());
	std::vector<size_t> indexes(maps.size());
	for (size_t i = 0; i < indexes.size(); ++i)
		indexes[i] = i;
	runtime::ForkJoinPool::commonPool().parallelForEach(indexes, [&](size_t index) {
		loadWorld(*maps[index], modelNodes, missingMeshesByMap[index], geoDir, statisticsByMap[index]);
	});
	std::set<std::string> missingMeshes;
	for (size_t i = 0; i < maps.size(); ++i) {
		missingMeshes.insert(missingMeshesByMap[i].begin(), missingMeshesByMap[i].end());
		statistics.geoFiles += statisticsByMap[i].geoFiles;
		statistics.placements += statisticsByMap[i].placements;
		statistics.missingMeshPlacements += statisticsByMap[i].missingMeshPlacements;
		statistics.attachedNodes += statisticsByMap[i].attachedNodes;
		statistics.geometries += statisticsByMap[i].geometries;
		statistics.placementGeometries += statisticsByMap[i].placementGeometries;
		statistics.materialGeometries += statisticsByMap[i].materialGeometries;
	}
	if (!missingMeshes.empty()) {
		std::string message = std::to_string(missingMeshes.size()) + " meshes are missing:\n";
		bool first = true;
		for (const std::string& name : missingMeshes) {
			if (!first)
				message += '\n';
			message += name;
			first = false;
		}
		log.warn(message);
	}
	int64_t loadedMaps = std::ranges::count_if(maps, [](const runtime::Ptr<models::GeoMap>& map) { return !map->getChildren()->isEmpty(); });
	if (loadedMaps == 0) {
		log.warn("No geo maps loaded.");
	} else {
		int64_t entities = 0;
		for (const runtime::Ptr<models::GeoMap>& map : maps)
			entities += map->getEntityCount();
		log.info("Loaded " + std::to_string(entities) + " entities on " + std::to_string(loadedMaps) + " maps");
	}
}

NodeMap GeoWorldLoader::loadMeshes(const std::filesystem::path& geoDir, Statistics& statistics) {
	NodeMap geoms;
	try {
		std::vector<uint8_t> file = readFile(geoDir / "models.mesh");
		ByteBuffer geo = ByteBuffer::wrap(file);
		geo.order(ByteOrder::BIG_ENDIAN_ORDER);
		while (geo.hasRemaining()) {
			int16_t nameLength = geo.getShort();
			if (nameLength < 0)
				throw runtime::IllegalArgumentException("NegativeArraySizeException: " + std::to_string(nameLength));
			std::string name(static_cast<size_t>(nameLength), '\0');
			geo.get(std::span<uint8_t>(reinterpret_cast<uint8_t*>(name.data()), name.size()));
			statistics.meshEntries++;
			runtime::Ref<scene::Node> node = scene::Node::create(std::nullopt);
			int8_t intentions = 0;
			int32_t singleChildMaterialId = 0;
			int32_t modelCount = geo.get() & 0xFF;
			for (int32_t c = 0; c < modelCount; c++) {
				runtime::Ref<scene::Mesh> m = scene::Mesh::create();
				statistics.meshes++;

				int32_t vertices = geo.getShort() & 0xFFFF;
				int32_t verticesBytes = vertices * 3 * 4; // 3 floats per vertex (x, y, z), 4 bytes each
				ByteBuffer vertexSlice = geo.slice(geo.position(), verticesBytes);
				std::vector<float> vertexValues(static_cast<size_t>(vertices) * 3);
				for (size_t i = 0; i < vertexValues.size(); ++i)
					vertexValues[i] = vertexSlice.getFloat(static_cast<int32_t>(i * 4));
				m->setVertices(vertexValues);
				geo.position(geo.position() + verticesBytes);

				int32_t faces = geo.getShort() & 0xFFFF;
				int8_t indexSize = geo.get();
				int32_t facesBytes = faces * 3 * indexSize; // 3 vertex indices per face, `indexSize` bytes each
				switch (indexSize) {
					case 1: {
						ByteBuffer indexSlice = geo.slice(geo.position(), facesBytes);
						std::vector<int8_t> indexValues(static_cast<size_t>(facesBytes));
						for (size_t i = 0; i < indexValues.size(); ++i)
							indexValues[i] = indexSlice.get(static_cast<int32_t>(i));
						m->setIndices(std::span<const int8_t>(indexValues));
						break;
					}
					case 2: {
						ByteBuffer indexSlice = geo.slice(geo.position(), facesBytes);
						std::vector<int16_t> indexValues(static_cast<size_t>(faces) * 3);
						for (size_t i = 0; i < indexValues.size(); ++i)
							indexValues[i] = indexSlice.getShort(static_cast<int32_t>(i * 2));
						m->setIndices(std::span<const int16_t>(indexValues));
						break;
					}
					default:
						throw IOException("Index size " + std::to_string(indexSize) + " is not supported");
				}
				geo.position(geo.position() + facesBytes);

				m->setMaterialId(geo.get());
				m->setCollisionIntentions(geo.get());
				intentions |= m->getCollisionIntentions();
				if (node->getName().empty() && (m->getMaterialId() == 11 || hasMaterialTemplate(m->getMaterialId())))
					node->setName(std::string_view(name));
				if (modelCount == 1)
					singleChildMaterialId = m->getMaterialId();
				node->attachChild(scene::Geometry::create(name, m));
			}
			node->setCollisionIntentions(intentions);
			node->setMaterialId(static_cast<int8_t>(singleChildMaterialId));
			if (name.find('|') == std::string::npos) {
				geoms.insert_or_assign(name, node);
			} else {
				for (const std::string& n : split(name, '|')) {
					runtime::Ref<scene::Spatial> spatialClone = node->clone();
					runtime::Ptr<scene::Node> clone = runtime::cast<scene::Node>(spatialClone);
					if (!clone->getName().empty())
						clone->setName(std::string_view(n));
					clone->getChild(std::string_view(name))->setName(std::string_view(n));
					geoms.insert_or_assign(n, runtime::Ref<scene::Node>(clone));
				}
			}
		}
	} catch (const IOException&) {
		// Java: GameServerError (CONVENTIONS: fatal startup errors are commons Exceptions; GameServerError.h, P5-14, does not exist yet)
		throw commons::utils::Exception("Could not load meshes", std::current_exception());
	} catch (const scene::CloneNotSupportedException&) {
		throw commons::utils::Exception("Could not load meshes", std::current_exception()); // Java: GameServerError
	}
	statistics.meshNames = static_cast<int32_t>(geoms.size());
	log.info("Loaded " + std::to_string(geoms.size()) + " meshes");
	return geoms;
}

void GeoWorldLoader::loadWorld(models::GeoMap& map, const NodeMap& modelNodes, std::set<std::string>& missingMeshes, const std::filesystem::path& geoDir,
	Statistics& statistics) {
	std::filesystem::path geoFile = geoDir / (std::to_string(map.getMapId()) + ".geo");
	std::error_code error;
	if (!std::filesystem::is_regular_file(geoFile, error)) {
		const model::templates::world::WorldMapTemplate* worldTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(map.getMapId());
		if (worldTemplate == nullptr)
			throw runtime::NullPointerException("No world map template for " + std::to_string(map.getMapId()));
		bool shouldHaveEntities = worldTemplate->getWorldSize() != 0 && !worldTemplate->isPrison() &&
								  !commons::utils::StringUtils::equalsIgnoreCase(worldTemplate->getName(), "IDTest_Dungeon") &&
								  !commons::utils::StringUtils::equalsIgnoreCase(worldTemplate->getName(), "System_Basic");
		if (shouldHaveEntities)
			log.warn(geoFile.string() + " is missing");
		return;
	}
	try {
		std::vector<uint8_t> file = readFile(geoFile);
		statistics.geoFiles++;
		ByteBuffer geo = ByteBuffer::wrap(file);
		geo.order(ByteOrder::BIG_ENDIAN_ORDER);
		while (geo.hasRemaining()) {
			int32_t nameLength = geo.getShort();
			if (nameLength < 0)
				throw runtime::IllegalArgumentException("NegativeArraySizeException: " + std::to_string(nameLength));
			std::string name(static_cast<size_t>(nameLength), '\0');
			geo.get(std::span<uint8_t>(reinterpret_cast<uint8_t*>(name.data()), name.size()));
			float locX = geo.getFloat();
			float locY = geo.getFloat();
			float locZ = geo.getFloat();
			math::Vector3f loc(locX, locY, locZ);
			math::Matrix3f matrix3f;
			for (int32_t i = 0; i < 3; i++)
				for (int32_t j = 0; j < 3; j++)
					matrix3f.set(i, j, geo.getFloat());
			float scaleX = geo.getFloat();
			float scaleY = geo.getFloat();
			float scaleZ = geo.getFloat();
			math::Vector3f scale(scaleX, scaleY, scaleZ);
			int8_t type = geo.get();
			int16_t id = geo.getShort();
			int8_t level = geo.get();
			statistics.placements++;
			auto model = modelNodes.find(name);
			if (model != modelNodes.end()) {
				runtime::Ref<scene::Node> node = model->second;
				if (type > 0) {
					runtime::Ref<DespawnableNode> despawnableNode = DespawnableNode::create();
					despawnableNode->copyFrom(*node);
					despawnableNode->type.set(scene::getById(type));
					despawnableNode->id.set(id);
					if (despawnableNode->type.get() == DespawnableNode::DespawnableType::TOWN_OBJECT) {
						if (level > 8)
							throw runtime::IllegalArgumentException(std::to_string(level) + " doesn't fit in bit mask");
						despawnableNode->levelBitMask.set(level < 1 ? 0 : static_cast<int8_t>(1 << (level - 1)));
					} else if (level != 0) {
						throw runtime::IllegalArgumentException("Unexpected value in town level field for non-town entity");
					}
					node = std::move(despawnableNode);
				}
				runtime::Ptr<scene::Node> nodeClone = attachToMapAndCreateZones(map, *node, matrix3f, loc, scale, statistics);
				statistics.placementGeometries += nodeClone->getQuantity();
				runtime::Ptr<DespawnableNode> townEntity = runtime::as<DespawnableNode>(nodeClone);
				if (townEntity != nullptr && townEntity->type.get() == DespawnableNode::DespawnableType::TOWN_OBJECT) {
					// replicate client logic: find .cgfs for higher town levels or reuse current one
					for (int32_t townLevel = level + 1; townLevel <= 5; townLevel++) {
						std::string townEntityName = replaceAll(name, "_01.cgf", "_0" + std::to_string(townLevel) + ".cgf");
						auto townModel = modelNodes.find(townEntityName);
						if (townModel == modelNodes.end()) {
							townEntity->levelBitMask.set(static_cast<int8_t>(townEntity->levelBitMask.get() | static_cast<int8_t>(1 << ((townLevel - 1) & 31))));
						} else {
							runtime::Ref<DespawnableNode> townNode = DespawnableNode::create();
							townNode->copyFrom(*townModel->second);
							townNode->type.set(townEntity->type.get());
							townNode->id.set(townEntity->id.get());
							townNode->levelBitMask.set(static_cast<int8_t>(1 << ((townLevel - 1) & 31)));
							townEntity = runtime::cast<DespawnableNode>(attachToMapAndCreateZones(map, *townNode, matrix3f, loc, scale, statistics));
						}
					}
				}
			} else {
				statistics.missingMeshPlacements++;
				missingMeshes.insert(name);
			}
		}
	} catch (...) {
		throw commons::utils::Exception("Could not load " + geoFile.string(), std::current_exception()); // Java: GameServerError
	}
	map.updateModelBound();
}

runtime::Ptr<scene::Node> GeoWorldLoader::attachToMapAndCreateZones(models::GeoMap& map, scene::Node& node, const math::Matrix3f& matrix3f,
	const math::Vector3f& loc, const math::Vector3f& scale, Statistics& statistics) {
	runtime::Ptr<scene::Node> nodeClone = runtime::cast<scene::Node>(attachChild(map, node, matrix3f, loc, scale));
	statistics.attachedNodes++;
	std::vector<runtime::Ptr<scene::Spatial>> children = nodeClone->getChildren()->snapshot();
	for (size_t c = 0; c < children.size(); c++) {
		if (runtime::as<scene::Geometry>(children[c]) != nullptr)
			statistics.geometries++;
		createZone(*children[c], map.getMapId(), children.size() == 1 ? 0 : static_cast<int32_t>(c + 1), statistics);
	}
	return nodeClone;
}

runtime::Ptr<scene::Spatial> GeoWorldLoader::attachChild(models::GeoMap& map, scene::Spatial& node, const math::Matrix3f& matrix,
	const math::Vector3f& location, const math::Vector3f& scale) {
	runtime::Ref<scene::Spatial> nodeClone = node.clone();
	nodeClone->setTransform(matrix, location, scale);
	nodeClone->updateModelBound();
	map.attachChild(nodeClone);
	return nodeClone; // held by the map's chunk node
}

void GeoWorldLoader::createZone(scene::Spatial& geometry, int32_t worldId, int32_t childNumber, Statistics& statistics) {
	if ((geometry.getCollisionIntentions() & collision::getId(collision::CollisionIntention::MATERIAL)) != 0) {
		int32_t regionId = getVectorHash(geometry.getWorldBound()->getCenter());
		const std::string geometryName = geometry.getName();
		int32_t index = lastIndexOf(geometryName, '/');
		int32_t dotIndex = lastIndexOf(geometryName, '.');
		std::string name = commons::utils::StringUtils::toUpperCase(commons::utils::StringUtils::substring(geometryName, index + 1, dotIndex));
		if (childNumber > 0)
			name += "_CHILD" + std::to_string(childNumber);
		geometry.setName(std::string_view(name + "_" + std::to_string(regionId)));
		statistics.materialGeometries++;
		GeoCallbacks::createMaterialZone(geometry, worldId, geometry.getName() + "_" + std::to_string(worldId));
	}
}

int32_t GeoWorldLoader::getVectorHash(const math::Vector3f& location) {
	int64_t xIntBits = math::JavaFloat::floatToIntBits(location.x);
	int64_t yIntBits = math::JavaFloat::floatToIntBits(location.y);
	int64_t zIntBits = math::JavaFloat::floatToIntBits(location.z);
	return static_cast<int32_t>(((xIntBits * 73856093) ^ (yIntBits * 19349669) ^ (zIntBits * 83492791)) % 700001);
}

} // namespace aion::gameserver::geoEngine
