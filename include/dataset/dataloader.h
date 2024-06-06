#pragma once
#include <memory>
#include <queue>
#include <thread>

#include "dataset/dataset.h"
#include "tensor.h"
#include "threading/task_cudart.h"

namespace dllm::dataset {
// This class is only a proxy class
struct LlmDataLoader {
  // fill will set the future, so you do not have to sync with x and y's future
  // handles explicitly
  void fill(const std::shared_ptr<Tensor> &x,
            const std::shared_ptr<Tensor> &y) const;

  LlmDataLoader(const std::shared_ptr<const LlmDataset> &dataset, int localRank,
                int batch_size, int num_workers, bool shuffle,
                const std::vector<int> &bindingMap = {});

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace dllm::dataset
