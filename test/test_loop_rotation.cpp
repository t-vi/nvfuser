// clang-format off
/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-present NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
// clang-format on
#include <gtest/gtest.h>

#include <inlining.h>
#include <ops/arith.h>
#include <scheduler/utils.h>
#include <test/test_gpu_validator.h>
#include <test/test_utils.h>

namespace nvfuser {

class LoopRotationTest : public NVFuserTest {
 private:
  // Please see note [Limitation of boundary assert]
  EnableOutOfBoundAssert guard;
};

TEST_F(LoopRotationTest, RotateInner_CUDA) {
  Fusion fusion;
  FusionGuard fg(&fusion);

  auto tv0 = makeConcreteTensor({-1, 3});
  fusion.addInput(tv0);
  auto tv1 = set(tv0);
  auto tv2 = set(tv1);
  auto tv3 = set(tv2);
  auto tv4 = set(tv3);
  fusion.addOutput(tv4);

  inlineMost();
  scheduler_utils::rotateLoop(tv4, -1, {tv1, tv2});

  const std::string expected_kernel = R"(
__global__ void CUDAGeneratedKernel(Tensor<float, 2> T0, Tensor<float, 2> T4) {
  NVFUSER_DEFINE_MAGIC_ZERO
  #pragma unroll 1
  for(nvfuser_index_t i21 = 0; i21 < T0.size[0]; ++i21) {
    int64_t i160;
    i160 = T0.stride[0] * i21;
    int64_t i522;
    i522 = 3 * i21;
    float T1[1];
    float T2[1];
    T1[0] = 0;
    T1[0]
       = T0[i160];
    T2[0]
       = T1[0];
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
      int64_t i657;
      i657 = (1 + i22) + nvfuser_zero;
      float T3[1];
      T3[0]
         = T2[0];
      T4[(i522 + (i22 + nvfuser_zero))]
         = T3[0];
      T1[0] = 0;
      if ((i657 < 3)) {
        T1[0]
           = T0[(i160 + (T0.stride[1] * i657))];
      }
      T2[0]
         = T1[0];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
  }
}
)";

  assertCUDAKernel(&fusion, expected_kernel);

  for (auto n : {0, 1, 99}) {
    auto options = at::TensorOptions().dtype(at::kFloat).device(at::kCUDA, 0);
    auto t0 = at::randn({n, 3}, options);
    FusionExecutor fe;
    fe.compileFusion(&fusion, {t0});
    auto cg_outputs = fe.runFusion({t0});
    testValidate(&fusion, cg_outputs, {t0}, {t0}, __LINE__, __FILE__);
  }
}

TEST_F(LoopRotationTest, RotateOuter_CUDA) {
  Fusion fusion;
  FusionGuard fg(&fusion);

  auto tv0 = makeConcreteTensor({-1, 3});
  fusion.addInput(tv0);
  auto tv1 = set(tv0);
  auto tv2 = set(tv1);
  auto tv3 = set(tv2);
  auto tv4 = set(tv3);
  fusion.addOutput(tv4);

  inlineAllAt(tv4, 1);
  scheduler_utils::rotateLoop(tv4, 0, {tv1, tv2});

  const std::string expected_kernel = R"(
__global__ void CUDAGeneratedKernel(Tensor<float, 2> T0, Tensor<float, 2> T4) {
  NVFUSER_DEFINE_MAGIC_ZERO
  bool b929;
  b929 = 0 < T0.size[0];
  float T1[3];
  float T2[3];
  #pragma unroll
  for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
    T1[i21] = 0;
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
    if (b929) {
      T1[i21]
         = T0[(T0.stride[1] * (i21 + nvfuser_zero))];
    }
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
    T2[i22]
       = T1[i22];
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll 1
  for(nvfuser_index_t i24 = 0; i24 < T0.size[0]; ++i24) {
    int64_t i546;
    i546 = 3 * i24;
    int64_t i687;
    i687 = T0.stride[0] + (T0.stride[0] * i24);
    bool b1349;
    b1349 = (1 + i24) < T0.size[0];
    // Alias Allocation - register
    auto& T3 = T1;
    #pragma unroll
    for(nvfuser_index_t i23 = 0; i23 < 3; ++i23) {
      T3[i23]
         = T2[i23];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i25 = 0; i25 < 3; ++i25) {
      T4[(i546 + (i25 + nvfuser_zero))]
         = T3[i25];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      T1[i21] = 0;
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      if (b1349) {
        T1[i21]
           = T0[(i687 + (T0.stride[1] * (i21 + nvfuser_zero)))];
      }
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
      T2[i22]
         = T1[i22];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
  }
}
)";
  assertCUDAKernel(&fusion, expected_kernel);

  for (auto n : {0, 1, 99}) {
    auto options = at::TensorOptions().dtype(at::kFloat).device(at::kCUDA, 0);
    auto t0 = at::randn({n, 3}, options);
    FusionExecutor fe;
    fe.compileFusion(&fusion, {t0});
    auto cg_outputs = fe.runFusion({t0});
    testValidate(&fusion, cg_outputs, {t0}, {t0}, __LINE__, __FILE__);
  }
}

