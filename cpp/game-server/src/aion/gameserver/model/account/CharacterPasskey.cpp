#include "aion/gameserver/model/account/CharacterPasskey.h"

namespace aion::gameserver::model::account {

CharacterPasskey::CharacterPasskey() = default;

CharacterPasskey::~CharacterPasskey() = default;

runtime::Ref<CharacterPasskey> CharacterPasskey::create() {
	return runtime::makeRef<CharacterPasskey>();
}

} // namespace aion::gameserver::model::account
