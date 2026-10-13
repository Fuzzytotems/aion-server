#pragma once

#include <optional>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * A neuron of the player model (D12, owner 2026-10-13): its incoming and outgoing links, bias, gradient and value.
 * <p>
 * C++: Java's public fields are Fields (the model is trained on a long-running task and read by the custom instance's AI). The second
 * constructor takes the previous layer, as Java's List<PlayerModelLink>.
 *
 * @author Jo
 */
class PlayerModelLink : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<Link>>>> inputs{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<Link>>>> outputs{};
	runtime::Field<double> bias{};
	runtime::Field<double> biasDelta{};
	runtime::Field<double> gradient{};
	runtime::Field<double> value{};
protected:
	PlayerModelLink();
	explicit PlayerModelLink(const runtime::ArrayList<runtime::Ref<PlayerModelLink>>& inputList);
public:
	static runtime::Ref<PlayerModelLink> create();
	static runtime::Ref<PlayerModelLink> create(const runtime::ArrayList<runtime::Ref<PlayerModelLink>>& inputList);
	double calculateValue();
	double calculateError(double target);
	double calculateGradient(std::optional<double> target);
	void updateWeights(double learnRate, double momentum);
protected:
	~PlayerModelLink() override;
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
