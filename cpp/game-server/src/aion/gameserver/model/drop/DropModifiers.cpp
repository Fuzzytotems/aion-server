#include "aion/gameserver/model/drop/DropModifiers.h"

namespace aion::gameserver::model::drop {

float DropModifiers::calculateDropChance(float chance, bool allowReductionDropRate) {
	if (allowReductionDropRate && reductionDropRate)
		chance *= *reductionDropRate;
	return chance * boostDropRate;
}

} // namespace aion::gameserver::model::drop
