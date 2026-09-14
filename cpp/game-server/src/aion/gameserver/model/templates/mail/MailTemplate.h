#pragma once

#include "aion/gameserver/model/templates/mail/MailTemplate.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.MailTemplate. @author Rolandas */
class MailTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/MailTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
