#include "aion/gameserver/model/templates/L10n.h"

#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::model::templates {

std::string L10n::getL10n() const {
	return utils::ChatUtil::l10n(getL10nId());
}

} // namespace aion::gameserver::model::templates
