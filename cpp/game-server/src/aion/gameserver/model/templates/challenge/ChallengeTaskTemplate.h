#pragma once

#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.xml.h"

namespace aion::gameserver::model::templates::challenge {

/** Java com.aionemu.gameserver.model.templates.challenge.ChallengeTaskTemplate. */
class ChallengeTaskTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::challenge
