#include "aion/gameserver/configs/main/GSConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void GSConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.country.code", SERVER_COUNTRY_CODE, "99");
	AION_BIND(p, "gameserver.players.max.level", PLAYER_MAX_LEVEL, "65");
	AION_BIND(p, "gameserver.timezone", TIME_ZONE_ID);
	AION_BIND(p, "gameserver.chatserver.enable", ENABLE_CHAT_SERVER, "false");
	AION_BIND(p, "gameserver.chatserver.min_level", CHAT_SERVER_MIN_LEVEL, "10");
	AION_BIND(p, "gameserver.character.creation.mode", CHARACTER_CREATION_MODE, "0");
	AION_BIND(p, "gameserver.character.limit.count", CHARACTER_LIMIT_COUNT, "8");
	AION_BIND(p, "gameserver.character.faction.limitation.mode", CHARACTER_FACTION_LIMITATION_MODE, "0");
	AION_BIND(p, "gameserver.ratio.limitation.enable", ENABLE_RATIO_LIMITATION, "false");
	AION_BIND(p, "gameserver.ratio.min.value", RATIO_MIN_VALUE, "60");
	AION_BIND(p, "gameserver.ratio.min.required.level", RATIO_MIN_REQUIRED_LEVEL, "10");
	AION_BIND(p, "gameserver.ratio.min.characters_count", RATIO_MIN_CHARACTERS_COUNT, "50");
	AION_BIND(p, "gameserver.ratio.high_player_count.disabling", RATIO_HIGH_PLAYER_COUNT_DISABLING, "500");
	AION_BIND(p, "gameserver.character.reentry.time", CHARACTER_REENTRY_TIME, "20");
	AION_BIND(p, "gameserver.min_skill_cast_interval_millis", MIN_SKILL_CAST_INTERVAL_MILLIS, "350");
	AION_BIND(p, "gameserver.item_wrap_limit", ITEM_WRAP_LIMIT, "0");
	AION_BIND(p, "gameserver.web_rewards.enable", ENABLE_WEB_REWARDS, "false");
	AION_BIND(p, "gameserver.analysis.quest_handlers", ANALYZE_QUESTHANDLERS, "true");
	AION_BIND(p, "gameserver.quest.handler_directory", QUEST_HANDLER_DIRECTORY, "./data/handlers/quest");
}

} // namespace aion::gameserver::configs::main
