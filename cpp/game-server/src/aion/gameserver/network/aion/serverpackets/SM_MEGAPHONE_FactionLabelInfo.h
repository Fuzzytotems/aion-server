#pragma once

#include <cstdint>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE_FactionLabel.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Companion of the generated enum SM_MEGAPHONE_FactionLabel (static-data.md §2.5): the constructor data `id` as a free function.
 */

/** Java: SM_MEGAPHONE.FactionLabel.id - NONE((byte) -1), ELYOS((byte) Race.ELYOS.getRaceId()), ASMODIANS((byte) Race.ASMODIANS.getRaceId()) */
inline int8_t id(SM_MEGAPHONE_FactionLabel label) {
	switch (label) {
		case SM_MEGAPHONE_FactionLabel::ELYOS:
			return static_cast<int8_t>(model::getRaceId(model::Race::ELYOS));
		case SM_MEGAPHONE_FactionLabel::ASMODIANS:
			return static_cast<int8_t>(model::getRaceId(model::Race::ASMODIANS));
		case SM_MEGAPHONE_FactionLabel::NONE:
			break;
	}
	return -1;
}

} // namespace aion::gameserver::network::aion::serverpackets
