#pragma once

#include "aion/gameserver/model/templates/mail/Header.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Header. @author Rolandas */
class Header : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Header.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
