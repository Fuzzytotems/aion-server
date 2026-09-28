#pragma once

#include "aion/commons/configuration/ConfigValue.h"

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

/**
 * Common declarations of the game server config class headers (configs/administration, configs/main, configs/network).
 * <p>
 * Every config field is rebound while the server runs (Config::load on event start/stop and //reload config, CommandsConfig on
 * ChatProcessor.reload, later //configure), so the field types follow CONVENTIONS.md "Fields that can change while the server runs": scalars
 * (bool, numbers, enums, CronExpression and time zone pointers) are std::atomic&lt;T&gt;, everything else is ConfigValue&lt;T&gt;. There are no
 * startup-only exceptions: no field is provably read only before other threads exist (the handler directories, pool sizes and network addresses
 * are also read by //reload, //sys and the LS/CS reconnect tasks), and an atomic read costs nothing measurable here.
 * <p>
 * Not in Java.
 */
namespace aion::gameserver::configs {

using commons::configuration::ConfigValue;

} // namespace aion::gameserver::configs
