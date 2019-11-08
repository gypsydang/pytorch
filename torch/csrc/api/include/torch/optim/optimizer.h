#pragma once

#include <ATen/core/TensorBody.h>
#include <ATen/core/ivalue.h>

#include <c10/util/flat_hash_map.h>

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <set>

using c10::Dict;

// Forward declarations confuse Doxygen
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace at {
class Tensor;
} // namespace at

namespace torch {
using at::Tensor;
namespace serialize {
class OutputArchive;
class InputArchive;
} // namespace serialize
} // namespace torch
#endif // DOXYGEN_SHOULD_SKIP_THIS

namespace torch {
namespace optim {
namespace detail {
/// Base class for all optimizers, that does not yet define a `step()`
/// mechanism. All it specifies is that optimizers must be supplied with a
/// vector of parameters. It also defines certain methods that all optimizers
/// shall have, such as `zero_grad`.

// TODO `int64_t` used here are dummy types. We can replace it with something else.
template <typename OptimizerParamState = int64_t, typename OptimizerParamGroup = int64_t, typename OptimizerOptions = int64_t>
class TORCH_API OptimizerBase {
 public:
  /// Constructs the `Optimizer` from a vector of parameters.
  explicit OptimizerBase(std::vector<Tensor> parameters);

  //todo
  explicit OptimizerBase(std::vector<OptimizerParamGroup> param_groups, OptimizerOptions defaults) : defaults_(defaults) {
    for (auto& param_group : param_groups) {
      add_param_group(param_group);
    }
  }

  explicit OptimizerBase(std::vector<Tensor> params, OptimizerOptions defaults)
    : OptimizerBase({OptimizerParamGroup(params)}, defaults) {}

  void add_param_group(OptimizerParamGroup param_group) {
    for (const auto& param : param_group.params()) {
      TORCH_CHECK(param.is_leaf(), "can't optimize a non-leaf Tensor");
    }
    if (!param_group.has_options()) {
      param_group.set_options(defaults_);
    }
    // TODO: check "some parameters appear in more than one parameter group"
    param_groups_.push_back(param_group);
  }

  virtual ~OptimizerBase() = default;

  // TODO: when all optimizers use the new design, we can devirtualize some of the following methods
  // such as add_parameters() / parameters() / size()

  /// Adds the given vector of parameters to the optimizer's parameter list.
  virtual void add_parameters(const std::vector<Tensor>& parameters);

  /// Zeros out the gradients of all parameters.
  virtual void zero_grad();

  /// Provides a const reference to the parameters this optimizer holds.
  virtual const std::vector<Tensor>& parameters() const noexcept;

  /// Provides a reference to the parameters this optimizer holds.
  virtual std::vector<Tensor>& parameters() noexcept;

  /// Returns the number of parameters referenced by the optimizer.
  virtual size_t size() const noexcept;

  /// Serializes the optimizer state into the given `archive`.
  virtual void save(serialize::OutputArchive& archive) const;

  /// Deserializes the optimizer state from the given `archive`.
  virtual void load(serialize::InputArchive& archive);

 protected:
  OptimizerBase() = default;

  /// Accesses a buffer at the given index.
  /// Additionally, zeros out the buffers when this is called on the index
  template <typename T>
  T& buffer_at(std::vector<T>& buffers, size_t index) {
    if (buffers.size() <= index) {
      const auto old_size = buffers.size();
      buffers.resize(index + 1);
      std::fill(buffers.begin() + old_size, buffers.end(), T{0});
    }
    return buffers[index];
  }

  /// Accesses a buffer at the given index, converts it to the type of the
  /// parameter at the corresponding index (a no-op if they match).
  /// Additionally, zeros out the buffers when this is called on the index
  Tensor& buffer_at(std::vector<Tensor>& buffers, size_t index);

  /// The parameters this optimizer optimizes.
  std::vector<Tensor> parameters_;
  //to do-description
  OptimizerOptions defaults_;
  std::vector<OptimizerParamGroup> param_groups_;
  c10::Dict<at::Tensor, OptimizerParamState> state_; // yf225: maybe flat_hash_map?
};

/// Serializes an `OptimizerBase` into an `OutputArchive`.
TORCH_API serialize::OutputArchive& operator<<(
    serialize::OutputArchive& archive,
    const OptimizerBase<>& optimizer);

/// Deserializes a `Tensor` from an `InputArchive`.
TORCH_API serialize::InputArchive& operator>>(
    serialize::InputArchive& archive,
    OptimizerBase<>& optimizer);
} // namespace detail

/// Optimizer that defines a required `step()` method that takes no arguments
/// and produces no values. The only side effect is that parameters are updated
/// according to the concrete optimization algorithm.

template <typename OptimizerParamState = int64_t, typename OptimizerParamGroup = int64_t, typename OptimizerOptions = int64_t>
class Optimizer : public detail::OptimizerBase<OptimizerParamState, OptimizerParamGroup, OptimizerOptions> {
 public:
  using detail::OptimizerBase<OptimizerParamState, OptimizerParamGroup, OptimizerOptions>::OptimizerBase;
  virtual void step() = 0;
};

/// Optimizer that requires the loss function to be supplied to the `step()`
/// function, as it may evaluate the loss function multiple times per step.
/// Examples of such algorithms are conjugate gradient and LBFGS. The `step()`
/// function also returns the loss value.
class LossClosureOptimizer : public detail::OptimizerBase<> {
 public:
  /// A loss function closure, which is expected to return the loss value.
  using LossClosure = std::function<Tensor()>;
  using detail::OptimizerBase<>::OptimizerBase;
  virtual Tensor step(LossClosure closure) = 0;
};

} // namespace optim
} // namespace torch
