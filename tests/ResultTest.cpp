/**
 * @file ResultTest.cpp
 * @brief Specification-based unit tests for util::Result<T,E>.
 *
 * Tests are derived from the documented API contract, not the implementation.
 *
 * Covered specifications:
 *  - Ok() and Err() factory functions construct the correct variant.
 *  - isOk() / isErr() return the correct boolean.
 *  - operator bool() is equivalent to isOk().
 *  - unwrap() returns the value from an Ok result.
 *  - unwrap() throws std::logic_error on an Err result.
 *  - unwrapOr() returns the value when Ok, or the default when Err.
 *  - unwrapOrElse() calls the function only when Err.
 *  - error() returns the error from an Err result.
 *  - error() throws std::logic_error on an Ok result.
 *  - ok()  → std::optional<T>: has_value when Ok, empty when Err.
 *  - err() → std::optional<E>: has_value when Err, empty when Ok.
 *  - map() transforms the value and leaves errors unchanged.
 *  - mapErr() transforms the error and leaves values unchanged.
 *  - andThen() chains Ok results; short-circuits on Err.
 *  - orElse() provides an alternative result when Err; passes Ok through.
 */

#include <gtest/gtest.h>
#include "Result.hpp"
#include "Error.hpp"

#include <string>
#include <stdexcept>

using util::Result;

// Convenience type aliases used throughout the tests
using IntStrResult = Result<int, std::string>;

// ============================================================================
// Ok / isOk / isErr / operator bool
// ============================================================================

/**
 * @brief Spec: Ok() constructs a successful result; isOk() returns true and
 * isErr() returns false.
 */
TEST(ResultTest, OkResultIsOk)
{
    auto r = IntStrResult::Ok(42);
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isErr());
}

/**
 * @brief Spec: Err() constructs an error result; isErr() returns true and
 * isOk() returns false.
 */
TEST(ResultTest, ErrResultIsErr)
{
    auto r = IntStrResult::Err("oops");
    EXPECT_TRUE(r.isErr());
    EXPECT_FALSE(r.isOk());
}

/**
 * @brief Spec: operator bool() returns true for Ok, false for Err.
 */
TEST(ResultTest, OperatorBoolReflectsIsOk)
{
    EXPECT_TRUE(static_cast<bool>(IntStrResult::Ok(1)));
    EXPECT_FALSE(static_cast<bool>(IntStrResult::Err("e")));
}

// ============================================================================
// unwrap
// ============================================================================

/**
 * @brief Spec: unwrap() on an Ok result returns the contained value.
 */
TEST(ResultTest, UnwrapOkReturnsValue)
{
    auto r = IntStrResult::Ok(99);
    EXPECT_EQ(r.unwrap(), 99);
}

/**
 * @brief Spec: unwrap() on an Err result throws std::logic_error.
 */
TEST(ResultTest, UnwrapErrThrowsLogicError)
{
    auto r = IntStrResult::Err("failure");
    EXPECT_THROW(r.unwrap(), std::logic_error);
}

// ============================================================================
// unwrapOr
// ============================================================================

/**
 * @brief Spec: unwrapOr() returns the stored value when the result is Ok.
 */
TEST(ResultTest, UnwrapOrReturnsValueWhenOk)
{
    auto r = IntStrResult::Ok(7);
    EXPECT_EQ(r.unwrapOr(0), 7);
}

/**
 * @brief Spec: unwrapOr() returns the default value when the result is Err.
 */
TEST(ResultTest, UnwrapOrReturnsDefaultWhenErr)
{
    auto r = IntStrResult::Err("bad");
    EXPECT_EQ(r.unwrapOr(-1), -1);
}

// ============================================================================
// unwrapOrElse
// ============================================================================

/**
 * @brief Spec: unwrapOrElse() returns the stored value when Ok, never calling
 * the fallback function.
 */