TEST_F(LoopRotationTest, NonDivisibleSplit_CUDA) {
  Fusion fusion;
  FusionGuard fg(&fusion);

  auto tv0 = makeConcreteTensor({-1, -1});
  fusion.addInput(tv0);
  auto tv1 = set(tv0);
  auto tv2 = set(tv1);
  auto tv3 = set(tv2);
  auto tv4 = set(tv3);
  fusion.addOutput(tv4);

  for (auto tv : {tv0, tv1, tv2, tv3, tv4}) {
    tv->merge(0);
    tv->split(0, 5);
  }
  inlineAllAt(tv4, 1);
  scheduler_utils::rotateLoop(tv4, 0, {tv1, tv2});

  const std::string expected_kernel = R"(
__global__ void CUDAGeneratedKernel(Tensor<float, 2> T0, Tensor<float, 2> T4) {
  NVFUSER_DEFINE_MAGIC_ZERO
  int64_t i1195;
  i1195 = T0.size[0] * T0.size[1];
  float T1[5];
  float T2[5];
  #pragma unroll
  for(nvfuser_index_t i36 = 0; i36 < 5; ++i36) {
    T1[i36] = 0;
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i36 = 0; i36 < 5; ++i36) {
    int64_t i154;
    i154 = i36 + nvfuser_zero;
    if ((i154 < i1195)) {
      T1[i36]
         = T0[((T0.stride[0] * (i154 / T0.size[1])) + (T0.stride[1] * (i154 % T0.size[1])))];
    }
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i37 = 0; i37 < 5; ++i37) {
    T2[i37]
       = T1[i37];
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll 1
  for(nvfuser_index_t i39 = 0; i39 < (ceilDiv((T0.size[0] * T0.size[1]), 5)); ++i39) {
    int64_t i608;
    i608 = 5 * i39;
    int64_t i900;
    i900 = 5 + i608;
    // Alias Allocation - register
    auto& T3 = T1;
    #pragma unroll
    for(nvfuser_index_t i38 = 0; i38 < 5; ++i38) {
      T3[i38]
         = T2[i38];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i40 = 0; i40 < 5; ++i40) {
      int64_t i609;
      i609 = i608 + (i40 + nvfuser_zero);
      if ((i609 < i1195)) {
        T4[i609]
           = T3[i40];
      }
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i36 = 0; i36 < 5; ++i36) {
      T1[i36] = 0;
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i36 = 0; i36 < 5; ++i36) {
      int64_t i901;
      i901 = i900 + (i36 + nvfuser_zero);
      if ((i901 < i1195)) {
        T1[i36]
           = T0[((T0.stride[0] * (i901 / T0.size[1])) + (T0.stride[1] * (i901 % T0.size[1])))];
      }
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i37 = 0; i37 < 5; ++i37) {
      T2[i37]
         = T1[i37];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
  }
}
)";
  assertCUDAKernel(&fusion, expected_kernel);

  for (auto n : {0, 1, 99}) {
    auto options = at::TensorOptions().dtype(at::kFloat).device(at::kCUDA, 0);
    auto t0 = at::randn({n, 3}, options);
    FusionExecutor fe;
    fe.compileFusion(&fusion, {t0});
    auto cg_outputs = fe.runFusion({t0});
    testValidate(&fusion, cg_outputs, {t0}, {t0}, __LINE__, __FILE__);
  }
}

TEST_F(LoopRotationTest, DoubleBuffered_CUDA) {
  Fusion fusion;
  FusionGuard fg(&fusion);

  auto tv0 = makeConcreteTensor({-1, 3});
  fusion.addInput(tv0);
  auto tv1 = set(tv0);
  auto tv2 = set(tv1);
  auto tv3 = set(tv2);
  auto tv4 = set(tv3);
  fusion.addOutput(tv4);

  inlineAllAt(tv4, 1);
  tv1->circularBuffer(5);
  scheduler_utils::rotateLoop(tv4, 0, {tv2});

  const std::string expected_kernel = R"(
__global__ void CUDAGeneratedKernel(Tensor<float, 2> T0, Tensor<float, 2> T4) {
  NVFUSER_DEFINE_MAGIC_ZERO
  int64_t i537;
  i537 = T0.stride[0] * 4;
  float T1[15];
  #pragma unroll
  for(nvfuser_index_t i24 = 0; i24 < 4; ++i24) {
    int64_t i152;
    i152 = 3 * i24;
    int64_t i223;
    i223 = T0.stride[0] * i24;
    bool b1072;
    b1072 = (i24 + nvfuser_zero) < T0.size[0];
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      T1[(i152 + i21)] = 0;
    }
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      if (b1072) {
        T1[(i152 + i21)]
           = T0[(i223 + (T0.stride[1] * (i21 + nvfuser_zero)))];
      }
    }
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  float T2[3];
  #pragma unroll
  for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
    T2[i22]
       = T1[i22];
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll 1
  for(nvfuser_index_t i25 = 0; i25 < T0.size[0]; ++i25) {
    int64_t i453;
    i453 = 4 + i25;
    int64_t i455;
    i455 = 3 * (i453 % 5);
    int64_t i539;
    i539 = i537 + (T0.stride[0] * i25);
    int64_t i868;
    i868 = 3 * i25;
    int64_t i949;
    i949 = 3 * ((1 + i25) % 5);
    bool b1342;
    b1342 = i453 < T0.size[0];
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      T1[(i455 + i21)] = 0;
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      if (b1342) {
        T1[(i455 + i21)]
           = T0[(i539 + (T0.stride[1] * (i21 + nvfuser_zero)))];
      }
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    float T3[3];
    #pragma unroll
    for(nvfuser_index_t i23 = 0; i23 < 3; ++i23) {
      T3[i23]
         = T2[i23];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i27 = 0; i27 < 3; ++i27) {
      T4[(i868 + (i27 + nvfuser_zero))]
         = T3[i27];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
      T2[i22]
         = T1[(i949 + i22)];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
  }
}
)";
  assertCUDAKernel(&fusion, expected_kernel);

  for (auto n : {0, 1, 99}) {
    auto options = at::TensorOptions().dtype(at::kFloat).device(at::kCUDA, 0);
    auto t0 = at::randn({n, 3}, options);
    FusionExecutor fe;
    fe.compileFusion(&fusion, {t0});
    auto cg_outputs = fe.runFusion({t0});
    testValidate(&fusion, cg_outputs, {t0}, {t0}, __LINE__, __FILE__);
  }
}

