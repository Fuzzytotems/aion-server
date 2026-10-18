#pragma once

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/gameserver/configs/detail/JavaString.h"

/**
 * Binding macros of the game server config classes (design handlers-and-porting-plan.md §1.10, "Configure").
 * <p>
 * AION_BIND(processor, key, FIELD[, defaultValue]) is Java's {@code @Property(key = key[, defaultValue = defaultValue])} on FIELD, and
 * AION_BIND_PATTERN(processor, keyPattern, FIELD) is {@code @Properties(keyPattern = keyPattern)}. They expand to
 * ConfigurableProcessor::bindNamed / bindPatternNamed with #FIELD and the game server's value formatter (detail/JavaString.h): a normal process
 * binds exactly like bind / bindPattern, and ConfigurableProcessor::describe lists the fields by name for //configure (C2 cp4, 2026-10-18).
 * <p>
 * Include only from config .cpp files. Fields of type CronExpression additionally need the transformer declared in
 * aion/gameserver/services/cron/CronService.h.
 * <p>
 * Not in Java.
 */

#define AION_BIND(processor, key, FIELD, ...) \
	(processor).template bindNamed<::aion::gameserver::configs::detail::JavaString>(#FIELD, key, FIELD __VA_OPT__(, ) __VA_ARGS__)

#define AION_BIND_PATTERN(processor, keyPattern, FIELD) \
	(processor).template bindPatternNamed<::aion::gameserver::configs::detail::JavaString>(#FIELD, keyPattern, FIELD)
