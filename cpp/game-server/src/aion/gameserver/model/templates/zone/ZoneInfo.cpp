#include "aion/gameserver/model/templates/zone/ZoneInfo.h"

#include "aion/gameserver/model/geometry/Area.h"

namespace aion::gameserver::model::templates::zone {

ZoneInfo::ZoneInfo(geometry::Area& areaValue, const ZoneTemplate* zoneTemplateValue) : area(areaValue), zoneTemplate(zoneTemplateValue) {
}

ZoneInfo::~ZoneInfo() = default;

runtime::Ref<ZoneInfo> ZoneInfo::create(geometry::Area& areaValue, const ZoneTemplate* zoneTemplateValue) {
	return runtime::makeRef<ZoneInfo>(areaValue, zoneTemplateValue);
}

} // namespace aion::gameserver::model::templates::zone
