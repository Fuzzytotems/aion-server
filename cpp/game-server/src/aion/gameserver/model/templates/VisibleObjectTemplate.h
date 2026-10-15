#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/VisibleObjectTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates {

/**
 * Java com.aionemu.gameserver.model.templates.VisibleObjectTemplate.
 * <p>
 * C++: implements templates::L10n (hub-headers.md §9.2). The methods are const like L10n: implementors are static data reached through
 * `const X*` (and the RefCounted PlayerCommonData).
 *
 * @author ATracer
 */
class VisibleObjectTemplate : public ::aion::gameserver::runtime::StaticTemplate, public L10n {
#include "aion/gameserver/model/templates/VisibleObjectTemplate.xml.inc"
public:
	/**
	 * For Npcs it will return npcid from templates xml
	 *
	 * @return id of object template
	 */
	virtual int32_t getTemplateId() const = 0;

	/**
	 * For Npcs it will return name from templates xml
	 *
	 * @return name of object (Java null of AbstractHouseObject is the empty string)
	 */
	virtual std::string getName() const = 0;

	/** @return BoundRadius::DEFAULT unless a subclass has its own */
	virtual const BoundRadius* getBoundRadius() const;
};

} // namespace aion::gameserver::model::templates
