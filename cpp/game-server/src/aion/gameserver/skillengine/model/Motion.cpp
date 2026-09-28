#include "aion/gameserver/skillengine/model/Motion.h"

namespace aion::gameserver::skillengine::model {

void Motion::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: `if (name != null) name = name.intern();` only saves memory; std::string has no interning, so nothing changes (DEVIATIONS P4-08)
}

} // namespace aion::gameserver::skillengine::model
