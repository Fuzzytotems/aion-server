#include "aion/gameserver/configs/main/SecurityConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void SecurityConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.security.aion.bin.check", AION_BIN_CHECK, "false");
	AION_BIND(p, "gameserver.security.antihack.teleportation", TELEPORTATION, "false");
	AION_BIND(p, "gameserver.security.antihack.speedhack", SPEEDHACK, "false");
	AION_BIND(p, "gameserver.security.antihack.speedhack.counter", SPEEDHACK_COUNTER, "1");
	AION_BIND(p, "gameserver.security.antihack.abnormal", ABNORMAL, "false");
	AION_BIND(p, "gameserver.security.antihack.abnormal.counter", ABNORMAL_COUNTER, "1");
	AION_BIND(p, "gameserver.security.antihack.punish", PUNISH, "0");
	AION_BIND(p, "gameserver.security.check_animations", CHECK_ANIMATIONS, "true");
	AION_BIND(p, "gameserver.security.captcha.enable", CAPTCHA_ENABLE, "false");
	AION_BIND(p, "gameserver.security.captcha.appear", CAPTCHA_APPEAR, "OD");
	AION_BIND(p, "gameserver.security.captcha.appear.rate", CAPTCHA_APPEAR_RATE, "5");
	AION_BIND(p, "gameserver.security.captcha.extraction.ban.time", CAPTCHA_EXTRACTION_BAN_TIME, "3000");
	AION_BIND(p, "gameserver.security.captcha.extraction.ban.add.time", CAPTCHA_EXTRACTION_BAN_ADD_TIME, "600");
	AION_BIND(p, "gameserver.security.captcha.bonus.fp.time", CAPTCHA_BONUS_FP_TIME, "5");
	AION_BIND(p, "gameserver.security.passkey.enable", PASSKEY_ENABLE, "false");
	AION_BIND(p, "gameserver.security.passkey.wrong.maxcount", PASSKEY_WRONG_MAXCOUNT, "5");
	AION_BIND(p, "gameserver.security.pingcheck.kick", PINGCHECK_KICK, "true");
	AION_BIND(p, "gameserver.security.flood.delay", FLOOD_DELAY, "1");
	AION_BIND(p, "gameserver.security.flood.msg", FLOOD_MSG, "6");
	AION_BIND(p, "gameserver.security.validation.flypath", ENABLE_FLYPATH_VALIDATOR, "false");
	AION_BIND(p, "gameserver.security.survey.delay.minute", SURVEY_DELAY, "20");
	AION_BIND(p, "gameserver.security.multi_clienting.restriction_mode", MULTI_CLIENTING_RESTRICTION_MODE, "NONE");
	AION_BIND(p, "gameserver.security.multi_clienting.ignored_mac_addresses", MULTI_CLIENTING_IGNORED_MAC_ADDRESSES, "");
	AION_BIND(p, "gameserver.security.multi_clienting.faction_switch_cooldown_minutes", MULTI_CLIENTING_FACTION_SWITCH_COOLDOWN_MINUTES, "20");
	AION_BIND(p, "gameserver.security.hdd_serial_lock.enable", HDD_SERIAL_LOCK_ENABLE, "false");
	AION_BIND(p, "gameserver.security.hdd_serial_lock.auto_lock", HDD_SERIAL_LOCK_UNLOCKED_ACCOUNTS, "false");
}

} // namespace aion::gameserver::configs::main
