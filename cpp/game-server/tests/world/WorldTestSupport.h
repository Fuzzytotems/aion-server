#pragma once

// Test support of the world chunk (P4-10): small static data holders published once per process, and a synthetic visible object whose
// controller records the known list notifications.

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::world::test {

inline constexpr int32_t POETA = 210010000;    // 2D map, 1024 x 1024, with zones
inline constexpr int32_t RESHANTA = 400010000; // 3D map, 512 x 512
inline constexpr int32_t DREDGION = 300110000; // instance map
inline constexpr int32_t ISHALGEN = 220010000; // 2 twins + 1 beginner twin

/** world_maps.xml of the test maps (the real attribute set of world_maps.xml) */
inline const char* const WORLD_MAPS_XML = R"(<world_maps>
	<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0" world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>
	<map id="400010000" cName="AB1" name="Reshanta" name_id="2" water_level="16" death_level="0" world_type="ABYSS" world_size="512" flags="FLY"/>
	<map id="300110000" cName="IDAB1" name="Dredgion" name_id="3" water_level="16" death_level="0" world_size="256" instance="true" flags="BIND"/>
	<map id="220010000" cName="DF1" name="Ishalgen" name_id="4" water_level="16" death_level="0" world_size="256" twin_count="2" beginner_twin_count="1" flags="RIDE"/>
</world_maps>)";

/**
 * zones of Poeta: a FLY polygon (100..300, 100..300, z 0..200), a PVP cylinder around (600, 600) with radius 50 (z 0..100), a SUB sphere around
 * (140, 140, 50) with radius 20 and priority 5, and a second SUB sphere with priority 0 at the same place
 */
inline const char* const ZONES_XML = R"(<zones>
	<zone mapid="210010000" name="FLY_AREA_210010000" area_type="POLYGON" zone_type="FLY" flags="8">
		<points bottom="0.0" top="200.0">
			<point x="100.0" y="100.0"/>
			<point x="300.0" y="100.0"/>
			<point x="300.0" y="300.0"/>
			<point x="100.0" y="300.0"/>
		</points>
	</zone>
	<zone mapid="210010000" name="PVP_AREA_210010000" area_type="CYLINDER" zone_type="PVP" flags="64">
		<cylinder bottom="0" top="100" x="600" y="600" r="50"/>
	</zone>
	<zone mapid="210010000" name="SUB_PRIORITY_210010000" area_type="SPHERE" zone_type="SUB" priority="5">
		<sphere x="140" y="140" z="50" r="20"/>
	</zone>
	<zone mapid="210010000" name="SUB_PLAIN_210010000" area_type="SPHERE" zone_type="SUB">
		<sphere x="140" y="140" z="50" r="20"/>
	</zone>
</zones>)";

/** The static data set this process published (holders are published once per process): none, the test holders or the real data. */
enum class PublishedData { NONE, TEST, REAL };
inline std::mutex publishedDataMutex;
inline PublishedData publishedData = PublishedData::NONE;

/**
 * Publishes the test holders (world maps, zones, no shields, no materials) and the world configuration once per process. Tests that use
 * World::getInstance(), ZoneService::getInstance() or WorldMapInstance::regionSize() call it first: those singletons read the data once.
 *
 * @return false if the process already published the real data (WorldRealDataTest ran first in this process): the caller skips
 */
inline bool publishTestStaticData() {
	std::scoped_lock lock(publishedDataMutex);
	if (publishedData == PublishedData::REAL)
		return false;
	if (publishedData == PublishedData::NONE) {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(0);
		configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(0);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		xml::LoadContext context;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(context, WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(context, ZONES_XML));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(context, "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(context, "<material_templates/>"));
		publishedData = PublishedData::TEST;
	}
	return true;
}

/** Records the known list notifications of its owner. */
class RecordingController final : public controllers::VisibleObjectController {
public:
	std::atomic<int32_t> seen{0};
	std::atomic<int32_t> notSeen{0};
	std::atomic<int32_t> notKnown{0};

	/** Notification kinds passed to the hook. */
	enum class Event { SEE, NOT_SEE, NOT_KNOW };
	/** Called on the notifying thread after the counter update; set before other threads can notify this controller. */
	std::function<void(Event, model::gameobjects::VisibleObject&)> hook;

	RecordingController() = default;

	void see(model::gameobjects::VisibleObject& object) override {
		seen.fetch_add(1);
		if (hook)
			hook(Event::SEE, object);
	}

	void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation) override {
		notSeen.fetch_add(1);
		if (hook)
			hook(Event::NOT_SEE, object);
	}

	void notKnow(model::gameobjects::VisibleObject& object) override {
		notKnown.fetch_add(1);
		if (hook)
			hook(Event::NOT_KNOW, object);
	}

	void onBeforeSpawn() override {}

	void onDespawn() override {}
};

/** A synthetic visible object with a plain KnownList (visible distance 95 m, no template, no spawn). */
class TestObject final : public model::gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND
public:
	/** Java: new X(objectId, controller, spawn, template, new WorldPosition(mapId), false), the way SpawnEngine creates visible objects */
	TestObject(CreateKey key, int32_t objectId, int32_t mapId, float visibleDistance)
		: VisibleObject(key, objectId, std::make_unique<RecordingController>(), nullptr, nullptr, WorldPosition::create(mapId), false),
		  visibleDistance(visibleDistance) {}

	std::string getName() override { return "TestObject" + std::to_string(getObjectId()); }

	float getVisibleDistance() override { return visibleDistance; }

	RecordingController& recorder() { return static_cast<RecordingController&>(getController()); }

protected:
	void postConstruct() override {
		VisibleObject::postConstruct();
		getController().setOwner(*this);
		setKnownlist(std::make_unique<knownlist::KnownList>(*this));
	}

	~TestObject() override = default;

private:
	const float visibleDistance;
};

} // namespace aion::gameserver::world::test
