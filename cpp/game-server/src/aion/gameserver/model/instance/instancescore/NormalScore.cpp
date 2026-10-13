#include "aion/gameserver/model/instance/instancescore/NormalScore.h"

namespace aion::gameserver::model::instance::instancescore {

NormalScore::NormalScore() = default;

NormalScore::~NormalScore() = default;

runtime::Ref<NormalScore> NormalScore::create() {
	return runtime::makeRef<NormalScore>();
}

// Java NormalScore.java:23-27
void NormalScore::addPoints(int32_t value) {
	this->points.set(this->points.get() + value);
	if (this->points.get() < 0)
		this->points.set(0);
}

} // namespace aion::gameserver::model::instance::instancescore
