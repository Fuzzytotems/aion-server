#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * The custom instance's player model (D12, owner 2026-10-13): a feed-forward network trained by back-propagation with momentum on the
 * player's recorded actions.
 * <p>
 * C++: Java's public fields are Fields (train runs on a long-running task while the AI reads isReady); the double... parameters are spans;
 * getOutputEstimation returns the values (Java's List<Double>).
 *
 * @author Jo
 */
class PlayerModel : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int64_t MAX_TRAINING_TIME_IN_MS = 180000;
public:
	/** C++: the QuiescentOptIn purpose of PlayerModelController::runTraining; train's quiescent points take effect only under it */
	static constexpr const char* TRAINING_QUIESCENCE = "PlayerModel training";
	runtime::Field<bool> isReady{};
	runtime::Field<double> learnRate{};
	runtime::Field<double> momentum{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<PlayerModelLink>>>> input{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<runtime::RcArrayList<runtime::Ref<PlayerModelLink>>>>>> inner{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<PlayerModelLink>>>> output{};
protected:
	PlayerModel(int32_t inputSize, int32_t hiddenSize, int32_t outputSize, int32_t innerSize, std::optional<double> learnRate,
		std::optional<double> momentum);
public:
	static runtime::Ref<PlayerModel> create(int32_t inputSize, int32_t hiddenSize, int32_t outputSize, int32_t innerSize,
		std::optional<double> learnRate, std::optional<double> momentum);
	void train(const std::vector<runtime::Ref<DataSet>>& dataSets, int32_t numEpochs);
private:
	void processInput(std::span<const double> inputs);
	void valideToOutput(std::span<const double> targets);
public:
	std::vector<double> getOutputEstimation(std::span<const double> inputs);
	static double getRandom();
protected:
	~PlayerModel() override;
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
