#pragma once

#include "aion/gameserver/model/templates/mail/Title.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Title. @author Rolandas */
class Title : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Title.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
