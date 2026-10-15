#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelLink.h"

#include "aion/gameserver/custom/instance/neuralnetwork/Link.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/Sigmoid.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

// Java PlayerModelLink.java:20-24
PlayerModelLink::PlayerModelLink() {
	inputs = runtime::RcArrayList<runtime::Ref<Link>>::create(AION_LOCK_CLASS(PlayerModelLink::inputs));
	outputs = runtime::RcArrayList<runtime::Ref<Link>>::create(AION_LOCK_CLASS(PlayerModelLink::outputs));
	bias.set(PlayerModel::getRandom());
}

runtime::Ref<PlayerModelLink> PlayerModelLink::create() {
	return runtime::makeRef<PlayerModelLink>();
}

// Java PlayerModelLink.java:26-33
PlayerModelLink::PlayerModelLink(const runtime::ArrayList<runtime::Ref<PlayerModelLink>>& inputList) : PlayerModelLink() {
	for (runtime::Ptr<PlayerModelLink> input : inputList) {
		runtime::Ref<Link> l = Link::create(*input, *this);
		input->outputs.get()->add(l);
		inputs.get()->add(l);
	}
}

runtime::Ref<PlayerModelLink> PlayerModelLink::create(const runtime::ArrayList<runtime::Ref<PlayerModelLink>>& inputList) {
	return runtime::makeRef<PlayerModelLink>(inputList);
}

// Java PlayerModelLink.java:35-41
double PlayerModelLink::calculateValue() {
	double sum = 0;
	for (runtime::Ptr<Link> l : *inputs.get())
		sum += l->getWeight() * l->getInput().value.get();
	value.set(Sigmoid::output(sum + bias.get()));
	return value.get();
}

// Java PlayerModelLink.java:43-45
double PlayerModelLink::calculateError(double target) {
	return target - value.get();
}

// Java PlayerModelLink.java:47-57
double PlayerModelLink::calculateGradient(std::optional<double> target) {
	if (!target) {
		double sum = 0;
		for (runtime::Ptr<Link> l : *outputs.get())
			sum += l->getOutput().gradient.get() * l->getWeight();
		gradient.set(sum * Sigmoid::derivative(value.get()));
		return gradient.get();
	}
	gradient.set(calculateError(*target) * Sigmoid::derivative(value.get()));
	return gradient.get();
}

// Java PlayerModelLink.java:59-68
void PlayerModelLink::updateWeights(double learnRate, double momentum) {
	double prevDelta = biasDelta.get();
	biasDelta.set(learnRate * gradient.get());
	bias.set(bias.get() + biasDelta.get() + momentum * prevDelta);
	for (runtime::Ptr<Link> l : *inputs.get()) {
		prevDelta = l->getWeightDelta();
		l->setWeightDelta(learnRate * gradient.get() * l->getInput().value.get());
		l->setWeight(l->getWeight() + l->getWeightDelta() + momentum * prevDelta);
	}
}

PlayerModelLink::~PlayerModelLink() = default;

} // namespace aion::gameserver::custom::instance::neuralnetwork
