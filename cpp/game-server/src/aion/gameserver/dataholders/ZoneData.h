#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/ZoneData.xml.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ZoneData.
 * <p>
 * C++: the zone infos are RefCounted (ZoneInfo.h) and refer to the bound zone templates, which stay (Java sets the list to null). Weather zone
 * ids are keyed by template identity (Java: ZoneTemplate has no equals). saveData writes the zones back with JAXB marshalling: write-back comes
 * after the load path (static-data.md §3.6 item 7), so it stays unported.
 *
 * @author ATracer
 */
class ZoneData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ZoneData.xml.inc"
private:
	std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::zone::ZoneInfo>>> zoneNameMap;
	std::unordered_map<const model::templates::zone::ZoneTemplate*, int32_t> weatherZoneIds;
	int32_t count = 0;

public:
	/** C++ only: out of line, where the RefCounted element types are complete */
	ZoneData();
	~ZoneData();

	const std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::zone::ZoneInfo>>>& getZones() const;

	int32_t size() const;

	/** Weather zone ID it's an order number (starts from 1) */
	int32_t getWeatherZoneId(const model::templates::zone::ZoneTemplate& template_) const;

	/** Java: writes ./data/static_data/zones/generated_zones.xml with JAXB. Unported: write-back */
	void saveData();
};

} // namespace aion::gameserver::dataholders
