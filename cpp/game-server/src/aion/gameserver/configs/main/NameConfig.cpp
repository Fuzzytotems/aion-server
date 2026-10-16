#include "aion/gameserver/configs/main/NameConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void NameConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.name.allow.custom", ALLOW_CUSTOM_NAMES, "false");
	AION_BIND(p, "gameserver.name.character_pattern", CHAR_NAME_PATTERN, "[a-zA-Z]{2,16}");
	AION_BIND(p, "gameserver.name.pet_pattern", PET_NAME_PATTERN, "[a-zA-Z]{2,16}");
	AION_BIND(p, "gameserver.name.forbidden_sequences_pattern", FORBIDDEN_SEQUENCE_PATTERN);
	AION_BIND(p, "gameserver.name.forbidden_words", FORBIDDEN_WORDS);
	AION_BIND(p, "gameserver.name.reserve_old_name_days", RESERVE_OLD_NAME_DAYS, "30");
}

} // namespace aion::gameserver::configs::main
