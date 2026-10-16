#pragma once

#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * The logistic function of the custom instance's player model (D12, owner 2026-10-13).
 *
 * @author Jo
 */
class Sigmoid {
public:
	static double output(double x);
	static double derivative(double x);
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
