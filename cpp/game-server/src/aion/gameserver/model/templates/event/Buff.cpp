#include "aion/gameserver/model/templates/event/Buff.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::event {

void Buff::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::event