TEST_F(LoopRotationTest, SelectDoubleBufferLoad_CUDA) {
  Fusion fusion;
  FusionGuard fg(&fusion);

  auto tv0 = makeConcreteTensor({-1, 3});
  fusion.addInput(tv0);
  auto tv1 = set(tv0);
  auto tv2 = set(tv1);
  auto tv3 = set(tv2);
  auto tv4 = set(tv3);
  fusion.addOutput(tv4);

  inlineAllAt(tv4, 1);
  tv1->circularBuffer(5);
  scheduler_utils::rotateLoop(tv4, 0, {tv1, tv2});

  const std::string expected_kernel = R"(
__global__ void CUDAGeneratedKernel(Tensor<float, 2> T0, Tensor<float, 2> T4) {
  NVFUSER_DEFINE_MAGIC_ZERO
  int64_t i545;
  i545 = 4 * T0.stride[0];
  int64_t i1116;
  i1116 = T0.stride[0] * 5;
  bool b1425;
  b1425 = 0 < T0.size[0];
  bool b1744;
  b1744 = 4 < T0.size[0];
  float T1[15];
  #pragma unroll
  for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
    T1[i21] = 0;
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
    if (b1425) {
      T1[i21]
         = T0[(T0.stride[1] * (i21 + nvfuser_zero))];
    }
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i24 = 0; i24 < 4; ++i24) {
    int64_t i285;
    i285 = 3 + (3 * i24);
    int64_t i368;
    i368 = T0.stride[0] + (T0.stride[0] * i24);
    bool b1679;
    b1679 = ((1 + i24) + nvfuser_zero) < T0.size[0];
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      T1[(i285 + i21)] = 0;
    }
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      if (b1679) {
        T1[(i285 + i21)]
           = T0[(i368 + (T0.stride[1] * (i21 + nvfuser_zero)))];
      }
    }
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  float T2[3];
  #pragma unroll
  for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
    T1[(12 + i21)] = 0;
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
    if (b1744) {
      T1[(12 + i21)]
         = T0[(i545 + (T0.stride[1] * (i21 + nvfuser_zero)))];
    }
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll
  for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
    T2[i22]
       = T1[i22];
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  #pragma unroll 1
  for(nvfuser_index_t i25 = 0; i25 < T0.size[0]; ++i25) {
    int64_t i925;
    i925 = 3 * i25;
    int64_t i1026;
    i1026 = 3 * (i25 % 5);
    int64_t i1118;
    i1118 = i1116 + (T0.stride[0] * i25);
    int64_t i1302;
    i1302 = 3 * ((1 + i25) % 5);
    bool b2180;
    b2180 = (5 + i25) < T0.size[0];
    float T3[3];
    #pragma unroll
    for(nvfuser_index_t i23 = 0; i23 < 3; ++i23) {
      T3[i23]
         = T2[i23];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i27 = 0; i27 < 3; ++i27) {
      T4[(i925 + (i27 + nvfuser_zero))]
         = T3[i27];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      T1[(i1026 + i21)] = 0;
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i21 = 0; i21 < 3; ++i21) {
      if (b2180) {
        T1[(i1026 + i21)]
           = T0[(i1118 + (T0.stride[1] * (i21 + nvfuser_zero)))];
      }
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    #pragma unroll
    for(nvfuser_index_t i22 = 0; i22 < 3; ++i22) {
      T2[i22]
         = T1[(i1302 + i22)];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
  }
}
)";
  assertCUDAKernel(&fusion, expected_kernel);

  for (auto n : {0, 1, 99}) {
    auto options = at::TensorOptions().dtype(at::kFloat).device(at::kCUDA, 0);
    auto t0 = at::randn({n, 3}, options);
    FusionExecutor fe;
    fe.compileFusion(&fusion, {t0});
    auto cg_outputs = fe.runFusion({t0});
    testValidate(&fusion, cg_outputs, {t0}, {t0}, __LINE__, __FILE__);
  }
}