TEST(ResultTest, UnwrapOrElseReturnsValueWhenOk)
{
    auto r = IntStrResult::Ok(5);
    bool called = false;
    int v = r.unwrapOrElse([&] { called = true; return -1; });
    EXPECT_EQ(v, 5);
    EXPECT_FALSE(called);
}

/**
 * @brief Spec: unwrapOrElse() calls the fallback function and returns its
 * result when Err.
 */
TEST(ResultTest, UnwrapOrElseCallsFallbackWhenErr)
{
    auto r = IntStrResult::Err("nope");
    bool called = false;
    int v = r.unwrapOrElse([&] { called = true; return 42; });
    EXPECT_EQ(v, 42);
    EXPECT_TRUE(called);
}

// ============================================================================
// error
// ============================================================================

/**
 * @brief Spec: error() on an Err result returns the contained error.
 */
TEST(ResultTest, ErrorReturnsContainedError)
{
    auto r = IntStrResult::Err("disk full");
    EXPECT_EQ(r.error(), "disk full");
}

/**
 * @brief Spec: error() on an Ok result throws std::logic_error.
 */
TEST(ResultTest, ErrorOnOkThrowsLogicError)
{
    auto r = IntStrResult::Ok(1);
    EXPECT_THROW(r.error(), std::logic_error);
}

// ============================================================================
// ok() / err() — optional conversion
// ============================================================================

/**
 * @brief Spec: ok() returns a non-empty optional containing the value when Ok.
 */
TEST(ResultTest, OkMethodReturnsNonEmptyOptionalWhenOk)
{
    auto r = IntStrResult::Ok(10);
    auto opt = r.ok();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 10);
}

/**
 * @brief Spec: ok() returns an empty optional when Err.
 */
TEST(ResultTest, OkMethodReturnsEmptyOptionalWhenErr)
{
    auto r = IntStrResult::Err("fail");
    EXPECT_FALSE(r.ok().has_value());
}

/**
 * @brief Spec: err() returns a non-empty optional containing the error when Err.
 */
TEST(ResultTest, ErrMethodReturnsNonEmptyOptionalWhenErr)
{
    auto r = IntStrResult::Err("timeout");
    auto opt = r.err();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), "timeout");
}

/**
 * @brief Spec: err() returns an empty optional when Ok.
 */
TEST(ResultTest, ErrMethodReturnsEmptyOptionalWhenOk)
{
    auto r = IntStrResult::Ok(3);
    EXPECT_FALSE(r.err().has_value());
}

// ============================================================================
// map
// ============================================================================

/**
 * @brief Spec: map() applies the function to the Ok value and wraps the result
 * in a new Ok.
 */
TEST(ResultTest, MapTransformsOkValue)
{
    auto r = IntStrResult::Ok(4);
    auto mapped = r.map([](int v) { return v * 2; });
    ASSERT_TRUE(mapped.isOk());
    EXPECT_EQ(mapped.unwrap(), 8);
}

/**
 * @brief Spec: map() passes an Err through unchanged — the function is never
 * called.
 */
TEST(ResultTest, MapPassesErrThrough)
{
    auto r = IntStrResult::Err("error");
    bool called = false;
    auto mapped = r.map([&](int v) { called = true; return v * 2; });
    EXPECT_TRUE(mapped.isErr());
    EXPECT_FALSE(called);
}

/**
 * @brief Spec: map() can change the value type (T → U).
 * Maps int → double; error type stays std::string so T ≠ E in both sides.
 */
TEST(ResultTest, MapCanChangeType)
{
    auto r = IntStrResult::Ok(5);
    auto mapped = r.map([](int v) { return static_cast<double>(v * 2); });
    ASSERT_TRUE(mapped.isOk());
    EXPECT_DOUBLE_EQ(mapped.unwrap(), 10.0);
}

// ============================================================================
// mapErr
// ============================================================================

