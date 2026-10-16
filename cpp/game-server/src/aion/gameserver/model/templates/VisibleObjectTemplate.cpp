#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"

#include "aion/gameserver/model/templates/BoundRadius.h"

namespace aion::gameserver::model::templates {

const BoundRadius* VisibleObjectTemplate::getBoundRadius() const {
	return &BoundRadius::DEFAULT;
}

} // namespace aion::gameserver::model::templates
