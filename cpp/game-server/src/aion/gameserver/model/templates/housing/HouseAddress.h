#pragma once

#include "aion/gameserver/model/templates/housing/HouseAddress.xml.h"

#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Java com.aionemu.gameserver.model.templates.housing.HouseAddress.
 * <p>
 * C++: the @XmlTransient `land` is a C++-only template pointer, set by the hook to the HousingLand being bound (bound objects never move).
 *
 * @author Rolandas
 */
class HouseAddress : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/housing/HouseAddress.xml.inc"
private:
	/** Java @XmlTransient HousingLand land */
	const HousingLand* land = nullptr;

public:
	const HousingLand* getLand() const { return land; }
};

} // namespace aion::gameserver::model::templates::housing
