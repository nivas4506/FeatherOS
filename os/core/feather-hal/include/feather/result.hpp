/*
 * Simple Result type for C++20 — replacement for std::expected (C++23)
 * 
 * Provides Result<T, E> with value_or_error semantics.
 * Uses tag dispatch to avoid ambiguity when T == E.
 */

#pragma once

#include <variant>
#include <string>
#include <utility>
#include <functional>
#include <type_traits>

namespace feather::hal {

// Helper for void results
struct Void { };

// Tag types for explicit construction
struct SuccessTag { explicit SuccessTag() = default; };
struct ErrorTag { explicit ErrorTag() = default; };

template<typename T, typename E = std::string>
class Result {
    std::variant<T, E> data_;

    // Private constructor for internal use
    Result(SuccessTag, T&& value) : data_(std::in_place_index<0>, std::forward<T>(value)) {}
    Result(SuccessTag, const T& value) : data_(std::in_place_index<0>, value) {}
    Result(ErrorTag, E&& error) : data_(std::in_place_index<1>, std::forward<E>(error)) {}
    Result(ErrorTag, const E& error) : data_(std::in_place_index<1>, error) {}

public:
    // Public constructors - use SFINAE to avoid ambiguity when T == E
    template<typename U = T>
    Result(U&& value) 
        requires (!std::same_as<std::decay_t<U>, std::decay_t<E>>)
        : data_(std::in_place_index<0>, std::forward<U>(value)) {}

    template<typename U = T>
    Result(const U& value) 
        requires (!std::same_as<std::decay_t<U>, std::decay_t<E>>)
        : data_(std::in_place_index<0>, value) {}

    template<typename U = E>
    Result(U&& error) 
        requires (!std::same_as<std::decay_t<U>, std::decay_t<T>>)
        : data_(std::in_place_index<1>, std::forward<U>(error)) {}

    template<typename U = E>
    Result(const U& error) 
        requires (!std::same_as<std::decay_t<U>, std::decay_t<T>>)
        : data_(std::in_place_index<1>, error) {}

    // Factory methods for explicit construction (always work)
    static Result ok(T&& value) { return Result(SuccessTag{}, std::forward<T>(value)); }
    static Result ok(const T& value) { return Result(SuccessTag{}, value); }
    static Result err(E&& error) { return Result(ErrorTag{}, std::forward<E>(error)); }
    static Result err(const E& error) { return Result(ErrorTag{}, error); }

    // Check if success
    bool has_value() const noexcept { return data_.index() == 0; }
    bool has_error() const noexcept { return data_.index() == 1; }

    // Access value (UB if error)
    T& value() & { return std::get<0>(data_); }
    const T& value() const& { return std::get<0>(data_); }
    T&& value() && { return std::get<0>(std::move(data_)); }

    // Access error (UB if success)
    E& error() & { return std::get<1>(data_); }
    const E& error() const& { return std::get<1>(data_); }
    E&& error() && { return std::get<1>(std::move(data_)); }

    // Get value or throw
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(*this).value(); }

    // Get pointer to value (nullptr if error)
    T* operator->() { return has_value() ? &value() : nullptr; }
    const T* operator->() const { return has_value() ? &value() : nullptr; }

    // Monadic operations
    template<typename F>
    auto and_then(F&& f) & {
        using U = std::invoke_result_t<F, T&>;
        if (has_value()) return f(value());
        return U(ErrorTag{}, error());
    }

    template<typename F>
    auto and_then(F&& f) && {
        using U = std::invoke_result_t<F, T&&>;
        if (has_value()) return f(std::move(*this).value());
        return U(ErrorTag{}, std::move(*this).error());
    }

    template<typename F>
    auto or_else(F&& f) & {
        using U = std::invoke_result_t<F, E&>;
        if (has_error()) return f(error());
        return U(SuccessTag{}, value());
    }

    template<typename F>
    auto or_else(F&& f) && {
        using U = std::invoke_result_t<F, E&&>;
        if (has_error()) return f(std::move(*this).error());
        return U(SuccessTag{}, std::move(*this).value());
    }

    // Transform value
    template<typename F>
    auto transform(F&& f) & {
        using U = std::invoke_result_t<F, T&>;
        if (has_value()) return Result<U, E>::ok(f(value()));
        return Result<U, E>::err(error());
    }

    template<typename F>
    auto transform(F&& f) && {
        using U = std::invoke_result_t<F, T&&>;
        if (has_value()) return Result<U, E>::ok(f(std::move(*this).value()));
        return Result<U, E>::err(std::move(*this).error());
    }

    // Transform error
    template<typename F>
    auto transform_error(F&& f) & {
        using U = std::invoke_result_t<F, E&>;
        if (has_error()) return Result<T, U>::err(f(error()));
        return Result<T, U>::ok(value());
    }

    template<typename F>
    auto transform_error(F&& f) && {
        using U = std::invoke_result_t<F, E&&>;
        if (has_error()) return Result<T, U>::err(f(std::move(*this).error()));
        return Result<T, U>::ok(std::move(*this).value());
    }

    // Value or default
    T value_or(T&& default_value) & {
        return has_value() ? value() : std::forward<T>(default_value);
    }

    T value_or(T&& default_value) && {
        return has_value() ? std::move(*this).value() : std::forward<T>(default_value);
    }
};

// Convenience alias for common case
template<typename T>
using Expected = Result<T, std::string>;

using ExpectedVoid = Expected<Void>;

} // namespace feather::hal