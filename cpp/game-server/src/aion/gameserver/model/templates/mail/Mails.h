#pragma once

#include "aion/gameserver/model/templates/mail/Mails.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Mails. @author Rolandas */
class Mails : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/Mails.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
