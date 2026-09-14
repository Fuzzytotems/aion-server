#pragma once

#include "aion/gameserver/model/templates/mail/StringParamList.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.StringParamList. @author Rolandas */
class StringParamList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/StringParamList.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
