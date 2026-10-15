#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"

#include <algorithm>

namespace aion::gameserver::model::instance::instancescore {

// Java PvpInstanceScore.java:16-20
PvpInstanceScore::PvpInstanceScore(int32_t winnerApReward_, int32_t loserApReward_, int32_t drawApReward_)
	: winnerApReward(winnerApReward_), loserApReward(loserApReward_), drawApReward(drawApReward_) {
}

PvpInstanceScore::~PvpInstanceScore() = default;

runtime::Ref<PvpInstanceScore> PvpInstanceScore::create(int32_t winnerApReward, int32_t loserApReward, int32_t drawApReward) {
	return runtime::makeRef<PvpInstanceScore>(winnerApReward, loserApReward, drawApReward);
}

// Java PvpInstanceScore.java:42-44
int32_t PvpInstanceScore::getPointsByRace(Race race) const {
	return race == Race::ELYOS ? elyosPoints.get() : asmodiansPoints.get();
}

// Java PvpInstanceScore.java:54-56
Race PvpInstanceScore::getRaceWithHighestPoints() const {
	int32_t elyos = elyosPoints.get();
	int32_t asmodians = asmodiansPoints.get();
	return elyos == asmodians ? Race::NONE : elyos > asmodians ? Race::ELYOS : Race::ASMODIANS;
}

// Java PvpInstanceScore.java:58-63 (synchronized)
void PvpInstanceScore::addPointsByRace(Race race, int32_t points) {
	SYNCHRONIZED(*this) {
		switch (race) {
			case Race::ELYOS:
				elyosPoints.set(std::max(0, elyosPoints.get() + points));
				break;
			case Race::ASMODIANS:
				asmodiansPoints.set(std::max(0, asmodiansPoints.get() + points));
				break;
			default: // Java: an arrow switch statement without a default does nothing for the other constants
				break;
		}
	}
}

// Java PvpInstanceScore.java:65-70 (synchronized)
void PvpInstanceScore::incrementKillsByRace(Race race) {
	SYNCHRONIZED(*this) {
		switch (race) {
			case Race::ELYOS:
				elyosKills.set(elyosKills.get() + 1);
				break;
			case Race::ASMODIANS:
				asmodiansKills.set(asmodiansKills.get() + 1);
				break;
			default:
				break;
		}
	}
}

} // namespace aion::gameserver::model::instance::instancescore
