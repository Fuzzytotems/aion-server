#include "aion/gameserver/custom/instance/neuralnetwork/Link.h"

#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelLink.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

// Java Link.java:15-19
Link::Link(PlayerModelLink& input, PlayerModelLink& output) : input(&input), output(&output) {
	weight.set(PlayerModel::getRandom());
}

runtime::Ref<Link> Link::create(PlayerModelLink& input, PlayerModelLink& output) {
	return runtime::makeRef<Link>(input, output);
}

Link::~Link() = default;

} // namespace aion::gameserver::custom::instance::neuralnetwork
