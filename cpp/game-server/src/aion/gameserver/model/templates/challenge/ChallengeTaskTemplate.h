#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::challenge {

/** Java com.aionemu.gameserver.model.templates.challenge.ChallengeTaskTemplate. */
class ChallengeTaskTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates::challenge
