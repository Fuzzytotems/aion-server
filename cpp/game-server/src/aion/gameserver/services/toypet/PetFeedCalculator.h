#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/templates/pet/fwd.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::services::toypet {

/**
 * <b>Current pre-calculated values multiplied by 4; in packet 14 bits. Max value: 17600 / 4 is 13 bits; feed points as in retail packets.</b><br>
 * static final byte[][] pointValues = new byte[][] {<br>
 * // 10 25 40 50 100 200 -- feed max count<br>
 * { 0, 0, 0, 0, 0, 0 }, // level 1~5 items (feed points 0)<br>
 * { 80, 200, 320, 400, 800, 1600 }, // level 6~10 items (feed points 8)<br>
 * { 160, 400, 640, 800, 1600, 3200 }, // level 11~15 items (feed points 16)<br>
 * { 240, 600, 960, 1200, 2400, 4800 }, // level 16~20 items (feed points 24)<br>
 * { 320, 800, 1280, 1600, 3200, 6400 }, // level 21~25 items (feed points 32)<br>
 * { 400, 1000, 1600, 2000, 4000, 8000 }, // level 26~30 items (feed points 40)<br>
 * { 480, 1200, 1920, 2400, 4800, 9600 }, // level 31~35 items (feed points 48)<br>
 * { 560, 1400, 2240, 2800, 5600, 11200 }, // level 36~40 items (feed points 56)<br>
 * { 640, 1600, 2560, 3200, 6400, 12800 }, // level 41~45 items (feed points 64)<br>
 * { 720, 1800, 2880, 3600, 7200, 14400 }, // level 46~50 items (feed points 72)<br>
 * { 800, 2000, 3200, 4000, 8000, 16000 }, // level 51~55 items (feed points 80)<br>
 * { 880, 2200, 3520, 4400, 8800, 17600 } // level 56~60 items (feed points 88)<br>
 * };
 *
 *  @author Rolandas
 */
class PetFeedCalculator final {
public:
	static inline runtime::Field<int8_t> ITEM_MAX_LEVEL{60};
	// Java static final arrays assigned by the static initializer block (PetFeedCalculator.java:38), which reads DataManager.PET_FEED_DATA when the
	// class is first used. C++ static initialization runs before the static data loads, so the port assigns them on first use: Field, not const.
	static inline runtime::Field<runtime::Ref<runtime::Array<int16_t>>> fullCounts{};
	static inline runtime::Field<runtime::Ref<runtime::Array<int8_t>>> itemLevels{};
	static inline runtime::Field<runtime::Ref<runtime::Array<runtime::Ref<runtime::Array<int32_t>>>>> pointValues{};

	/** Calculate point values for each item levels and each max feed count */
	static void calculate();
	/** Formula to calculate pointValues array */
	static int32_t getPoints(int32_t feedPoints, int32_t maxFeedCount);
	static void updatePetFeedProgress(PetFeedProgress& progress, int32_t itemLevel, int32_t maxFeedCount);
	static const model::templates::pet::PetFeedResult* getReward(int32_t fullCount, const model::templates::pet::PetRewards* rewardGroup,
		PetFeedProgress& progress, int32_t playerLevel);
};

} // namespace aion::gameserver::services::toypet
