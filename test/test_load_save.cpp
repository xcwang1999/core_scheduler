#include <ATen/Context.h>
#include <ATen/ops/allclose.h>
#include <ATen/ops/cat.h>
#include <cuda_fp16.h>
#include <gtest/gtest.h>
#include <torch/nn/modules/normalization.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "compute/layer_norm.h"
#include "compute/utils.h"
#include "module/layer_norm.h"
#include "threading/dynamic_scheduler.h"

template <typename T>
struct TypeToTorch;

template <>
struct TypeToTorch<float> {
  using Type = float;
  static constexpr at::ScalarType type = at::kFloat;
};

template <>
struct TypeToTorch<nv_half> {
  using Type = c10::Half;
  static constexpr at::ScalarType type = at::kHalf;
};

template <>
struct TypeToTorch<double> {
  using Type = double;
  static constexpr at::ScalarType type = at::kDouble;
};

class TestLoadSaveFixture : public ::testing::Test {
 protected:
  dllm::DynamicScheduler scheduler{0};

  template <typename T>
  void TestRoutine(int size);
};

namespace {
std::string generate_unique_filename() {
  // 获取当前系统时间
  const auto now = std::chrono::system_clock::now();
  const auto time_t = std::chrono::system_clock::to_time_t(now);
  const auto local_tm = *std::localtime(&time_t);

  // 获取当前毫秒，确保唯一性
  const auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now.time_since_epoch()) %
                          1000;

  // 使用 std::stringstream 和 std::put_time 来格式化日期和时间
  std::stringstream ss;
  ss << std::put_time(&local_tm, "%Y%m%d_%H%M%S");

  // 使用 fmt::format 来构建最终的文件名
  return fmt::format("/tmp/test_time_{}_{:03d}_model.parquet", ss.str(),
                     current_ms.count());
}
}  // namespace

template <typename T>
void TestLoadSaveFixture::TestRoutine(const int size) {
  const auto path = generate_unique_filename();
  at::manual_seed(1);
  const at::Device device(at::kCUDA, 0);
  const at::ScalarType dtype = TypeToTorch<T>::type;

  dllm::module::LayerNorm ln{
      scheduler,
      dllm::module::LayerNorm::Options{{3 * size}}.device(device).dtype(dtype)};

  const auto weightClone =
      dllm::compute::Utils::clone(scheduler, ln->state()->forward.weight);

  dllm::save(ln, path);

  dllm::compute::Utils::zero_(scheduler, ln->state()->forward.weight);

  dllm::load(ln, path);

  std::remove(path.c_str());

  ASSERT_TRUE(at::allclose(weightClone, ln->state()->forward.weight));
}

TEST_F(TestLoadSaveFixture, TestRoutineF32) { TestRoutine<float>(128); }
TEST_F(TestLoadSaveFixture, TestRoutineF64) { TestRoutine<double>(128); }
