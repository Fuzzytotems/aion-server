#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/item/WeaponStats.xml.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.WeaponStats. @author ATracer */
class WeaponStats : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/WeaponStats.xml.inc"
public:
	/** Java `(minDamage + maxDamage) / 2f`: int addition (wraps like Java), then float division */
	float getMeanDamage() const {
		int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(minDamage) + static_cast<uint32_t>(maxDamage));
		return static_cast<float>(sum) / 2.0f;
	}
};

} // namespace aion::gameserver::model::templates::item
