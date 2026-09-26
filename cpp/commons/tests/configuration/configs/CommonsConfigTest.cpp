#include "aion/commons/configs/CommonsConfig.h"

#include <gtest/gtest.h>

#include "aion/commons/configuration/ConfigurableProcessor.h"

using namespace aion::commons;
using namespace aion::commons::configs;
using configuration::ConfigurableProcessor;
using configuration::Properties;

TEST(CommonsConfigTest, DefaultsAndJavaKeys) {
	CommonsConfig::RUNNABLESTATS_ENABLE = true;
	EXPECT_TRUE(ConfigurableProcessor::process(Properties(), {&CommonsConfig::bind}).empty());
	EXPECT_FALSE(CommonsConfig::RUNNABLESTATS_ENABLE);

	Properties p;
	p.setProperty("commons.runnablestats.enable", "TRUE");
	p.setProperty("commons.script_compiler.caching.enable", "false"); // not ported, but consumed like in Java
	EXPECT_TRUE(ConfigurableProcessor::process(p, {&CommonsConfig::bind}).empty());
	EXPECT_TRUE(CommonsConfig::RUNNABLESTATS_ENABLE);

	p.setProperty("commons.script_compiler.caching.enable", "maybe"); // still validated, like in Java
	EXPECT_THROW(ConfigurableProcessor::process(p, {&CommonsConfig::bind}), configuration::TransformationException);
	CommonsConfig::RUNNABLESTATS_ENABLE = false;
}
