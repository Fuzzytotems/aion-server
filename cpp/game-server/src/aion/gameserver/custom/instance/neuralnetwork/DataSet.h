#pragma once

#include <span>
#include <vector>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * One training sample of the player model: the state inputs and the action targets.
 * <p>
 * C++: the getters return the arrays' values (Java returns the arrays themselves; nothing writes them).
 *
 * @author Jo
 */
class DataSet : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<runtime::Array<double>> values{};
	const runtime::Ref<runtime::Array<double>> targets{};
protected:
	DataSet(std::span<const double> values, std::span<const double> targets);
public:
	static runtime::Ref<DataSet> create(std::span<const double> values, std::span<const double> targets);
	std::vector<double> getValues() const;
	std::vector<double> getTargets() const;
protected:
	~DataSet() override;
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
