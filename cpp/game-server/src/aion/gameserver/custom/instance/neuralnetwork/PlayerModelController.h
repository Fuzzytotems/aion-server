#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/custom/instance/neuralnetwork/DataSet.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * Builds and queries a player's model from his recorded entries (D12, owner 2026-10-13).
 * <p>
 * C++: trainModelForPlayer returns a Ref (null: Java's null when the player has no entries); the training runs on the long-running pool,
 * pinned to the model, through runTraining, which lets the reclaimer advance between training epochs (runtime-architecture.md §2.6).
 *
 * @author Jo
 */
class PlayerModelController {
private:
	static constexpr int32_t TRAINING_EPOCHS = 1000;
public:
	static runtime::Ref<PlayerModel> trainModelForPlayer(int32_t playerId, const std::vector<int32_t>& skillSet);
	static std::vector<int32_t> getSkillSetForPlayer(int32_t playerId);
	static int32_t getActionOutput(PlayerModel& model, std::span<const double> inputArray);
	static int32_t getMaxIndex(const std::vector<double>& values);

	/**
	 * C++: the body of trainModelForPlayer's long-running task - model.train(dataSets, epochs) inside a QuiescentScope with the
	 * PlayerModel::TRAINING_QUIESCENCE opt-in, so train places a quiescentPoint before each training epoch and the task does not hold one
	 * reclaimer epoch for the whole training (a "Reclamation lag" warning after ~10 s). The training itself is unchanged. Call it directly
	 * in a task body (TaskScope::depth() == 1) whose frames hold only Refs: elsewhere the quiescent points are no-ops.
	 */
	static void runTraining(runtime::Ref<PlayerModel> model, const std::vector<runtime::Ref<DataSet>>& dataSets, int32_t epochs);
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