/**
 * @brief Spec: mapErr() applies the function to the Err value and wraps the
 * result in a new Err.
 */
TEST(ResultTest, MapErrTransformsErrValue)
{
    auto r = IntStrResult::Err("oops");
    auto mapped = r.mapErr([](const std::string& e) { return "wrapped: " + e; });
    ASSERT_TRUE(mapped.isErr());
    EXPECT_EQ(mapped.error(), "wrapped: oops");
}

/**
 * @brief Spec: mapErr() passes an Ok through unchanged — the function is never
 * called.
 */
TEST(ResultTest, MapErrPassesOkThrough)
{
    auto r = IntStrResult::Ok(7);
    bool called = false;
    auto mapped = r.mapErr([&](const std::string& e) { called = true; return e + "!"; });
    EXPECT_TRUE(mapped.isOk());
    EXPECT_FALSE(called);
}

// ============================================================================
// andThen
// ============================================================================

/**
 * @brief Spec: andThen() chains a function on an Ok result and returns its
 * result.
 */
TEST(ResultTest, AndThenChainsOnOk)
{
    auto r = IntStrResult::Ok(3);
    auto chained = r.andThen([](int v) {
        return IntStrResult::Ok(v * 10);
    });
    ASSERT_TRUE(chained.isOk());
    EXPECT_EQ(chained.unwrap(), 30);
}

/**
 * @brief Spec: andThen() on an Err propagates the error — the function is
 * never called.
 */
TEST(ResultTest, AndThenShortCircuitsOnErr)
{
    auto r = IntStrResult::Err("bad");
    bool called = false;
    auto chained = r.andThen([&](int v) {
        called = true;
        return IntStrResult::Ok(v);
    });
    EXPECT_TRUE(chained.isErr());
    EXPECT_FALSE(called);
}

/**
 * @brief Spec: andThen() propagates errors returned from the chained function.
 */
TEST(ResultTest, AndThenPropagatesChainedError)
{
    auto r = IntStrResult::Ok(1);
    auto chained = r.andThen([](int) {
        return IntStrResult::Err("chained error");
    });
    ASSERT_TRUE(chained.isErr());
    EXPECT_EQ(chained.error(), "chained error");
}

// ============================================================================
// orElse
// ============================================================================

/**
 * @brief Spec: orElse() provides an alternative result when Err.
 */
TEST(ResultTest, OrElseProvidesAlternativeOnErr)
{
    auto r = IntStrResult::Err("fail");
    auto recovered = r.orElse([](const std::string&) {
        return IntStrResult::Ok(0);
    });
    ASSERT_TRUE(recovered.isOk());
    EXPECT_EQ(recovered.unwrap(), 0);
}

/**
 * @brief Spec: orElse() passes an Ok result through — the function is never
 * called.
 */
TEST(ResultTest, OrElsePassesOkThrough)
{
    auto r = IntStrResult::Ok(42);
    bool called = false;
    auto result = r.orElse([&](const std::string&) {
        called = true;
        return IntStrResult::Ok(0);
    });
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.unwrap(), 42);
    EXPECT_FALSE(called);
}

// ============================================================================
// Chaining multiple operations (integration)
// ============================================================================

/**
 * @brief Spec: multiple map/andThen calls compose correctly for an Ok result.
 */
TEST(ResultTest, ComposedChainSucceeds)
{
    auto r = IntStrResult::Ok(2)
        .map([](int v) { return v + 1; })              // 3
        .andThen([](int v) { return IntStrResult::Ok(v * 4); }); // 12

    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.unwrap(), 12);
}

/**
 * @brief Spec: an Err early in the chain propagates all the way through.
 */
TEST(ResultTest, ComposedChainShortCircuitsOnErr)
{
    auto r = IntStrResult::Err("initial error")
        .map([](int v) { return v + 1; })
        .andThen([](int v) { return IntStrResult::Ok(v * 4); });

    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), "initial error");
}
