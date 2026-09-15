#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.xml.h"

#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::siegelocation {

/** Java com.aionemu.gameserver.model.templates.siegelocation.SiegeLocationTemplate. @author Sarynth, antness, Source, Wakizashi */
class SiegeLocationTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }

	/** @throws NullPointerException (Java) without artifact_activation */
	int32_t getRepeatCount() const;

	/** @throws NullPointerException (Java) without artifact_activation */
	int32_t getRepeatInterval() const;

	/** Java returns Collections.emptyList() without fortress_dependency */
	const std::vector<int32_t>& getFortressDependency() const;

	/** @return the kinah reward of the level, 0 without kinah_rewards or for a level beyond them */
	int32_t getKinahRewardByRewardLevel(int32_t rewardLevel) const;
};

} // namespace aion::gameserver::model::templates::siegelocation
