#include "aion/gameserver/custom/instance/neuralnetwork/DataSet.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

namespace {

runtime::Ref<runtime::Array<double>> toArray(std::span<const double> values) {
	runtime::Ref<runtime::Array<double>> array = runtime::Array<double>::make(static_cast<int32_t>(values.size()));
	for (size_t i = 0; i < values.size(); i++)
		(*array)[static_cast<int32_t>(i)] = values[i];
	return array;
}

std::vector<double> toVector(const runtime::Array<double>& array) {
	std::vector<double> values;
	for (double value : array.snapshot())
		values.push_back(value);
	return values;
}

} // namespace

// Java DataSet.java:11-14
DataSet::DataSet(std::span<const double> values, std::span<const double> targets) : values(toArray(values)), targets(toArray(targets)) {
}

runtime::Ref<DataSet> DataSet::create(std::span<const double> values, std::span<const double> targets) {
	return runtime::makeRef<DataSet>(values, targets);
}

// Java DataSet.java:16-18
std::vector<double> DataSet::getValues() const {
	return toVector(*values);
}

// Java DataSet.java:20-22
std::vector<double> DataSet::getTargets() const {
	return toVector(*targets);
}

DataSet::~DataSet() = default;

} // namespace aion::gameserver::custom::instance::neuralnetwork
