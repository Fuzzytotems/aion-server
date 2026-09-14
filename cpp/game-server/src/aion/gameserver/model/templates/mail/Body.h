#pragma once

#include "aion/gameserver/model/templates/mail/Body.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Body. @author Rolandas */
class Body : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Body.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
