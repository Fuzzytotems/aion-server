#pragma once

#include "aion/gameserver/model/templates/mail/Tail.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Tail. @author Rolandas */
class Tail : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Tail.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
