#include "aion/gameserver/dataholders/ZoneData.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/templates/zone/Cylinder.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/Semisphere.h"
#include "aion/gameserver/model/templates/zone/Sphere.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
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

namespace {

/** JAXB's attribute value escaping */
std::string escaped(std::string_view value) {
	std::string out;
	for (char c : value) {
		switch (c) {
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '"': out += "&quot;"; break;
			default: out += c;
		}
	}
	return out;
}

void attribute(std::string& out, std::string_view name, std::string_view value) {
	out += ' ';
	out += name;
	out += "=\"";
	out += escaped(value);
	out += '"';
}

void floatAttribute(std::string& out, std::string_view name, float value) {
	attribute(out, name, geoEngine::math::JavaFloat::toString(value));
}

/** an optional Float field: JAXB writes no attribute for null */
void floatAttribute(std::string& out, std::string_view name, const std::optional<float>& value) {
	if (value)
		floatAttribute(out, name, *value);
}

void writeSphere(std::string& out, const char* element, const model::templates::zone::Sphere& sphere) {
	out += "        <";
	out += element;
	floatAttribute(out, "x", sphere.getX());
	floatAttribute(out, "y", sphere.getY());
	floatAttribute(out, "z", sphere.getZ());
	floatAttribute(out, "r", sphere.getR());
	out += "/>\n";
}

/**
 * Java: the JAXB marshalling of ZoneData (JAXB_FORMATTED_OUTPUT, UTF-8), in the JAXB RI's formatted form, as SpawnsData's saveSpawn writes its
 * files: the XML declaration with standalone="yes", four spaces per level, empty elements self-closed. A zone's attributes come in the RI's
 * property order of ZoneTemplate (XmlAccessType.NONE, no propOrder): its annotated fields in declaration order (flags, priority, mapid, siege_id,
 * town_id, area_type, zone_type), then the `name` property of getXmlName/setXmlName; a null siege_id is no attribute, the int fields always are.
 * Then the shape elements in field order (points, cylinder, sphere, semisphere), each with its attributes in field order (Points: top, bottom,
 * then a <point y x> per point; Cylinder: top, bottom, x, y, r; Sphere and Semisphere: x, y, z, r), a null Float field being no attribute. A
 * MaterialZoneTemplate is written as the ZoneTemplate it extends: JAXBContext.newInstance(ZoneData.class) knows no subclass.
 */
std::string serializeZones(std::span<const model::templates::zone::ZoneTemplate* const> zones) {
	std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	if (zones.empty())
		return out + "<zones/>\n";
	out += "<zones>\n";
	for (const model::templates::zone::ZoneTemplate* zone : zones) {
		out += "    <zone";
		attribute(out, "flags", std::to_string(zone->getFlags()));
		attribute(out, "priority", std::to_string(zone->getPriority()));
		attribute(out, "mapid", std::to_string(zone->getMapid()));
		if (zone->getSiegeId()) {
			std::string ids;
			for (int32_t id : *zone->getSiegeId()) {
				if (!ids.empty())
					ids += ' ';
				ids += std::to_string(id);
			}
			attribute(out, "siege_id", ids);
		}
		attribute(out, "town_id", std::to_string(zone->getTownId()));
		attribute(out, "area_type", xml::enumName(zone->getAreaType()));
		attribute(out, "zone_type", xml::enumName(zone->getZoneType()));
		attribute(out, "name", zone->getXmlName());
		const model::templates::zone::Points* points = zone->getPoints();
		if (!points && !zone->getCylinder() && !zone->getSphere() && !zone->getSemisphere()) {
			out += "/>\n";
			continue;
		}
		out += ">\n";
		if (points) {
			out += "        <points";
			floatAttribute(out, "top", points->getTop());
			floatAttribute(out, "bottom", points->getBottom());
			if (points->getPoint().empty()) {
				out += "/>\n";
			} else {
				out += ">\n";
				for (const model::templates::zone::Point2D& point : points->getPoint()) {
					out += "            <point";
					floatAttribute(out, "y", point.getY());
					floatAttribute(out, "x", point.getX());
					out += "/>\n";
				}
				out += "        </points>\n";
			}
		}
		if (const model::templates::zone::Cylinder* cylinder = zone->getCylinder()) {
			out += "        <cylinder";
			floatAttribute(out, "top", cylinder->getTop());
			floatAttribute(out, "bottom", cylinder->getBottom());
			floatAttribute(out, "x", cylinder->getX());
			floatAttribute(out, "y", cylinder->getY());
			floatAttribute(out, "r", cylinder->getR());
			out += "/>\n";
		}
		if (const model::templates::zone::Sphere* sphere = zone->getSphere())
			writeSphere(out, "sphere", *sphere);
		if (const model::templates::zone::Semisphere* semisphere = zone->getSemisphere())
			writeSphere(out, "semisphere", *semisphere);
		out += "    </zone>\n";
	}
	return out + "</zones>\n";
}

} // namespace

// Java ZoneData.java:107-119
void ZoneData::saveData() {
	std::vector<const ZoneTemplate*> zones;
	zones.reserve(zoneList.size());
	for (const ZoneTemplate& zone : zoneList)
		zones.push_back(&zone);
	saveData(zones);
}

void ZoneData::saveData(std::span<const ZoneTemplate* const> zones) {
	const std::filesystem::path xml("./data/static_data/zones/generated_zones.xml");
	// Java: Marshaller.marshal(this, xml) with zones.xsd as its schema. The C++ writes the same bytes without the schema check: the templates
	// come from the validated load or from createMaterialZoneTemplate. The RI wraps a FileNotFoundException in MarshalException(Throwable),
	// whose message is null, and saveData logs it as "Error while saving data: null" with the I/O exception as its cause, then returns
	const std::string text = serializeZones(zones);
	std::ofstream out(xml, std::ios::binary | std::ios::trunc);
	if (out)
		out << text;
	if (!out || !out.flush()) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.ZoneData")
			.error("Error while saving data: null", commons::utils::IOException(xml.string() + " (cannot be written)"));
	}
}

} // namespace aion::gameserver::dataholders
