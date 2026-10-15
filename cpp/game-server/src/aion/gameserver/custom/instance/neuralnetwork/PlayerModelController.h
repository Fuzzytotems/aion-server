#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * Builds and queries a player's model from his recorded entries (D12, owner 2026-10-13).
 * <p>
 * C++: trainModelForPlayer returns a Ref (null: Java's null when the player has no entries); the training runs on the long-running pool,
 * pinned to the model.
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
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
