#include <dtensor_mpi.h>
#include <gtest/gtest.h>
#include <mpi.h>
#include <thread_pool.h>

#include <Eigen/Dense>

#include "communication/all_reduce.h"
#include "logger.h"

class AllReduceMPITestFixture : public ::testing::Test {
 protected:
  dllm::Context context{};
  int localRank, rank, worldSize;
  MPI_Comm comm;

  void SetUp() override {
    CHECK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    CHECK_MPI(MPI_Comm_size(MPI_COMM_WORLD, &worldSize));
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
                        &comm);
    MPI_Comm_rank(comm, &localRank);
  }

  void TearDown() override { MPI_Comm_free(&comm); }
};

namespace {
template <typename T>
void TestForwardT(const dllm::Context &context, int rank, MPI_Comm comm) {
  const int m = 128;
  auto shapeX = cute::make_shape(m);
  auto layoutX = cute::make_layout(shapeX, cute::GenRowMajor{});

  Eigen::Vector<T, Eigen::Dynamic> x(m);

  auto tensorX = std::make_shared<dllm::DTensor1D<dllm::communication::MPI>>(
      rank, comm,
      dllm::Tensor1D{x.data(), layoutX, dllm::toDtype<T>(), dllm::CUDA});

  std::srand(rank + 1);
  x.setRandom();

  auto task =
      dllm::communication::AllReduce<dllm::communication::MPI>::runInplace(
          tensorX, dllm::communication::SUM);
  tensorX.reset();
  task(&context);

  Eigen::Vector<T, Eigen::Dynamic> accumulator(m);
  if (rank == 0) {
    int worldSize;
    CHECK_MPI(MPI_Comm_size(comm, &worldSize));
    accumulator.setZero();
    for (int i = 0; i < worldSize; ++i) {
      std::srand(i + 1);
      accumulator += Eigen::Vector<T, Eigen::Dynamic>(m).setRandom();
    }
    ASSERT_NEAR(
        accumulator.array().abs().maxCoeff() - x.array().abs().maxCoeff(), 0,
        1e-4);
  }
}
}  // namespace

TEST_F(AllReduceMPITestFixture, TestForwardF32) {
  TestForwardT<float>(context, localRank, comm);
}
TEST_F(AllReduceMPITestFixture, TestForwardF64) {
  TestForwardT<double>(context, localRank, comm);
}