// This is a case similar to matmul, where we have
// tv4 = set(tv0) // cp.async for matmul
// tv1 = set(tv4) // ld.matrix for matmul
// and both are double buffered
TEST_F(LoopRotationTest, MultipleDoubleBuffer_CUDA) {
  if (!deviceMajorMinorCheck(8)) {
    GTEST_SKIP() << "skipping tests on pre-Ampere GPUs";
    return;
  }
  Fusion fusion;
  FusionGuard fg(&fusion);

  auto tv0 = makeConcreteTensor({-1, 3});
  fusion.addInput(tv0);
  auto tv1 = set(tv0);
  auto tv2 = set(tv1);
  auto tv3 = set(tv2);
  fusion.addOutput(tv3);

  auto tv4 = tv0->cacheAfter(LoadStoreOpType::CpAsyncCa);
  tv4->setMemoryType(MemoryType::Shared);

  inlineAllAt(tv3, 1);
  inlineSelectedAt({tv1, tv2, tv3}, tv3, 2);

  tv4->circularBuffer(5);
  tv1->doubleBuffer();
  scheduler_utils::rotateLoop(tv3, 0, {tv1});

  // TODO: i827 < 3 is trivial, simplify it away
  const std::string expected_kernel = R"(
__global__ void CUDAGeneratedKernel(Tensor<float, 2> T0, Tensor<float, 2> T3) {
  alignas(16) extern __shared__ char array[];
  unsigned smem_offset = 0;
  NVFUSER_DEFINE_MAGIC_ZERO
  float* ptr98;
  ptr98 = T0.data;
  float* ptr384;
  ptr384 = ptr98 + (T0.stride[0] * 4);
  smem_offset = alignBufferSize(smem_offset, 16);
  float* T4 = reinterpret_cast<float*>(array + smem_offset);
  smem_offset += (15 * sizeof(float));
  #pragma unroll
  for(nvfuser_index_t i18 = 0; i18 < 4; ++i18) {
    float* ptr165;
    ptr165 = ptr98 + (T0.stride[0] * i18);
    unsigned i249;
    i249 = (toSmem(T4)) + (12 * i18);
    bool b1167;
    b1167 = (i18 + nvfuser_zero) < T0.size[0];
    #pragma unroll
    for(nvfuser_index_t i17 = 0; i17 < 3; ++i17) {
      Ampere::cpAsyncCa<float, 1>((i249 + (4 * i17)),(ptr165 + (T0.stride[1] * (i17 + nvfuser_zero))),b1167);
    }
    Ampere::cpAsyncCommit();
  }
  NVFUSER_UPDATE_MAGIC_ZERO
  Ampere::cpAsyncPartialBarrier<3>();
  float T1[2];
  T1[0]
     = T4[0];
  #pragma unroll 1
  for(nvfuser_index_t i19 = 0; i19 < T0.size[0]; ++i19) {
    float* ptr385;
    ptr385 = ptr384 + (T0.stride[0] * i19);
    int64_t i486;
    i486 = 4 + i19;
    unsigned i489;
    i489 = (toSmem(T4)) + (12 * (i486 % 5));
    int64_t i567;
    i567 = 1 + (3 * (i19 % 5));
    int64_t i895;
    i895 = 3 * i19;
    bool b1359;
    b1359 = i486 < T0.size[0];
    Ampere::cpAsyncPartialBarrier<3>();
    #pragma unroll
    for(nvfuser_index_t i17 = 0; i17 < 3; ++i17) {
      Ampere::cpAsyncCa<float, 1>((i489 + (4 * i17)),(ptr385 + (T0.stride[1] * (i17 + nvfuser_zero))),b1359);
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    Ampere::cpAsyncCommit();
    #pragma unroll
    for(nvfuser_index_t i22 = 0; i22 < 2; ++i22) {
      T1[((1 + i22) % 2)]
         = T4[(i567 + i22)];
      float T2[1];
      T2[0]
         = T1[(i22 % 2)];
      T3[(i895 + (i22 + nvfuser_zero))]
         = T2[0];
    }
    NVFUSER_UPDATE_MAGIC_ZERO
    float T2[1];
    T2[0]
       = T1[0];
    T3[(2 + i895)]
       = T2[0];
    NVFUSER_UPDATE_MAGIC_ZERO
    T1[0]
       = T4[(3 * ((1 + i19) % 5))];
  }
}
)";
  assertCUDAKernel(&fusion, expected_kernel);

  for (auto n : {0, 1, 99}) {
    auto options = at::TensorOptions().dtype(at::kFloat).device(at::kCUDA, 0);
    auto t0 = at::randn({n, 3}, options);
    FusionExecutor fe;
    fe.compileFusion(&fusion, {t0});
    auto cg_outputs = fe.runFusion({t0});
    testValidate(&fusion, cg_outputs, {t0}, {t0}, __LINE__, __FILE__);
  }
}
} // namespace nvfuser