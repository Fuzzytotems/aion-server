#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"

#include "aion/gameserver/model/templates/BoundRadius.h"

namespace aion::gameserver::model::templates {

const BoundRadius* VisibleObjectTemplate::getBoundRadius() const {
	return &BoundRadius::ZERO; // upstream da540feec: ZERO (DEFAULT is the creature cylinder now)
}

} // namespace aion::gameserver::model::templates
