#include <torch/optim/optimizer.h>

#include <torch/csrc/autograd/generated/variable_factories.h>
#include <torch/ordered_dict.h>
#include <torch/serialize/archive.h>
#include <torch/types.h>

#include <string>
#include <utility>
#include <vector>

namespace torch {
namespace optim {
namespace detail {
template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
OptimizerBase::OptimizerBase(std::vector<Tensor> parameters)
    : parameters_(std::move(parameters)) {}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
void OptimizerBase::add_parameters(const std::vector<Tensor>& parameters) {
  parameters_.insert(parameters_.end(), parameters.begin(), parameters.end());
}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
void OptimizerBase::zero_grad() {
  for (auto& parameter : parameters_) {
    if (parameter.grad().defined()) {
      parameter.grad().detach_();
      parameter.grad().zero_();
    }
  }
  for (auto& group : param_groups) {
    for (auto& p : group.params()) {
      if(p.grad().defined()) {
        p.grad().detach_();
        p.grad().zero_();
      }
    }
  }
}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
const std::vector<Tensor>& OptimizerBase::parameters() const noexcept {
  return parameters_;
}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
std::vector<Tensor>& OptimizerBase::parameters() noexcept {
  return parameters_;
}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
size_t OptimizerBase::size() const noexcept {
  return parameters_.size();
}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
Tensor& OptimizerBase::buffer_at(std::vector<Tensor>& buffers, size_t index) {
  if (buffers.size() <= index) {
    buffers.reserve(index);
    for (auto i = buffers.size(); i <= index; ++i) {
      buffers.push_back(torch::zeros_like(parameters_.at(i)));
    }
  }
  // Copy the buffer to the device and dtype of the parameter.
  const auto& parameter = parameters_.at(index);
  const auto& buffer = buffers.at(index);
  if (buffer.device() != parameter.device() ||
      buffer.dtype() != parameter.dtype()) {
    buffers[index] = buffer.to(parameter.device(), parameter.scalar_type());
  }
  return buffers[index];
}

template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
void OptimizerBase::save(serialize::OutputArchive& archive) const {}
template <typename OptimizerParamState, typename OptimizerParamGroup, typename OptimizerOptions>
void OptimizerBase::load(serialize::InputArchive& archive) {}

/// Serializes an `OptimizerBase` into an `OutputArchive`.
serialize::OutputArchive& operator<<(
    serialize::OutputArchive& archive,
    const OptimizerBase<>& optimizer) {
  optimizer.save(archive);
  return archive;
}

/// Deserializes a `Tensor` from an `InputArchive`.
serialize::InputArchive& operator>>(
    serialize::InputArchive& archive,
    OptimizerBase<>& optimizer) {
  optimizer.load(archive);
  return archive;
}
} // namespace detail
} // namespace optim
} // namespace torch
