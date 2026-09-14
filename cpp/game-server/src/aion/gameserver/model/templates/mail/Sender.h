#pragma once

#include "aion/gameserver/model/templates/mail/Sender.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Sender. @author Rolandas */
class Sender : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Sender.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
