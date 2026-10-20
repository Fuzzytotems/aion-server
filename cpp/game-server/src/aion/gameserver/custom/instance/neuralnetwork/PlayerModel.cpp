#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"

#include <format>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/custom/instance/neuralnetwork/DataSet.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelLink.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

using LinkList = runtime::RcArrayList<runtime::Ref<PlayerModelLink>>;

static const auto log = commons::logging::LoggerFactory::getLogger("CUSTOM_INSTANCE_LOG");

// Java PlayerModel.java:28-56
PlayerModel::PlayerModel(int32_t inputSize, int32_t hiddenSize, int32_t outputSize, int32_t innerSize, std::optional<double> learnRate,
	std::optional<double> momentum) {
	if (!learnRate)
		this->learnRate.set(.4);
	else
		this->learnRate.set(*learnRate);
	if (!momentum)
		this->momentum.set(.9);
	else
		this->momentum.set(*momentum);
	input = LinkList::create(AION_LOCK_CLASS(PlayerModel::input));
	inner = runtime::RcArrayList<runtime::Ref<LinkList>>::create(AION_LOCK_CLASS(PlayerModel::inner));
	output = LinkList::create(AION_LOCK_CLASS(PlayerModel::output));
	isReady.set(false);

	for (int32_t i = 0; i < inputSize; i++)
		input.get()->add(PlayerModelLink::create());
	if (innerSize < 1)
		innerSize = 1;
	for (int32_t i = 0; i < innerSize; i++) {
		inner.get()->add(LinkList::create(AION_LOCK_CLASS(PlayerModel::inner)));
		for (int32_t j = 0; j < hiddenSize; j++)
			inner.get()->get(i)->add(PlayerModelLink::create(i == 0 ? *input.get() : *inner.get()->get(i - 1)));
	}
	for (int32_t i = 0; i < outputSize; i++)
		output.get()->add(PlayerModelLink::create(*inner.get()->get(innerSize - 1)));
}

runtime::Ref<PlayerModel> PlayerModel::create(int32_t inputSize, int32_t hiddenSize, int32_t outputSize, int32_t innerSize,
	std::optional<double> learnRate, std::optional<double> momentum) {
	return runtime::makeRef<PlayerModel>(inputSize, hiddenSize, outputSize, innerSize, learnRate, momentum);
}

// Java PlayerModel.java:58-77
void PlayerModel::train(const std::vector<runtime::Ref<DataSet>>& dataSets, int32_t numEpochs) {
	int64_t startTime = commons::utils::currentTimeMillis();
	isReady.set(false);
	for (int32_t i = 0; i < numEpochs; i++) {
		// C++: under PlayerModelController::runTraining, release the reclaimer epoch between training epochs; no borrow lives across them
		if (runtime::QuiescentOptIn::active(TRAINING_QUIESCENCE))
			runtime::quiescentPoint(); // quiescent-safe: train's frame holds the data sets as Refs and the model through its caller's Ref
		for (const runtime::Ref<DataSet>& dataSet : dataSets) {
			processInput(dataSet->getValues());
			valideToOutput(dataSet->getTargets());
		}
		if (commons::utils::currentTimeMillis() >= startTime + MAX_TRAINING_TIME_IN_MS) {
			numEpochs = i;
			break;
		}
	}
	isReady.set(true);
	int64_t processingTime = commons::utils::currentTimeMillis() - startTime;
	if (processingTime >= MAX_TRAINING_TIME_IN_MS)
		log.warn(std::format("[CI_ROAH] Deep learning exceeded [MAX_TRAINING_TIME_IN_MS={}] with {} data sets. Only {} cycles were processed.",
			MAX_TRAINING_TIME_IN_MS, dataSets.size(), numEpochs));
}

// Java PlayerModel.java:79-87
void PlayerModel::processInput(std::span<const double> inputs) {
	size_t i = 0;
	for (runtime::Ptr<PlayerModelLink> n : *input.get()) {
		if (i >= inputs.size()) // Java: inputs[i++] past the array
			throw runtime::ArrayIndexOutOfBoundsException(std::format("Index {} out of bounds for length {}", i, inputs.size()));
		n->value.set(inputs[i++]);
	}
	for (runtime::Ptr<LinkList> layer : *inner.get())
		layer->forEach([](runtime::Ptr<PlayerModelLink> n) { n->calculateValue(); });
	output.get()->forEach([](runtime::Ptr<PlayerModelLink> n) { n->calculateValue(); });
}

// Java PlayerModel.java:89-99
void PlayerModel::valideToOutput(std::span<const double> targets) {
	size_t i = 0;
	for (runtime::Ptr<PlayerModelLink> n : *output.get()) {
		if (i >= targets.size()) // Java: targets[i++] past the array
			throw runtime::ArrayIndexOutOfBoundsException(std::format("Index {} out of bounds for length {}", i, targets.size()));
		n->calculateGradient(targets[i++]);
	}
	for (int32_t j = inner.get()->size() - 1; j >= 0; j--) {
		runtime::Ptr<LinkList> layer = inner.get()->get(j);
		layer->forEach([](runtime::Ptr<PlayerModelLink> a) { a->calculateGradient(std::nullopt); });
		layer->forEach([this](runtime::Ptr<PlayerModelLink> a) { a->updateWeights(learnRate.get(), momentum.get()); });
	}
	output.get()->forEach([this](runtime::Ptr<PlayerModelLink> a) { a->updateWeights(learnRate.get(), momentum.get()); });
}

// Java PlayerModel.java:101-107
std::vector<double> PlayerModel::getOutputEstimation(std::span<const double> inputs) {
	processInput(inputs);
	std::vector<double> outputList;
	for (runtime::Ptr<PlayerModelLink> n : *output.get())
		outputList.push_back(n->value.get());
	return outputList;
}

// Java PlayerModel.java:109-111
double PlayerModel::getRandom() {
	return 2 * commons::utils::Rnd::nextDouble() - 1;
}

PlayerModel::~PlayerModel() = default;

} // namespace aion::gameserver::custom::instance::neuralnetwork
