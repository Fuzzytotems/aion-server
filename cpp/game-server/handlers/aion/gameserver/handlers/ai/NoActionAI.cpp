#include "aion/gameserver/handlers/ai/NoActionAI.h"

#include <optional>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai {

AION_AI(NoActionAI, "noaction");

void NoActionAI::handleAttack(runtime::Ptr<Creature> creature) {
	// Java: switch (getOwner().getObjectTemplate().getTribe()) with fall-through to one loseAggro(true) - hp regen for training dummies. The
	// template's tribe is std::optional (Java null for the templates without a tribe attribute), and Java's switch on a null enum throws a
	// NullPointerException, so the port throws it too. All 3,390 npc_templates with ai="noaction" carry a tribe, so this is unreachable today.
	std::optional<TribeClass> tribe = getOwner().getObjectTemplate()->getTribe();
	if (!tribe)
		throw runtime::NullPointerException(
			"Cannot invoke \"TribeClass.ordinal()\" because the return value of \"NpcTemplate.getTribe()\" is null");
	switch (*tribe) { // hp regen for training dummies
		case TribeClass::TARGETBASFELT_DF1:
		case TribeClass::TARGETBASFELT2_DF1:
		case TribeClass::DUMMY:
		case TribeClass::DUMMY2:
		case TribeClass::LF5_DUMMY1:
		case TribeClass::LF5_DUMMY2:
		case TribeClass::DF5_DUMMY1:
		case TribeClass::DF5_DUMMY2:
			getOwner().getController().loseAggro(true);
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::ai
