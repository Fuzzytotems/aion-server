#include "aion/gameserver/skillengine/task/CraftingTask.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"

namespace aion::gameserver::skillengine::task {

CraftingTask::CraftingTask(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::StaticObject> responderValue,
	const gameserver::model::templates::recipe::RecipeTemplate* /*recipeTemplate*/, int32_t skillLvlDiffValue, int32_t /*bonus*/)
	: AbstractCraftTask(requesterValue, responderValue, skillLvlDiffValue), recipeTemplate(nullptr), maxCritCount(0), bonus(0) {
	// Java: super(requester, responder, skillLvlDiff); the members from recipeTemplate, bonus and DataManager.ITEM_DATA (CraftingTask.java:31-38).
	// The const members hold placeholders until C-02 (m5c-plan.md) ports the constructor.
	AION_UNPORTED();
}

CraftingTask::~CraftingTask() = default;

runtime::Ref<CraftingTask> CraftingTask::create(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::StaticObject> responderValue,
	const gameserver::model::templates::recipe::RecipeTemplate* recipeTemplateValue, int32_t skillLvlDiffValue, int32_t bonusValue) {
	return runtime::makeRef<CraftingTask>(requesterValue, responderValue, recipeTemplateValue, skillLvlDiffValue, bonusValue);
}

void CraftingTask::onFailureFinish() {
	AION_UNPORTED();
}

bool CraftingTask::onSuccessFinish() {
	AION_UNPORTED();
}

bool CraftingTask::calculateCrit() {
	AION_UNPORTED();
}

void CraftingTask::sendInteractionUpdate() {
	AION_UNPORTED();
}

void CraftingTask::onInteractionAbort() {
	AION_UNPORTED();
}

void CraftingTask::onInteractionFinish() {
	AION_UNPORTED();
}

void CraftingTask::onInteractionStart() {
	AION_UNPORTED();
}

void CraftingTask::analyzeInteraction() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::task
