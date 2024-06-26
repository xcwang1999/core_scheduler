/*
 * Copyright (c) 2024 The Core team
 *
 * Licensed under the Apache License, Version 2.0;
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <Eigen/Dense>
#include <fstream>

#include "compute/relu.h"
#include "logger.h"
#include "tensor.h"

namespace Eigen::internal {
template <>
struct scalar_random_op<nv_half> {
  EIGEN_EMPTY_STRUCT_CTOR(scalar_random_op)
  inline const nv_half operator()() const {
    return static_cast<nv_half>(random<float>());
  }
};
}  // namespace Eigen::internal

class TestCSRelu : public ::testing::Test {
 protected:
  cs::ContextCompute context{};

  void SetUp() override {
    CHECK_CUDART(
        cudaStreamCreateWithFlags(&context.cudaStream, cudaStreamNonBlocking));
  }

  void TearDown() override {
    CHECK_CUDART(cudaStreamDestroy(context.cudaStream));
  }

  template <typename Element>
  void TestRoutine(const cs::TensorIndexType size);
};

template <typename Element>
void TestCSRelu::TestRoutine(const cs::TensorIndexType size) {
  Eigen::Vector<Element, Eigen::Dynamic> hostInput(size);
  Eigen::Vector<Element, Eigen::Dynamic> hostOutput(size);
  Eigen::Vector<Element, Eigen::Dynamic> hostRef(size);
  hostInput.setRandom();
  hostOutput.setZero();

  auto shape = cute::make_shape(size);
  auto layout = cute::make_layout(shape, cute::GenRowMajor{});

  void *DeviceInput, *DeviceOutput;
  CHECK_CUDART(cudaMalloc(&DeviceInput, sizeof(Element) * cute::size(layout)));
  CHECK_CUDART(cudaMalloc(&DeviceOutput, sizeof(Element) * cute::size(layout)));
  auto tensorInput = std::make_shared<cs::Tensor1D>(
      DeviceInput, layout, cs::toDtype<Element>(), cs::CUDA);
  auto tensorOutput = std::make_shared<cs::Tensor1D>(
      DeviceOutput, layout, cs::toDtype<Element>(), cs::CUDA);

  CHECK_CUDART(cudaMemcpy(DeviceInput, hostInput.data(),
                          sizeof(Element) * cute::size(layout),
                          cudaMemcpyHostToDevice));
  CHECK_CUDART(
      cudaMemset(DeviceOutput, 0, sizeof(Element) * cute::size(layout)));

  CHECK_CUDART(cudaDeviceSynchronize());
  hostRef =
      hostInput.unaryExpr([](Element x) { return std::max<Element>(0, x); });

  auto tast = cs::compute::ReLU::forward(tensorInput, tensorOutput);
  tast(&context);

  CHECK_CUDART(cudaMemcpy(hostOutput.data(), DeviceOutput,
                          sizeof(Element) * cute::size(layout),
                          cudaMemcpyDeviceToHost));
  CHECK_CUDART(cudaDeviceSynchronize());

  auto isApprox = hostOutput.isApprox(hostRef);

  if (!isApprox) {
    if constexpr (std::is_same_v<Element, nv_half>) {
      std::ofstream fileOuput("output.txt");
      fileOuput << hostOutput.template cast<float>() << std::endl;
      fileOuput.close();
      std::ofstream fileRef("ref.txt");
      fileRef << hostRef.template cast<float>() << std::endl;
      fileRef.close();
    } else {
      std::ofstream fileOuput("output.txt");
      fileOuput << hostOutput << std::endl;
      fileOuput.close();
      std::ofstream fileRef("ref.txt");
      fileRef << hostRef << std::endl;
      fileRef.close();
    }
  }

  ASSERT_TRUE(isApprox);
  CHECK_CUDART(cudaFree(DeviceInput));
  CHECK_CUDART(cudaFree(DeviceOutput));
}

TEST_F(TestCSRelu, TestF32_128) { TestRoutine<float>(128); }

TEST_F(TestCSRelu, TestF32_512) { TestRoutine<float>(512); }

TEST_F(TestCSRelu, TestF32_1024) { TestRoutine<float>(1024); }

TEST_F(TestCSRelu, TestF64_128) { TestRoutine<double>(128); }

TEST_F(TestCSRelu, TestF64_512) { TestRoutine<double>(512); }

TEST_F(TestCSRelu, TestF64_1024) { TestRoutine<double>(1024); }

TEST_F(TestCSRelu, TestF16_128) { TestRoutine<nv_half>(128); }

TEST_F(TestCSRelu, TestF16_512) { TestRoutine<nv_half>(512); }

TEST_F(TestCSRelu, TestF16_1024) { TestRoutine<nv_half>(1024); }
