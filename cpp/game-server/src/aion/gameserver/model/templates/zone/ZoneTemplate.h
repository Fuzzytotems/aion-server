#pragma once

#include <atomic>
#include <string>

#include "aion/gameserver/model/templates/zone/ZoneTemplate.xml.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::templates::zone {

/**
 * Java com.aionemu.gameserver.model.templates.zone.ZoneTemplate.
 * <p>
 * C++ (upstream sync foundation, docs/deviations/P4-07a.md): upstream 4617c9fd2 removed ZoneName and binds the name attribute directly
 * (getName returns the String). The C++ callers still use the interned ZoneName until wave 4 ports 4617c9fd2, so getZoneName interns the bound
 * name on first use (Java 51d7a19cb setXmlName interned it while binding) and getXmlName stays as the former String accessor.
 *
 * @author ATracer
 */
class ZoneTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/ZoneTemplate.xml.inc"
public:
	/** C++ only: the former Java getXmlName (the bound name, empty before the name attribute was bound; Java null) */
	const std::string& getXmlName() const { return name; }

	/** C++ only: the former Java getName (51d7a19cb). @return the interned zone name, nullptr while no name is bound (Java null) */
	const ::aion::gameserver::world::zone::ZoneName* getZoneName() const;

	bool hasZoneAttribute(::aion::gameserver::world::zone::ZoneAttributes attribute) const;

	/** C++ only: true for the WorldZoneTemplate of a whole map (its 51d7a19cb zone type DUMMY, see WorldZoneTemplate.h) */
	bool isWholeMapZone() const { return wholeMapZone; }

protected:
	/** C++ only: set by the WorldZoneTemplate constructor */
	bool wholeMapZone = false;

private:
	/** the interned ZoneName of `name`, computed by getZoneName; copyable so the bound vector can relocate its templates while it is filled */
	struct ZoneNameCache {
		mutable std::atomic<const ::aion::gameserver::world::zone::ZoneName*> value{nullptr};
		ZoneNameCache() = default;
		ZoneNameCache(const ZoneNameCache& other) : value(other.value.load(std::memory_order_relaxed)) {}
		ZoneNameCache& operator=(const ZoneNameCache& other) {
			value.store(other.value.load(std::memory_order_relaxed), std::memory_order_relaxed);
			return *this;
		}
	};
	ZoneNameCache zoneName;
};

} // namespace aion::gameserver::model::templates::zone
