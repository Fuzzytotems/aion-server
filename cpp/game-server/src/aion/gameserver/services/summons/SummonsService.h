#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/summons/fwd.h"
#include "aion/gameserver/services/summons/fwd.h"

namespace aion::gameserver::services::summons {

/**
 * @author xTz
 */
class SummonsService {
private:
	/** Java: static class ReleaseSummonTask implements Runnable (used only by the bodies, defined in SummonsService.cpp) */
	class ReleaseSummonTask;
public:
	static runtime::Ptr<model::gameobjects::Summon> createSummon(model::gameobjects::player::Player& master, int32_t npcId, int32_t skillId,
		int32_t skillLevel, int32_t time);
	/**
	 * Releases the summon after {@link UnsummonType#getDelayMillis()}, see {@link Summon#registerRelease(SummonRelease)} for competing releases.
	 */
	static void release(model::gameobjects::Summon& summon, model::summons::UnsummonType unsummonType);
	/** Change to rest mode */
	static void restMode(model::gameobjects::Summon& summon);
	static void setUnkMode(model::gameobjects::Summon& summon);
	/** Change to guard mode */
	static void guardMode(model::gameobjects::Summon& summon);
	/** Change to attackMode */
	static void attackMode(model::gameobjects::Summon& summon);
	static void doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon);
	static void doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon, model::summons::UnsummonType unsummonType);
	/** @param unsummonType null for a plain mode change (doMode(summonMode, summon) passes null, SummonsService.java:177) */
	static void doMode(model::summons::SummonMode summonMode, model::gameobjects::Summon& summon, int32_t targetObjId,
		std::optional<model::summons::UnsummonType> unsummonType);
};

} // namespace aion::gameserver::services::summons
