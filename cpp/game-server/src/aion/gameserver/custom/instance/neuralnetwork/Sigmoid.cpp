#include "aion/gameserver/custom/instance/neuralnetwork/Sigmoid.h"

#include <cmath>

namespace aion::gameserver::custom::instance::neuralnetwork {

// Java Sigmoid.java:8-10 (Math.exp of the float-rounded -x, widened back to double)
double Sigmoid::output(double x) {
	return x < -45.0 ? 0.0 : x > 45.0 ? 1.0 : 1.0 / (1.0 + std::exp(static_cast<double>(static_cast<float>(-x))));
}

// Java Sigmoid.java:12-14
double Sigmoid::derivative(double x) {
	return x * (1 - x);
}

} // namespace aion::gameserver::custom::instance::neuralnetwork
