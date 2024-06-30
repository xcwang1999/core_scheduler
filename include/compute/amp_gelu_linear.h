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

#pragma once
#include "compute/gelu_linear.h"
#include "module/amp_state.h"
#include "tensor.h"
#include "threading/scheduler.h"

namespace cs::compute {
struct AmpGeluLinear {
  struct State final : module::AmpState, GeluLinear::State {
    using Forward = GeluLinear::State::Forward;
    using Backward = GeluLinear::State::Backward;
    using Args = GeluLinear::State::Args;

    struct ForwardHighPrecision {
      Tensor weight{};
      Tensor bias{};
    } forwardHighPrecision;

    State(const Forward &forward, const ForwardHighPrecision &forwardHighPrecision,
          const Backward &backward, const Args &args);

    [[nodiscard]] OrderedDict<std::string, Tensor> parametersHighPrecision()
        const override;
  };

  using Options = GeluLinear::Options;

  static std::shared_ptr<State> init(const Scheduler &scheduler,
                                     const Options &options);
};
}  // namespace cs::compute