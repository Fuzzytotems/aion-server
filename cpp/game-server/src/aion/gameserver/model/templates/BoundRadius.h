#pragma once

#include "aion/gameserver/model/templates/BoundRadius.xml.h"

namespace aion::gameserver::model::templates {

/**
 * Java com.aionemu.gameserver.model.templates.BoundRadius.
 * <p>
 * C++: an immortal K1 template. Java also creates one per PlayerAccountData.updateBoundingRadius call (`new BoundRadius(0.25f, 0.25f,
 * appearance.getBoundHeight())`), which PlayerCommonData keeps as `const BoundRadius*`: those run-time radii are interned by intern() (S0c freeze
 * decision), never freed, and bounded by the distinct appearance heights.
 *
 * @author ATracer
 */
class BoundRadius : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/BoundRadius.xml.inc"
public:
	/** Java `new BoundRadius(0f, 0f, 0f)`; immortal like every template */
	static const BoundRadius DEFAULT;

	BoundRadius() = default;

	BoundRadius(float front, float side, float upper);

	float getMaxOfFrontAndSide() const;

	/** C++ only: the immortal BoundRadius with these values (Java `new BoundRadius(front, side, upper)` at run time); thread-safe */
	static const BoundRadius* intern(float front, float side, float upper);
};

} // namespace aion::gameserver::model::templates
