#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/portal/fwd.h"

namespace aion::gameserver::model::templates::portal {

/**
 * Java com.aionemu.gameserver.model.templates.portal.PortalItem.
 * <p>
 * C++: a JAXB class no static data root reaches (xmlgen-report.md, unreachable types), so it is a plain K5 class (fieldmap) without a binder.
 *
 * @author AionChs Master, Schattenlilie
 */
class PortalItem {
protected:
	int32_t id = 0;
	int32_t itemid = 0;
	int32_t quantity = 0;

public:
	/** @return the id */
	int32_t getId() const { return id; }

	/** @return the itemid */
	int32_t getItemid() const { return itemid; }

	/** @return the quantity */
	int32_t getQuantity() const { return quantity; }
};

} // namespace aion::gameserver::model::templates::portal
