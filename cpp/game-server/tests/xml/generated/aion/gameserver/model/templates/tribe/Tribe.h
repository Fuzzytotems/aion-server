#pragma once

// TEST SHELL for the generated-code slice test (GeneratedSliceTest.cpp), created with `xmlgen.py scaffold model.templates.tribe.Tribe`. It stands
// in for the hand-written class of the static data port (P4-09).

#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/tribe/Tribe.xml.h"

namespace aion::gameserver::model::templates::tribe {

/** Java com.aionemu.gameserver.model.templates.tribe.Tribe (test shell). @author ATracer */
class Tribe {
#include "aion/gameserver/model/templates/tribe/Tribe.xml.inc"
public:
	/** Java: getBase() */
	TribeClass getBase() const { return base == TribeClass::NONE ? name : base; }
	const std::optional<std::vector<TribeClass>>& getAggroList() const { return aggro; }
	const std::optional<std::vector<TribeClass>>& getFriendList() const { return friend_; }
};

} // namespace aion::gameserver::model::templates::tribe
