#include "aion/gameserver/skillengine/task/AbstractCraftTask.h"

namespace aion::gameserver::skillengine::task {

AbstractCraftTask::AbstractCraftTask(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> responderValue, int32_t skillLvlDiffValue)
	: AbstractInteractionTask(requesterValue, responderValue), skillLvlDiff(skillLvlDiffValue) {
}

AbstractCraftTask::~AbstractCraftTask() = default;

bool AbstractCraftTask::onInteraction() {
	if (currentSuccessValue.get() == fullBarValue) {
		return onSuccessFinish();
	}
	if (currentFailureValue.get() == fullBarValue) {
		onFailureFinish();
		return true;
	}

	analyzeInteraction();

	sendInteractionUpdate();
	return false;
}

} // namespace aion::gameserver::skillengine::task
