#include "aion/gameserver/model/instance/instancescore/DarkPoetaScore.h"

namespace aion::gameserver::model::instance::instancescore {

DarkPoetaScore::DarkPoetaScore() = default;

DarkPoetaScore::~DarkPoetaScore() = default;

runtime::Ref<DarkPoetaScore> DarkPoetaScore::create() {
	return runtime::makeRef<DarkPoetaScore>();
}

// Java DarkPoetaScore.java:15-17
void DarkPoetaScore::addPoints(int32_t value) {
	this->points.set(this->points.get() + value);
}

} // namespace aion::gameserver::model::instance::instancescore
