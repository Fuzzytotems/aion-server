#pragma once

#include "aion/gameserver/model/templates/mail/SysMail.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.SysMail. @author Rolandas */
class SysMail : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/SysMail.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::mail
