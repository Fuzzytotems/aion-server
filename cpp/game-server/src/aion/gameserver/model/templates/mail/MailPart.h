#pragma once

#include "aion/gameserver/model/templates/mail/MailPart.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.MailPart. @author Rolandas */
class MailPart : public ::aion::gameserver::model::templates::mail::StringParamList {
#include "aion/gameserver/model/templates/mail/MailPart.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
