#include "aion/gameserver/configs/administration/AdminConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::administration {

void AdminConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.administration.customtags", NAME_TAGS,
	          "%s, \u00BBJDev\u00AB\uE04A%s, \u00BBDev\u00AB\uE04A%s, \u00BBJEM\u00AB\uE04A%s, \u00BBEM\u00AB\uE04A%s, \u00BBJGM\u00AB\uE04A%s, "
	          "\u00BBGM\u00AB\uE04A%s, \u00BBSGM\u00AB\uE04A%s, \u00BBAdmin\u00AB\uE04A%s");
	AION_BIND(p, "gameserver.administration.unrestricted_itemtrade", UNRESTRICTED_ITEMTRADE, "1");
	AION_BIND(p, "gameserver.administration.gm_panel", GM_PANEL, "2");
	AION_BIND(p, "gameserver.administration.gm_skills", GM_SKILLS, "8");
	AION_BIND(p, "gameserver.administration.flight.free_fly", FREE_FLIGHT, "1");
	AION_BIND(p, "gameserver.administration.flight.unlimited_time", UNLIMITED_FLIGHT_TIME, "1");
	AION_BIND(p, "gameserver.administration.auto_res", AUTO_RES, "1");
	AION_BIND(p, "gameserver.administration.view_player_details", VIEW_PLAYER_DETAILS, "5");
	AION_BIND(p, "gameserver.administration.instance.enter_all", INSTANCE_ENTER_ALL, "2");
	AION_BIND(p, "gameserver.administration.instance.open_doors", INSTANCE_OPEN_DOORS, "6");
	AION_BIND(p, "gameserver.administration.instance.door_info", INSTANCE_DOOR_INFO, "9");
	AION_BIND(p, "gameserver.administration.house.enter_all", HOUSE_ENTER_ALL, "9");
	AION_BIND(p, "gameserver.administration.house.show_address", HOUSE_SHOW_ADDRESS, "9");
	AION_BIND(p, "gameserver.administration.dialog_info", DIALOG_INFO, "9");
	AION_BIND(p, "gameserver.administration.enchant_info", ENCHANT_INFO, "9");
	AION_BIND(p, "gameserver.administration.zone_info", ZONE_INFO, "9");
	AION_BIND(p, "gameserver.administration.audit_info", AUDIT_INFO, "9");
	AION_BIND(p, "gameserver.administration.command.quest.advanced_parameters", CMD_QUEST_ADV_PARAMS, "9");
	AION_BIND(p, "gameserver.administration.login.execute_commands", LOGIN_EXECUTE_COMMANDS, "//invis, //invul, //enemy none, //see");
	AION_BIND(p, "gameserver.administration.login.print_revision", REVISION_INFO_ON_LOGIN, "9");
	AION_BIND(p, "gameserver.administration.login.announce_levels", ANNOUNCE_LEVELS, "*");
	AION_BIND(p, "gameserver.administration.login.announce_to_all_players", ANNOUNCE_LOGIN_TO_ALL_PLAYERS, "true");
	AION_BIND(p, "gameserver.administration.logout.announce_to_all_players", ANNOUNCE_LOGOUT_TO_ALL_PLAYERS, "true");
}

} // namespace aion::gameserver::configs::administration
