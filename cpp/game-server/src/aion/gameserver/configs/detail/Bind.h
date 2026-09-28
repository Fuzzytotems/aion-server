#pragma once

#include "aion/commons/configuration/ConfigurableProcessor.h"

/**
 * Binding macros of the game server config classes (design handlers-and-porting-plan.md §1.10, "Configure").
 * <p>
 * AION_BIND(processor, key, FIELD[, defaultValue]) is Java's {@code @Property(key = key[, defaultValue = defaultValue])} on FIELD, and
 * AION_BIND_PATTERN(processor, keyPattern, FIELD) is {@code @Properties(keyPattern = keyPattern)}. Today they expand to
 * ConfigurableProcessor::bind / bindPattern. They exist so that the //configure port (reflective field access by name, deferred) can pass #FIELD
 * to an introspecting processor without touching the 33 bind functions again.
 * <p>
 * Include only from config .cpp files. Fields of type CronExpression additionally need the transformer declared in
 * aion/gameserver/services/cron/CronService.h.
 * <p>
 * Not in Java.
 */

#define AION_BIND(processor, key, FIELD, ...) (processor).bind(key, FIELD __VA_OPT__(, ) __VA_ARGS__)

#define AION_BIND_PATTERN(processor, keyPattern, FIELD) (processor).bindPattern(keyPattern, FIELD)
