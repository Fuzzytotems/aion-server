#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"

namespace aion::gameserver::model::templates::zone {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Not static data despite its package: a zone's area and template built by ZoneService
 * (fieldmap K3, `ZoneInstance.template`, `ZoneService.zoneByMapIdMap`), RefCounted, created with create(). The area is an interface held by Ref
 * (Area, hub-headers.md §9.2).
 *
 * @author MrPoke
 */
class ZoneInfo : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<geometry::Area> area;
	const ZoneTemplate* zoneTemplate;

protected:
	ZoneInfo(geometry::Area& area, const ZoneTemplate* zoneTemplate);
	~ZoneInfo() override;

public:
	/** Java: new ZoneInfo(area, zoneTemplate) */
	static runtime::Ref<ZoneInfo> create(geometry::Area& area, const ZoneTemplate* zoneTemplate);

	/** @return the area */
	runtime::Ptr<geometry::Area> getArea() const { return area; }

	/** @return the zoneTemplate */
	const ZoneTemplate* getZoneTemplate() const { return zoneTemplate; }
};

} // namespace aion::gameserver::model::templates::zone
