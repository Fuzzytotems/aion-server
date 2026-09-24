#include "aion/gameserver/skillengine/effect/ProcVPHealInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_EXP.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::gameobjects::player::PlayerCommonData;
using runtime::Ptr;

namespace {

/** Java long a * b (wraps on overflow) */
constexpr int64_t mulLong(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

} // namespace

void ProcVPHealInstantEffect::applyEffect(model::Effect& effect) const {
	if (Ptr<Player> player = runtime::as<Player>(effect.getEffected())) {
		Ptr<PlayerCommonData> pcd = player->getCommonData();

		int64_t cap = mulLong(pcd->getMaxReposeEnergy(), value2) / 100;

		if (pcd->isReadyForReposeEnergy() && pcd->getCurrentReposeEnergy() < cap) {
			int32_t valueWithDelta = calculateBaseValue(effect);
			int64_t addEnergy = 0;
			// Java: (int) (long * int * 0.001) - the long product wraps, the double product is narrowed by the saturating (int) cast
			if (percent)
				addEnergy = geoEngine::math::JavaFloat::doubleToInt(
					static_cast<double>(mulLong(pcd->getMaxReposeEnergy(), valueWithDelta)) * 0.001); // recheck when more skills
			else
				addEnergy = valueWithDelta;

			pcd->addReposeEnergy(addEnergy);
			utils::PacketSendUtility::sendPacket(*player,
				network::aion::serverpackets::SM_STATUPDATE_EXP(pcd->getExpShown(), pcd->getExpRecoverable(), pcd->getExpNeed(),
					pcd->getCurrentReposeEnergy(), pcd->getMaxReposeEnergy()));
		}
	}
}

} // namespace aion::gameserver::skillengine::effect
