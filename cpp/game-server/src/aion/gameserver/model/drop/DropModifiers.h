#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::model::drop {

class DropModifiers {
private:
	bool isDropNpcChest_{};
	Race dropRace{};
	float boostDropRate{};
	std::optional<float> reductionDropRate{};
	std::optional<int32_t> maxDropsPerGroup{};

public:
	bool isDropNpcChest() const { return this->isDropNpcChest_; }

	void setIsDropNpcChest(bool dropNpcChest) { this->isDropNpcChest_ = dropNpcChest; }

	Race getDropRace() const { return this->dropRace; }

	void setDropRace(Race value) { this->dropRace = value; }

	float getBoostDropRate() const { return this->boostDropRate; }

	void setBoostDropRate(float value) { this->boostDropRate = value; }

	std::optional<float> getReductionDropRate() const { return this->reductionDropRate; }

	void setReductionDropRate(std::optional<float> value) { this->reductionDropRate = value; }

	std::optional<int32_t> getMaxDropsPerGroup() const { return this->maxDropsPerGroup; }

	void setMaxDropsPerGroup(std::optional<int32_t> value) { this->maxDropsPerGroup = value; }

	float calculateDropChance(float chance, bool allowReductionDropRate);
};

} // namespace aion::gameserver::model::drop
