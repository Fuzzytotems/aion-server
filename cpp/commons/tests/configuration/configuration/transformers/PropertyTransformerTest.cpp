#include <memory>

#include <gtest/gtest.h>

#include "aion/commons/configuration/ConfigurableProcessor.h"

using namespace aion::commons;
using namespace aion::commons::configuration;

namespace test {

/** Stand-in for a server specific type like the game server's CronExpression. */
struct Schedule {
	std::string expression;
	bool operator==(const Schedule&) const = default;
};

/** A type whose transformer does not provide a type name. */
struct Unnamed {
	int32_t value = 0;
};

} // namespace test

// Java: PropertyTransformers.register(new CronExpressionTransformer())
namespace aion::commons::configuration::transformers {

template <>
struct PropertyTransformer<test::Schedule> {
	static std::string typeName() { return "CronExpression"; }

	static test::Schedule parseObject(std::string_view value) {
		if (value.find('?') == std::string_view::npos)
			throw utils::IllegalArgumentException("'?' expected");
		return test::Schedule{std::string(value)};
	}
};

/** Nullable variant sharing cached instances, like CronExpressions.getOrCreate (empty value = nullptr). */
template <>
struct PropertyTransformer<std::shared_ptr<const test::Schedule>> {
	static std::string typeName() { return "CronExpression"; }

	static std::shared_ptr<const test::Schedule> parseObject(std::string_view value) {
		return value.empty() ? nullptr : std::make_shared<const test::Schedule>(PropertyTransformer<test::Schedule>::parseObject(value));
	}
};

template <>
struct PropertyTransformer<test::Unnamed> {
	static test::Unnamed parseObject(std::string_view value) { return test::Unnamed{static_cast<int32_t>(value.size())}; }
};

} // namespace aion::commons::configuration::transformers

TEST(PropertyTransformerTest, CustomTypesWorkInBindingsAndContainers) {
	Properties p;
	p.setProperty("single", "0 0 12 ? * SUN");
	p.setProperty("list", "\"0 0 0,12,20 ? * *\", 0 0 23 ? * *");
	p.setProperty("disabled", "");
	p.setProperty("shared", "0 0 5 ? * WED");
	p.setProperty("unnamed", "abc");

	test::Schedule single;
	std::vector<test::Schedule> list;
	std::optional<test::Schedule> disabled = test::Schedule{"x"};
	std::shared_ptr<const test::Schedule> shared, missing;
	test::Unnamed unnamed;
	std::set<std::string> unused = ConfigurableProcessor::process(p, {[&](ConfigurableProcessor& processor) {
		                                                              processor.bind("single", single);
		                                                              processor.bind("list", list);
		                                                              processor.bind("disabled", disabled);
		                                                              processor.bind("shared", shared);
		                                                              processor.bind("missing", missing, "");
		                                                              processor.bind("unnamed", unnamed);
	                                                              }});
	EXPECT_TRUE(unused.empty());
	EXPECT_EQ(single.expression, "0 0 12 ? * SUN");
	EXPECT_EQ(list, (std::vector<test::Schedule>{{"0 0 0,12,20 ? * *"}, {"0 0 23 ? * *"}}));
	EXPECT_FALSE(disabled.has_value());
	ASSERT_TRUE(shared);
	EXPECT_EQ(shared->expression, "0 0 5 ? * WED");
	EXPECT_FALSE(missing);
	EXPECT_EQ(unnamed.value, 3);
}

TEST(PropertyTransformerTest, TypeNamesInErrors) {
	EXPECT_EQ(transformers::typeName<std::vector<test::Schedule>>(), "CronExpression[]");
	EXPECT_EQ(transformers::typeName<test::Unnamed>(), "test::Unnamed"); // fallback: C++ type name
	try {
		ConfigurableProcessor::transform<std::vector<test::Schedule>>("0 0 12 * * SUN");
		FAIL();
	} catch (const TransformationException& e) {
		EXPECT_STREQ(e.what(), "Error parsing \"0 0 12 * * SUN\" as CronExpression[]");
	}
}

// unsupported field types are compile errors
static_assert(transformers::Transformable<int32_t>);
static_assert(transformers::Transformable<std::optional<std::regex>>);
static_assert(transformers::Transformable<std::shared_ptr<const test::Schedule>>);
static_assert(!transformers::Transformable<char>);
static_assert(!transformers::Transformable<const char*>);
static_assert(!transformers::Transformable<std::map<std::string, int32_t>>);
static_assert(!transformers::Transformable<std::vector<char>>);
static_assert(transformers::TransformableMap<std::map<std::string, int32_t>>);
static_assert(!transformers::TransformableMap<std::map<std::string, char>>);
