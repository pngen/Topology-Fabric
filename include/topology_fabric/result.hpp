
// TopologyFabric/result.hpp - lightweight error/result types.
#pragma once
#include <string>
#include <stdexcept>
#include <optional>
#include <utility>

namespace topology_fabric {

enum class ErrorCode : uint8_t {
  OK = 0,
  INVALID_ARGUMENT,
  NOT_FOUND,
  STALE_HANDLE,
  INVALID_GRAPH,
  MALFORMED_DATA,
  OVERSIZED,
  UNSUPPORTED,
  PROVIDER_FAILED,
  INTERNAL,
  VALIDATION_FAILED
};

struct Error {
  ErrorCode code = ErrorCode::OK;
  std::string message;
  bool ok() const noexcept { return code == ErrorCode::OK; }
  explicit operator bool() const noexcept { return ok(); }
};

inline Error make_error(ErrorCode code, std::string msg) { return Error{code, std::move(msg)}; }

// Exception type for programmatic failures.
class TopologyError : public std::runtime_error {
 public:
  explicit TopologyError(std::string msg) : std::runtime_error(std::move(msg)) {}
  explicit TopologyError(ErrorCode code, std::string msg)
      : std::runtime_error(std::move(msg)), code_(code) {}
  ErrorCode code() const noexcept { return code_; }
 private:
  ErrorCode code_ = ErrorCode::INTERNAL;
};

// A minimal Result<T> (std::expected is C++23; provide our own).
template <typename T>
class Result {
 public:
  Result(T value) : value_(std::move(value)), ok_(true) {}
  Result(Error err) : error_(std::move(err)), ok_(false) {}
  static Result failure(Error err) { return Result(err); }
  bool ok() const noexcept { return ok_; }
  bool has_value() const noexcept { return ok_; }
  T& value() { return value_; }
  const T& value() const { return value_; }
  const Error& error() const { return error_; }
  T value_or(T fallback) const { return ok_ ? value_ : std::move(fallback); }
  T& operator*() { return value_; }
  const T& operator*() const { return value_; }
  T* operator->() { return &value_; }
  const T* operator->() const { return &value_; }
 private:
  T value_{};
  Error error_;
  bool ok_ = false;
};

}  // namespace topology_fabric
