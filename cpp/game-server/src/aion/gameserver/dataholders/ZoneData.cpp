#include "aion/gameserver/dataholders/ZoneData.h"

#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/geometry/CylinderArea.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/geometry/SemisphereArea.h"
#include "aion/gameserver/model/geometry/SphereArea.h"
#include "aion/gameserver/model/templates/zone/AreaType.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dataholders {

using model::templates::zone::AreaType;
using model::templates::zone::ZoneInfo;
using model::templates::zone::ZoneTemplate;

namespace {

/** Java unboxing of a Float attribute: NullPointerException for null */
float unbox(const std::optional<float>& value, const char* name) {
	if (!value)
		throw runtime::NullPointerException(std::string("Cannot invoke \"java.lang.Float.floatValue()\" because the return value of \"") + name +
		                                    "()\" is null");
	return *value;
}

template <class T>
const T& deref(const T* value, const char* name) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string("Cannot invoke a method because the return value of \"ZoneTemplate.") + name + "()\" is null");
	return *value;
}

} // namespace

ZoneData::ZoneData() = default;

ZoneData::~ZoneData() = default;

void ZoneData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// the hook creates RefCounted run-time objects (and SpawnsData writes collection shims): a nested TaskScope when the load runs in one
	// (DataManager), an own one when a holder is bound alone (tests binding templates outside a scope)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	int32_t lastMapId = 0;
	int32_t weatherZoneId = 1;
	for (const ZoneTemplate& zone : zoneList) {
		runtime::Ref<model::geometry::Area> area;
		switch (zone.getAreaType()) {
			case AreaType::POLYGON: {
				const auto& points = deref(zone.getPoints(), "getPoints");
				area = model::geometry::PolyArea::create(zone.getName(), zone.getMapid(), points.getPoint(), points.getBottom(), points.getTop());
				break;
			}
			case AreaType::CYLINDER: {
				const auto& cylinder = deref(zone.getCylinder(), "getCylinder");
				area = model::geometry::CylinderArea::create(zone.getName(), zone.getMapid(), unbox(cylinder.getX(), "getX"), unbox(cylinder.getY(), "getY"),
				                                             unbox(cylinder.getR(), "getR"), unbox(cylinder.getBottom(), "getBottom"),
				                                             unbox(cylinder.getTop(), "getTop"));
				break;
			}
			case AreaType::SPHERE: {
				const auto& sphere = deref(zone.getSphere(), "getSphere");
				if (unbox(sphere.getR(), "getR") <= 0)
					break;
				area = model::geometry::SphereArea::create(zone.getName(), zone.getMapid(), unbox(sphere.getX(), "getX"), unbox(sphere.getY(), "getY"),
				                                           unbox(sphere.getZ(), "getZ"), unbox(sphere.getR(), "getR"));
				break;
			}
			case AreaType::SEMISPHERE: {
				const auto& semisphere = deref(zone.getSemisphere(), "getSemisphere");
				area =
				  model::geometry::SemisphereArea::create(zone.getName(), zone.getMapid(), unbox(semisphere.getX(), "getX"), unbox(semisphere.getY(), "getY"),
				                                          unbox(semisphere.getZ(), "getZ"), unbox(semisphere.getR(), "getR"));
				break;
			}
		}
		if (area) {
			std::vector<runtime::Ref<ZoneInfo>>& zones = zoneNameMap[zone.getMapid()];
			if (zone.getZoneType() == model::templates::zone::ZoneClassName::WEATHER) {
				if (lastMapId != zone.getMapid()) {
					lastMapId = zone.getMapid();
					weatherZoneId = 1;
				}
				weatherZoneIds.insert_or_assign(&zone, weatherZoneId++);
			}
			zones.push_back(ZoneInfo::create(*area, &zone));
			count++;
		}
	}
	// Java: zoneList = null (the zone infos refer to the bound templates, which stay)
}

const std::unordered_map<int32_t, std::vector<runtime::Ref<ZoneInfo>>>& ZoneData::getZones() const {
	return zoneNameMap;
}

int32_t ZoneData::size() const {
	return count;
}

int32_t ZoneData::getWeatherZoneId(const ZoneTemplate& template_) const {
	auto it = weatherZoneIds.find(&template_);
	return it != weatherZoneIds.end() ? it->second : 0;
}

void ZoneData::saveData() {
	AION_UNPORTED(); // write-back of the zone templates with JAXB (static-data.md §3.6 item 7)
}

} // namespace aion::gameserver::dataholders
