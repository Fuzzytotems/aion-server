#include "aion/gameserver/model/instance/instancescore/LegionDominionScore.h"

namespace aion::gameserver::model::instance::instancescore {

LegionDominionScore::LegionDominionScore() = default;

LegionDominionScore::~LegionDominionScore() = default;

runtime::Ref<LegionDominionScore> LegionDominionScore::create() {
	return runtime::makeRef<LegionDominionScore>();
}

// Java LegionDominionScore.java:24-26
void LegionDominionScore::addPoints(int32_t value) {
	this->points.set(this->points.get() + value);
}

} // namespace aion::gameserver::model::instance::instancescore
