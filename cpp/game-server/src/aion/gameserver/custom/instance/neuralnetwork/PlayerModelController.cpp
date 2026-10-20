#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelController.h"

#include <algorithm>

#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/instance/neuralnetwork/DataSet.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelEntry.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

// Java PlayerModelController.java:21-41
runtime::Ref<PlayerModel> PlayerModelController::trainModelForPlayer(int32_t playerId, const std::vector<int32_t>& skillSet) {
	// get input data:
	std::vector<runtime::Ref<PlayerModelEntry>> playerModelEntries;
	for (runtime::Ptr<PlayerModelEntry> entry : *CustomInstanceService::getInstance().getPlayerModelEntries(playerId))
		playerModelEntries.emplace_back(entry);

	// specify model:
	std::vector<runtime::Ref<DataSet>> dataSets;
	for (size_t i = 0; i < playerModelEntries.size(); i++) {
		int32_t previousSkillID = -1;
		if (i > 0)
			previousSkillID = playerModelEntries[i - 1]->getSkillID();
		dataSets.push_back(DataSet::create(playerModelEntries[i]->toStateInputArray(skillSet, previousSkillID),
			playerModelEntries[i]->toActionOutputArray(skillSet)));
	}
	if (dataSets.empty())
		return {};
	runtime::Ref<PlayerModel> model = PlayerModel::create(static_cast<int32_t>(dataSets[0]->getValues().size()), 10,
		static_cast<int32_t>(dataSets[0]->getTargets().size()), 1, std::nullopt, std::nullopt);

	// train
	utils::ThreadPoolManager::getInstance().executeLongRunning({model.get()}, [model, dataSets] { runTraining(model, dataSets, TRAINING_EPOCHS); });
	return model;
}

// C++ (not in Java): Java's model.train(dataSets, TRAINING_EPOCHS) of the long-running task, with quiescent points between training epochs
void PlayerModelController::runTraining(runtime::Ref<PlayerModel> model, const std::vector<runtime::Ref<DataSet>>& dataSets, int32_t epochs) {
	// the frames above hold only Refs: the model (this parameter) and the data sets (the task's captures); train holds no borrow across epochs
	runtime::QuiescentScope quiescent;
	runtime::QuiescentOptIn optIn(PlayerModel::TRAINING_QUIESCENCE);
	model->train(dataSets, epochs);
}

// Java PlayerModelController.java:43-49
std::vector<int32_t> PlayerModelController::getSkillSetForPlayer(int32_t playerId) {
	std::vector<int32_t> skillSet;
	for (runtime::Ptr<PlayerModelEntry> pme : *CustomInstanceService::getInstance().getPlayerModelEntries(playerId))
		if (std::find(skillSet.begin(), skillSet.end(), pme->getSkillID()) == skillSet.end())
			skillSet.push_back(pme->getSkillID());
	return skillSet;
}

// Java PlayerModelController.java:51-54
int32_t PlayerModelController::getActionOutput(PlayerModel& model, std::span<const double> inputArray) {
	std::vector<double> targetValues = model.getOutputEstimation(inputArray);
	return getMaxIndex(targetValues);
}

// Java PlayerModelController.java:56-68
int32_t PlayerModelController::getMaxIndex(const std::vector<double>& values) {
	int32_t i = 0;
	int32_t actionI = 0;
	double max_t = 0;
	for (double t : values) {
		if (t > max_t) {
			max_t = t;
			actionI = i;
		}
		i++;
	}
	return actionI;
}

} // namespace aion::gameserver::custom::instance::neuralnetwork
