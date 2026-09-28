#include "aion/gameserver/skillengine/action/Action.h"

namespace aion::gameserver::skillengine::action {

bool Action::canAct(model::Skill& /*skill*/) const {
	return true;
}

} // namespace aion::gameserver::skillengine::action
