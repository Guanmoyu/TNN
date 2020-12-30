// Tencent is pleased to support the open source community by making TNN available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "tnn/device/arm/acc/compute_arm82/compute_half.h"

#include <string.h>

#include "tnn/core/macro.h"
#include "tnn/device/arm/acc/compute/compute.h"
#include "tnn/device/arm/acc/Half8.h"
#include "tnn/device/arm/arm_common.h"
#include "tnn/device/arm/arm_util.h"
#include "tnn/utils/half_utils.h"
#include "tnn/utils/naive_compute.h"
#include "tnn/utils/omp_utils.h"

namespace TNN_NS {

#if TNN_ARM82
template <>
void PostAddBias<fp16_t, fp16_t>(void* dst, const void* bias, long area, long oc8) {
    for (long z = oc8 - 1; z >= 0; --z) {
        Half8 vbias = Half8::load(reinterpret_cast<const fp16_t*>(bias) + 8 * z);
        auto dst_z  = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
        long p      = 0;
        for (; p < area - 3; p += 4) {
            auto dst_p = dst_z + 8 * p;
            Half8 dst_0 = Half8::load(dst_p);
            Half8 dst_1 = Half8::load(dst_p + 8);
            Half8 dst_2 = Half8::load(dst_p + 16);
            Half8 dst_3 = Half8::load(dst_p + 24);
            dst_0 = dst_0 + vbias;
            dst_1 = dst_1 + vbias;
            dst_2 = dst_2 + vbias;
            dst_3 = dst_3 + vbias;
            Half8::save(dst_p, dst_0);
            Half8::save(dst_p + 8, dst_1);
            Half8::save(dst_p + 16, dst_2);
            Half8::save(dst_p + 24, dst_3);
        }
        for (; p < area; ++p) {
            auto dst_p = dst_z + 8 * p;
            Half8 dst_0 = Half8::load(dst_p);
            dst_0 = dst_0 + vbias;
            Half8::save(dst_p, dst_0);
        }
    }
}

template <>
void PostAddBiasRelu<fp16_t, fp16_t>(void* dst, const void* bias, long area, long oc8) {
    Half8 vzero = Half8((fp16_t)0.f);
    for (long z = oc8 - 1; z >= 0; --z) {
        Half8 vbias = Half8::load(reinterpret_cast<const fp16_t*>(bias) + 8 * z);
        auto dst_z  = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
        long p      = 0;
        for (; p < area - 3; p += 4) {
            auto dst_p = dst_z + 8 * p;
            Half8 dst_0 = Half8::load(dst_p);
            Half8 dst_1 = Half8::load(dst_p + 8);
            Half8 dst_2 = Half8::load(dst_p + 16);
            Half8 dst_3 = Half8::load(dst_p + 24);
            dst_0 = Half8::max(dst_0 + vbias, vzero);
            dst_1 = Half8::max(dst_1 + vbias, vzero);
            dst_2 = Half8::max(dst_2 + vbias, vzero);
            dst_3 = Half8::max(dst_3 + vbias, vzero);
            Half8::save(dst_p, dst_0);
            Half8::save(dst_p + 8, dst_1);
            Half8::save(dst_p + 16, dst_2);
            Half8::save(dst_p + 24, dst_3);
        }
        for (; p < area; ++p) {
            auto dst_p = dst_z + 8 * p;
            Half8 dst_0 = Half8::load(dst_p);
            dst_0 = Half8::max(dst_0 + vbias, vzero);
            Half8::save(dst_p, dst_0);
        }
    }
}

template <>
void PostAddBiasRelu6<fp16_t, fp16_t>(void* dst, const void* bias, long area, long oc8) {
    Half8 vzero = Half8((fp16_t)0.f);
    Half8 vrelu6 = Half8((fp16_t)6.f);
    for (long z = oc8 - 1; z >= 0; --z) {
        Half8 vbias = Half8::load(reinterpret_cast<const fp16_t*>(bias) + 8 * z);
        auto dst_z   = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
        long p       = 0;
        for (; p < area - 3; p += 4) {
            auto dst_p = dst_z + 8 * p;
            Half8 dst_0 = Half8::load(dst_p);
            Half8 dst_1 = Half8::load(dst_p + 8);
            Half8 dst_2 = Half8::load(dst_p + 16);
            Half8 dst_3 = Half8::load(dst_p + 24);
            dst_0 = Half8::min(Half8::max(dst_0 + vbias, vzero), vrelu6);
            dst_1 = Half8::min(Half8::max(dst_1 + vbias, vzero), vrelu6);
            dst_2 = Half8::min(Half8::max(dst_2 + vbias, vzero), vrelu6);
            dst_3 = Half8::min(Half8::max(dst_3 + vbias, vzero), vrelu6);
            Half8::save(dst_p, dst_0);
            Half8::save(dst_p + 8, dst_1);
            Half8::save(dst_p + 16, dst_2);
            Half8::save(dst_p + 24, dst_3);
        }
        for (; p < area; ++p) {
            auto dst_p = dst_z + 8 * p;
            Half8 dst_0 = Half8::load(dst_p);
            dst_0 = Half8::min(Half8::max(dst_0 + vbias, vzero), vrelu6);
            Half8::save(dst_p, dst_0);
        }
    }
}

template <>
void PostAddBiasSwish<fp16_t, fp16_t, true>(void* dst, const void* bias, long area, long oc8) {
    if (!bias) {
        for (long z = oc8 - 1; z >= 0; --z) {
            fp16_t* dst_z  = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
            for (long p = 0; p < area; ++p) {
                auto dst_p = dst_z + 8 * p;
                Half8 val = Half8::load(dst_p);
                Half8::save(dst_p, val * Half8::fast_sigmoid(val));
            }
        }
    } else {
        for (long z = oc8 - 1; z >= 0; --z) {
            Half8 vbias = Half8::load(reinterpret_cast<const fp16_t*>(bias) + 8 * z);
            fp16_t* dst_z = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
            for (long p = 0; p < area; ++p) {
                auto dst_p = dst_z + 8 * p;
                Half8 val = Half8::load(dst_p);
                val = val + vbias;
                Half8::save(dst_p, val * Half8::fast_sigmoid(val));
            }
        }
    }
}
template <>
void PostAddBiasSwish<fp16_t, fp16_t, false>(void* dst, const void* bias, long area, long oc8) {
    if (!bias) {
        for (long z = oc8 - 1; z >= 0; --z) {
            fp16_t* dst_z  = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
            for (long p = 0; p < area; ++p) {
                auto dst_p = dst_z + 8 * p;
                Half8 val = Half8::load(dst_p);
                Half8::save(dst_p, val * Half8::sigmoid(val));
            }
        }
    } else {
        for (long z = oc8 - 1; z >= 0; --z) {
            Half8 vbias = Half8::load(reinterpret_cast<const fp16_t*>(bias) + 8 * z);
            fp16_t* dst_z     = reinterpret_cast<fp16_t*>(dst) + area * 8 * z;
            for (long p = 0; p < area; ++p) {
                auto dst_p = dst_z + 8 * p;
                Half8 val = Half8::load(dst_p);
                val = val + vbias;
                Half8::save(dst_p, val * Half8::sigmoid(val));
            }
        }
    }
}

void MaxPoolingHalf(const fp16_t* src, long iw, long ih, fp16_t* dst, long ow, long oh, long kw, long kh,
                    long stride_w, long stride_h, long pad_w, long pad_h) {
    for (long oy = 0; oy < oh; ++oy) {
        for (long ox = 0; ox < ow; ++ox) {
            const long srcOriginX = ox * stride_w - pad_w;
            const long srcOriginY = oy * stride_h - pad_h;
            const long kxs        = MAX(0, -srcOriginX);
            const long kxe        = MIN(kw, iw - srcOriginX);
            const long kys        = MAX(0, -srcOriginY);
            const long kye        = MIN(kh, ih - srcOriginY);
            const auto src_ptr    = src + (srcOriginY * iw + srcOriginX) * 8;
            auto dst_ptr          = dst + (oy * ow + ox) * 8;

            Half8 vmax = Half8(HALF_LOWEST);

            for (long ky = kys; ky < kye; ++ky) {
                const auto src_ptr_h = src_ptr + (ky * iw) * 8;
                for (long kx = kxs; kx < kxe; kx++) {
                    vmax = Half8::max(vmax, Half8::load(src_ptr_h + kx * 8));
                }
            }

            Half8::save(dst_ptr, vmax);
        }
    }
}

void AvgPoolingHalf(const fp16_t* src, long iw, long ih, fp16_t* dst, long ow, long oh, long kw, long kh,
                    long stride_w, long stride_h, long pad_w, long pad_h) {
    for (long oy = 0; oy < oh; ++oy) {
        for (long ox = 0; ox < ow; ++ox) {
            const long srcOriginX    = ox * stride_w - pad_w;
            const long srcOriginY    = oy * stride_h - pad_h;
            const long kxs           = MAX(0, -srcOriginX);
            const long kxe           = MIN(kw, iw - srcOriginX);
            const long kys           = MAX(0, -srcOriginY);
            const long kye           = MIN(kh, ih - srcOriginY);
            const float kernel_count = 1.0 / ((kxe - kxs) * (kye - kys));
            const auto src_ptr       = src + (srcOriginY * iw + srcOriginX) * 8;
            auto dst_ptr             = dst + (oy * ow + ox) * 8;

            Half8 vavg = Half8(fp16_t(0.f));

            for (long ky = kys; ky < kye; ++ky) {
                const auto src_ptr_h = src_ptr + (ky * iw) * 8;
                for (long kx = kxs; kx < kxe; kx++) {
                    vavg = vavg + Half8::load(src_ptr_h + kx * 8);
                }
            }

            Half8::save(dst_ptr, vavg * Half8(fp16_t(kernel_count)));
        }
    }
}

template <>
void DepthwiseUnit<fp16_t, fp16_t>(fp16_t* dst, const fp16_t* src, const fp16_t* weight, long fw, long fh, long weight_y_step, long dilate_x_step,
                   long dilate_y_step) {
    long fx, fy;
    Half8 dst_v = Half8((fp16_t)0.0f);
    const auto* src_z    = src;
    const auto* weight_z = weight;
    for (fy = 0; fy < fh; ++fy) {
        const auto* src_y    = src_z + fy * dilate_y_step;
        const auto* weight_y = weight_z + fy * weight_y_step;
        for (fx = 0; fx < fw; ++fx) {
            Half8 src_v = Half8::load(src_y + fx * dilate_x_step);
            Half8 weight_v = Half8::load(weight_y + 8 * fx);
            Half8::mla(dst_v, src_v, weight_v);
        }
    }
    Half8::save(dst, dst_v);
}

template <>
void DepthwiseConv<fp16_t, fp16_t>(fp16_t* dst, const fp16_t* src, const fp16_t* weight, long width, long src_w_step, long fw, long fh,
                   long dilate_x_step, long dilate_y_step, long height, long srcHStep, long dstHStep) {
    long dx, fx, fy;
    for (long y = 0; y < height; ++y) {
        auto srcY = src + y * srcHStep;
        auto dstY = dst + y * dstHStep;
        dx        = 0;
        for (; dx + 3 < width; dx += 4) {
            Half8 dst_v[4];
            for (long i = 0; i < 4; i++) {
                dst_v[i] = Half8((fp16_t)0.0f);
            }
            const auto* src_z    = srcY + src_w_step * dx;
            const auto* weight_z = weight;
            for (fy = 0; fy < fh; ++fy) {
                const auto* src_y    = src_z + fy * dilate_y_step;
                const auto* weight_y = weight_z + fy * fw * 8;
                for (fx = 0; fx < fw; ++fx) {
                    Half8 weight_v = Half8::load(weight_y + 8 * fx);
                    Half8 src_v0   = Half8::load(src_y + fx * dilate_x_step);
                    Half8 src_v1   = Half8::load(src_y + fx * dilate_x_step + src_w_step);
                    Half8 src_v2   = Half8::load(src_y + fx * dilate_x_step + 2 * src_w_step);
                    Half8 src_v3   = Half8::load(src_y + fx * dilate_x_step + 3 * src_w_step);
                    Half8::mla(dst_v[0], src_v0, weight_v);
                    Half8::mla(dst_v[1], src_v1, weight_v);
                    Half8::mla(dst_v[2], src_v2, weight_v);
                    Half8::mla(dst_v[3], src_v3, weight_v);
                }
            }
            Half8::save(dstY + (dx + 0) * 8, dst_v[0]);
            Half8::save(dstY + (dx + 1) * 8, dst_v[1]);
            Half8::save(dstY + (dx + 2) * 8, dst_v[2]);
            Half8::save(dstY + (dx + 3) * 8, dst_v[3]);
        }
        for (; dx < width; ++dx) {
            Half8 dst_v = Half8((fp16_t)0.0f);
            const auto* src_z    = srcY + src_w_step * dx;
            const auto* weight_z = weight;
            for (fy = 0; fy < fh; ++fy) {
                const auto* src_y    = src_z + fy * dilate_y_step;
                const auto* weight_y = weight_z + fy * fw * 8;
                for (fx = 0; fx < fw; ++fx) {
                    Half8 src_v    = Half8::load(src_y + fx * dilate_x_step);
                    Half8 weight_v = Half8::load(weight_y + 8 * fx);
                    Half8::mla(dst_v, src_v, weight_v);
                }
            }
            Half8::save(dstY + dx * 8, dst_v);
        }
    }
}

template <>
void DepthwiseUnitDeconv<fp16_t, fp16_t>(const fp16_t* dst, fp16_t* src, const fp16_t* weight, long fw, long fh, long weight_y_step,
                         long dilate_x_step, long dilate_y_step) {
    long fx, fy;
    fp16_t* src_z              = src;
    const fp16_t* weight_z = weight;
    Half8 dstV = Half8::load(dst);
    for (fy = 0; fy < fh; ++fy) {
        fp16_t* src_y = src_z + fy * dilate_y_step;
        const fp16_t* weight_y = weight_z + fy * weight_y_step;
        for (fx = 0; fx < fw; ++fx) {
            Half8 weight_x = Half8::load(weight_y + 8 * fx);
            Half8 src_x    = Half8::load(src_y + fx * dilate_x_step);
            Half8::mla(src_x, weight_x, dstV);
            Half8::save(src_y + fx * dilate_x_step, src_x);
        }
    }
}

template <>
void DepthwiseDeconv<fp16_t, fp16_t>(const fp16_t* dst, fp16_t* src, const fp16_t* weight, long width, long src_w_setup, long fw, long fh,
                     long dilate_x_step, long dilate_y_step) {
    long dx;
    for (dx = 0; dx < width; ++dx) {
        const fp16_t* dst_x = dst + dx * 8;
        fp16_t* src_dx      = src + src_w_setup * dx;
        DepthwiseUnitDeconv(dst_x, src_dx, weight, fw, fh, fw * 8, dilate_x_step, dilate_y_step);
    }
}

#define transpose_4x4(v0, v1, v2, v3, v_zero)       \
{                                                   \
    float32x4x2_t q01 = vtrnq_f32(v0, v1);          \
    float32x4x2_t q23 = vtrnq_f32(v2, v_zero);      \
    float32x2_t d00 = vget_low_f32(q01.val[0]);     \
    float32x2_t d01 = vget_high_f32(q01.val[0]);    \
    float32x2_t d10 = vget_low_f32(q01.val[1]);     \
    float32x2_t d11 = vget_high_f32(q01.val[1]);    \
    float32x2_t d20 = vget_low_f32(q23.val[0]);     \
    float32x2_t d21 = vget_high_f32(q23.val[0]);    \
    float32x2_t d30 = vget_low_f32(q23.val[1]);     \
    float32x2_t d31 = vget_high_f32(q23.val[1]);    \
    v0 = vcombine_f32(d00, d20);                    \
    v1 = vcombine_f32(d10, d30);                    \
    v2 = vcombine_f32(d01, d21);                    \
    v3 = vcombine_f32(d11, d31);                    \
}
int PackNeonC3(fp16_t *dst, const float *src, size_t hw, size_t channel) {
    auto src0 = src;
    auto src1 = src + hw;
    auto src2 = src + hw * 2;
    int cur_hw = 0;
    float32x4_t v_zero_f32 = vdupq_n_f32(0.f);
#ifdef __aarch64__
    float16x4_t v_zero_f16 = vdup_n_f16(0.f);
#else
    uint16x4_t v_zero_u16  = vdup_n_u16(0x0);
    uint16_t *dst_u16 = reinterpret_cast<uint16_t *>(dst);
#endif
    for (; cur_hw + 3 < hw; cur_hw += 4) {
        float32x4_t v0 = vld1q_f32(src0 + cur_hw);
        float32x4_t v1 = vld1q_f32(src1 + cur_hw);
        float32x4_t v2 = vld1q_f32(src2 + cur_hw);
        float32x4_t v3;
        transpose_4x4(v0, v1, v2, v3, v_zero_f32);
#ifdef __aarch64__
        vst1q_f16(dst + cur_hw * 8,      vcombine_f16(vcvt_f16_f32(v0), v_zero_f16));
        vst1q_f16(dst + cur_hw * 8 + 8,  vcombine_f16(vcvt_f16_f32(v1), v_zero_f16));
        vst1q_f16(dst + cur_hw * 8 + 16, vcombine_f16(vcvt_f16_f32(v2), v_zero_f16));
        vst1q_f16(dst + cur_hw * 8 + 24, vcombine_f16(vcvt_f16_f32(v3), v_zero_f16));
#else
        vst1q_u16(dst_u16 + cur_hw * 8,      vcombine_u16(vreinterpret_u16_f16(vcvt_f16_f32(v0)), v_zero_u16));
        vst1q_u16(dst_u16 + cur_hw * 8 + 8,  vcombine_u16(vreinterpret_u16_f16(vcvt_f16_f32(v1)), v_zero_u16));
        vst1q_u16(dst_u16 + cur_hw * 8 + 16, vcombine_u16(vreinterpret_u16_f16(vcvt_f16_f32(v2)), v_zero_u16));
        vst1q_u16(dst_u16 + cur_hw * 8 + 24, vcombine_u16(vreinterpret_u16_f16(vcvt_f16_f32(v3)), v_zero_u16));        
#endif
    }
    for (; cur_hw < hw; cur_hw++) {
        dst[cur_hw * 8 + 0] = src0[cur_hw];
        dst[cur_hw * 8 + 1] = src1[cur_hw];
        dst[cur_hw * 8 + 2] = src2[cur_hw];
        dst[cur_hw * 8 + 3] = 0.f;
        dst[cur_hw * 8 + 4] = 0.f;
        dst[cur_hw * 8 + 5] = 0.f;
        dst[cur_hw * 8 + 6] = 0.f;
        dst[cur_hw * 8 + 7] = 0.f;
    }
    return 0;
}
#endif

#ifndef TNN_USE_NEON
/*
general deconv micro kernel fp16_t
*/
void DeconvFp16O8(fp16_t* dst, const fp16_t* src, const fp16_t* weight, long width, long dst_w_step, long src_depth_quad,
                   long src_depth_step, long fw, long fh, long dilate_x_step, long dilate_y_step) {
    long dx, sz, fx, fy;
    for (dx = 0; dx < width; ++dx) {
        auto dst_dx = dst + dx * dst_w_step;
        for (fy = 0; fy < fh; ++fy) {
            auto dst_y    = dst_dx + fy * dilate_y_step;
            auto weight_y = weight + fy * fw * src_depth_quad * 64;
            for (fx = 0; fx < fw; ++fx) {
                auto dst_x    = dst_y + fx * dilate_x_step;
                auto weight_x = weight_y + fx * src_depth_quad * 64;
                fp16_t temp[8] = {fp16_t(0.0f)};
                for (sz = 0; sz < src_depth_quad; ++sz) {
                    auto weight_z = weight_x + sz * 64;
                    auto src_z    = src + dx * 8 + sz * src_depth_step;
                    for (long i = 0; i < 8; ++i) {
                        for (long j = 0; j < 8; ++j) {
                            temp[j] = temp[j] + src_z[i] * weight_z[8 * i + j];
                        }
                    }
                }
                for (long j = 0; j < 8; ++j) {
                    dst_x[j] = dst_x[j] + temp[j];
                }
            }
        }
    }
}
#endif

void Half2Float(float* dst, const fp16_t* src, const size_t length) {
#if TNN_ARM82 && !defined(TNN_ARM82_SIMU)
    Half2FloatKernel(dst, src, length);
#else
    for (auto i = 0; i < length; i++) {
        dst[i] = src[i];
    }
#endif
}
void Float2Half(fp16_t* dst, const float* src, const size_t length) {
#if TNN_ARM82 && !defined(TNN_ARM82_SIMU)
    Float2HalfKernel(dst, src, length);
#else
    for (auto i = 0; i < length; i++) {
        dst[i] = src[i];
    }
#endif
}

void FloatC4ToHalfC8(fp16_t* dst, const float* src, long batch, long channel, long hw) {
    long c_r4 = UP_DIV(channel, 4);
    long c_r8 = UP_DIV(channel, 8);
    for (long n = 0; n < batch; n++) {
        auto dst_n = dst + n * c_r8 * hw * 8;
        auto src_n = src + n * c_r4 * hw * 4;
        OMP_PARALLEL_FOR_GUIDED_
        for (long ci = 0; ci < c_r4; ++ci) {
            long co         = ci / 2;
            long dst_offset = (ci % 2) ? 4 : 0;
            auto dst_c      = dst_n + co * hw * 8 + dst_offset;
            auto src_c      = src_n + ci * hw * 4;
            for (long cnt = 0; cnt < hw; cnt++) {
                // nchw4 to nchw8
#if defined(TNN_ARM82) && !defined(TNN_ARM82_SIMU) && defined(__aarch64__)
                vst1_f16(dst_c + cnt * 8, vcvt_f16_f32(vld1q_f32(src_c + cnt * 4)));
#elif defined(TNN_ARM82) && !defined(TNN_ARM82_SIMU) && !defined(__aarch64__) && defined(__arm__)
                vst1_u16((unsigned short*)(dst_c + cnt * 8), vreinterpret_u16_f16(vcvt_f16_f32(vld1q_f32(src_c + cnt * 4))));
#else
                for (long idx = 0; idx < 4; idx++) {
                    dst_c[cnt * 8 + idx] = src_c[cnt * 4 + idx];
                }
#endif
            }
        }
    }
}

void HalfC8ToFloatC4(float* dst, const fp16_t* src, long batch, long channel, long hw) {
    long c_r4 = UP_DIV(channel, 4);
    long c_r8 = UP_DIV(channel, 8);
    for (long n = 0; n < batch; n++) {
        auto src_n = src + n * c_r8 * hw * 8;
        auto dst_n = dst + n * c_r4 * hw * 4;
        OMP_PARALLEL_FOR_GUIDED_
        for (long co = 0; co < c_r4; ++co) {
            long ci         = co / 2;
            long src_offset = (co % 2) ? 4 : 0;
            auto src_c      = src_n + ci * hw * 8 + src_offset;
            auto dst_c      = dst_n + co * hw * 4;
            for (long cnt = 0; cnt < hw; cnt++) {
                // nchw8 to nchw4
#if defined(TNN_ARM82) && !defined(TNN_ARM82_SIMU) && defined(__aarch64__)
                vst1q_f32(dst_c + cnt * 4, vcvt_f32_f16(vld1_f16(src_c + cnt * 8)));
#elif defined(TNN_ARM82) && !defined(TNN_ARM82_SIMU) && !defined(__aarch64__) && defined(__arm__)
                vst1q_f32(dst_c + cnt * 4, vcvt_f32_f16(vreinterpret_f16_u16(vld1_u16((unsigned short*)src_c + cnt * 8))));
#else
                for (long idx = 0; idx < 4; idx++) {
                    dst_c[cnt * 4 + idx] = src_c[cnt * 8 + idx];
                }
#endif
            }
        }
    }
}

/*
convert data type from uint8 to half, data format from nhwc 2 nc8hw8
*/
template <bool reverse_channel>
void BGRAToBlobImpl(const uint8_t *src, fp16_t *dst, const float *scale, const float *bias,
                    int hw, int channel) {
    int i = 0;
    fp16_t scale_half[4] = {fp16_t(scale[0]), fp16_t(scale[1]), fp16_t(scale[2]), fp16_t(scale[3])};
    fp16_t bias_half[4]  = {fp16_t(bias[0]), fp16_t(bias[1]), fp16_t(bias[2]), fp16_t(bias[3])};
#if (defined TNN_USE_NEON) && (TNN_ARM82) && (!defined TNN_ARM82_SIMU) && (__aarch64__)
    float16x8_t bias_neon_b = vdupq_n_f16(bias_half[0]);
    float16x8_t bias_neon_g = vdupq_n_f16(bias_half[1]);
    float16x8_t bias_neon_r = vdupq_n_f16(bias_half[2]);
    float16x8_t bias_neon_a = vdupq_n_f16(bias_half[3]);
    float16x8_t vzero       = vdupq_n_f16(0.0f);
    float16x8x4_t vf16;
    for (; i < hw - 7; i += 8) {
        uint8x8x4_t v_u8 = vld4_u8(src + i * 4);
        uint16x8_t b_u16 = vmovl_u8(v_u8.val[0]);
        uint16x8_t g_u16 = vmovl_u8(v_u8.val[1]);
        uint16x8_t r_u16 = vmovl_u8(v_u8.val[2]);
        uint16x8_t a_u16 = vmovl_u8(v_u8.val[3]);

        vf16.val[0] = vcvtq_f16_u16(reverse_channel ? r_u16 : b_u16);
        vf16.val[1] = vcvtq_f16_u16(g_u16);
        vf16.val[2] = vcvtq_f16_u16(reverse_channel ? b_u16 : r_u16);
        vf16.val[3] = vcvtq_f16_u16(a_u16);

        vf16.val[0] = vaddq_f16(bias_neon_b, vmulq_n_f16(vf16.val[0], scale_half[0]));
        vf16.val[1] = vaddq_f16(bias_neon_g, vmulq_n_f16(vf16.val[1], scale_half[1]));
        vf16.val[2] = vaddq_f16(bias_neon_r, vmulq_n_f16(vf16.val[2], scale_half[2]));
        vf16.val[3] = vaddq_f16(bias_neon_a, vmulq_n_f16(vf16.val[3], scale_half[3]));

        if (channel == 3) {
            vf16.val[3] = vdupq_n_f16(0.0f);
        }

        float16x8x4_t vf16_dump;
        vf16_dump.val[0] = vzip1q_f16(vf16.val[0], vzero);
        vf16_dump.val[1] = vzip1q_f16(vf16.val[1], vzero);
        vf16_dump.val[2] = vzip1q_f16(vf16.val[2], vzero);
        vf16_dump.val[3] = vzip1q_f16(vf16.val[3], vzero);
        vst4q_f16(dst + i * 8, vf16_dump);
        vf16_dump.val[0] = vzip2q_f16(vf16.val[0], vzero);
        vf16_dump.val[1] = vzip2q_f16(vf16.val[1], vzero);
        vf16_dump.val[2] = vzip2q_f16(vf16.val[2], vzero);
        vf16_dump.val[3] = vzip2q_f16(vf16.val[3], vzero);
        vst4q_f16(dst + i * 8 + 32, vf16_dump);
    }
#endif
    for (; i < hw; ++i) {
        dst[8 * i + 0] = scale_half[0] * fp16_t(src[4 * i + (reverse_channel ? 2 : 0)]) + bias_half[0];
        dst[8 * i + 1] = scale_half[1] * fp16_t(src[4 * i + 1]) + bias_half[1];
        dst[8 * i + 2] = scale_half[2] * fp16_t(src[4 * i + (reverse_channel ? 0 : 2)]) + bias_half[2];
        dst[8 * i + 3] = scale_half[3] * fp16_t(src[4 * i + 3]) + bias_half[3];
        if (channel == 3) {
            dst[8 * i + 3] = 0.0f;
        }
    }
}

template void BGRAToBlobImpl<true>(const uint8_t *src, fp16_t *dst, const float *scale, const float *bias,
                                   int hw, int channel);
template void BGRAToBlobImpl<false>(const uint8_t *src, fp16_t *dst, const float *scale, const float *bias,
                                    int hw, int channel);


/*
convert data type from uint8 to half, data format from nhw3 2 nc8hw8
*/
template <bool reverse_channel>
void BGRToBlobImpl(const uint8_t *src, fp16_t *dst, const float *scale, const float *bias, int hw) {
    int i = 0;
    fp16_t scale_half[3] = {fp16_t(scale[0]), fp16_t(scale[1]), fp16_t(scale[2])};
    fp16_t bias_half[3]  = {fp16_t(bias[0]), fp16_t(bias[1]), fp16_t(bias[2])};
#if (defined TNN_USE_NEON) && (TNN_ARM82) && (!defined TNN_ARM82_SIMU) && (__aarch64__)
    float16x8_t bias_neon_b = vdupq_n_f16(bias_half[0]);
    float16x8_t bias_neon_g = vdupq_n_f16(bias_half[1]);
    float16x8_t bias_neon_r = vdupq_n_f16(bias_half[2]);
    float16x8_t vzero       = vdupq_n_f16(0.0f);
    float16x8x4_t vf16;
    vf16.val[3] = vzero;
    for (; i < hw - 7; i += 8) {
        uint8x8x3_t v_u8 = vld3_u8(src + i * 3);
        uint16x8_t b_u16 = vmovl_u8(v_u8.val[0]);
        uint16x8_t g_u16 = vmovl_u8(v_u8.val[1]);
        uint16x8_t r_u16 = vmovl_u8(v_u8.val[2]);

        vf16.val[0] = vcvtq_f16_u16(reverse_channel ? r_u16 : b_u16);
        vf16.val[1] = vcvtq_f16_u16(g_u16);
        vf16.val[2] = vcvtq_f16_u16(reverse_channel ? b_u16 : r_u16);

        vf16.val[0] = vaddq_f16(bias_neon_b, vmulq_n_f16(vf16.val[0], scale_half[0]));
        vf16.val[1] = vaddq_f16(bias_neon_g, vmulq_n_f16(vf16.val[1], scale_half[1]));
        vf16.val[2] = vaddq_f16(bias_neon_r, vmulq_n_f16(vf16.val[2], scale_half[2]));

        float16x8x4_t vf16_dump;
        vf16_dump.val[0] = vzip1q_f16(vf16.val[0], vzero);
        vf16_dump.val[1] = vzip1q_f16(vf16.val[1], vzero);
        vf16_dump.val[2] = vzip1q_f16(vf16.val[2], vzero);
        vf16_dump.val[3] = vzip1q_f16(vf16.val[3], vzero);
        vst4q_f16(dst + i * 8, vf16_dump);
        vf16_dump.val[0] = vzip2q_f16(vf16.val[0], vzero);
        vf16_dump.val[1] = vzip2q_f16(vf16.val[1], vzero);
        vf16_dump.val[2] = vzip2q_f16(vf16.val[2], vzero);
        vf16_dump.val[3] = vzip2q_f16(vf16.val[3], vzero);
        vst4q_f16(dst + i * 8 + 32, vf16_dump);
    }
#endif
    for (; i < hw; ++i) {
        dst[8 * i + 0] = scale_half[0] * fp16_t(src[3 * i + (reverse_channel ? 2 : 0)]) + bias_half[0];
        dst[8 * i + 1] = scale_half[1] * fp16_t(src[3 * i + 1]) + bias_half[1];
        dst[8 * i + 2] = scale_half[2] * fp16_t(src[3 * i + (reverse_channel ? 0 : 2)]) + bias_half[2];
        dst[8 * i + 3] = 0.0f;
    }
}

template void BGRToBlobImpl<true>(const uint8_t *src, fp16_t *dst, const float *scale, const float *bias, int hw);
template void BGRToBlobImpl<false>(const uint8_t *src, fp16_t *dst, const float *scale, const float *bias, int hw);

/*
convert data type from uint8 to half, data format from nhw1 2 nc8hw8
*/
void GrayToBlob(const uint8_t *src, fp16_t *dst, const float scale, const float bias, int hw) {
    int i = 0;
    fp16_t scale_half = fp16_t(scale);
    fp16_t bias_half  = fp16_t(bias);
    memset(dst, 0, hw * 8 * sizeof(fp16_t));
#if (defined TNN_USE_NEON) && (TNN_ARM82) && (!defined TNN_ARM82_SIMU) && (__aarch64__)
    float16x8_t scale_neon = vdupq_n_f16(scale_half);
    float16x8_t bias_neon  = vdupq_n_f16(bias_half);
    for (; i < hw - 7; i += 8) {
        uint8x8_t v_u8   = vld1_u8(src + i);
        float16x8_t vf16 = vcvtq_f16_u16(vmovl_u8(v_u8));
        float16x8_t rf16 = vaddq_f16(bias_neon, vmulq_f16(scale_neon, vf16));

        vst1q_lane_f16(dst + (i + 0) * 8, rf16, 0);
        vst1q_lane_f16(dst + (i + 1) * 8, rf16, 1);
        vst1q_lane_f16(dst + (i + 2) * 8, rf16, 2);
        vst1q_lane_f16(dst + (i + 3) * 8, rf16, 3);
        vst1q_lane_f16(dst + (i + 4) * 8, rf16, 4);
        vst1q_lane_f16(dst + (i + 5) * 8, rf16, 5);
        vst1q_lane_f16(dst + (i + 6) * 8, rf16, 6);
        vst1q_lane_f16(dst + (i + 7) * 8, rf16, 7);
    }
#endif
    for (; i < hw; ++i) {
        dst[8 * i] = scale_half * fp16_t(src[i]) + bias_half;
    }
}


template <bool reverse_channel>
void BlobToBGRAImpl(const fp16_t *src, uint8_t *dst, const float *scale, const float *bias,
                           int hw, int channel) {
    int i = 0;
    fp16_t scale_half[4] = {fp16_t(scale[0]), fp16_t(scale[1]), fp16_t(scale[2]), fp16_t(scale[3])};
    fp16_t bias_half[4]  = {fp16_t(bias[0]), fp16_t(bias[1]), fp16_t(bias[2]), fp16_t(bias[3])};
#if (defined TNN_USE_NEON) && (TNN_ARM82) && (!defined TNN_ARM82_SIMU) && (__aarch64__)
    float16x8_t bias_neon_b = vdupq_n_f16(bias_half[0]);
    float16x8_t bias_neon_g = vdupq_n_f16(bias_half[1]);
    float16x8_t bias_neon_r = vdupq_n_f16(bias_half[2]);
    float16x8_t bias_neon_a = vdupq_n_f16(bias_half[3]);
    uint8x8x4_t vi8x4;
    float16x8x4_t vf16;
    for (; i < hw - 7; i += 8) {
        float16x8x4_t vf16_0 = vld4q_f16(src + i * 8);
        float16x8x4_t vf16_1 = vld4q_f16(src + i * 8 + 32);
        vf16.val[0] = vuzp1q_f16(vf16_0.val[0], vf16_1.val[0]);
        vf16.val[1] = vuzp1q_f16(vf16_0.val[1], vf16_1.val[1]);
        vf16.val[2] = vuzp1q_f16(vf16_0.val[2], vf16_1.val[2]);
        vf16.val[3] = vuzp1q_f16(vf16_0.val[3], vf16_1.val[3]);

        vf16.val[0] = vaddq_f16(bias_neon_b, vmulq_n_f16(vf16.val[0], scale_half[0]));
        vf16.val[1] = vaddq_f16(bias_neon_g, vmulq_n_f16(vf16.val[1], scale_half[1]));
        vf16.val[2] = vaddq_f16(bias_neon_r, vmulq_n_f16(vf16.val[2], scale_half[2]));
        vf16.val[3] = vaddq_f16(bias_neon_a, vmulq_n_f16(vf16.val[3], scale_half[3]));

        int16x8_t s16_0 = vcvtaq_s16_f16(vf16.val[reverse_channel ? 2 : 0]);
        int16x8_t s16_1 = vcvtaq_s16_f16(vf16.val[1]);
        int16x8_t s16_2 = vcvtaq_s16_f16(vf16.val[reverse_channel ? 0 : 2]);
        int16x8_t s16_3 = vcvtaq_s16_f16(vf16.val[3]);

        vi8x4.val[0] = vqmovun_s16(s16_0);
        vi8x4.val[1] = vqmovun_s16(s16_1);
        vi8x4.val[2] = vqmovun_s16(s16_2);
        vi8x4.val[3] = vqmovun_s16(s16_3);

        if (channel == 3) {
            uint8x8x4_t vi8x4_tmp = vld4_u8(dst + i * 4);
            vi8x4.val[3]          = vi8x4_tmp.val[3];
        }

        vst4_u8(dst + i * 4, vi8x4);
    }
#endif
    for (; i < hw; ++i) {
        dst[4 * i + 0] = half2uint8(reverse_channel ? (scale_half[2] * fp16_t(src[8 * i + 2]) + bias_half[2]) :
                                                       (scale_half[0] * fp16_t(src[8 * i + 0]) + bias_half[0]));
        dst[4 * i + 1] = half2uint8(scale_half[1] * fp16_t(src[8 * i + 1]) + bias_half[1]);
        dst[4 * i + 2] = half2uint8(reverse_channel ? (scale_half[0] * fp16_t(src[8 * i + 0]) + bias_half[0]) :
                                                       (scale_half[2] * fp16_t(src[8 * i + 2]) + bias_half[2]));
        if (channel == 4) {
            dst[4 * i + 3] = half2uint8(scale_half[3] * fp16_t(src[8 * i + 3]) + bias_half[3]);
        }
    }
}

template void BlobToBGRAImpl<true>(const fp16_t *src, uint8_t *dst, const float *scale, const float *bias,
                           int hw, int channel);
template void BlobToBGRAImpl<false>(const fp16_t *src, uint8_t *dst, const float *scale, const float *bias,
                           int hw, int channel);


template <bool reverse_channel>
void BlobToBGRImpl(const fp16_t *src, uint8_t *dst, const float *scale, const float *bias, int hw) {
    int i = 0;
    fp16_t scale_half[3] = {fp16_t(scale[0]), fp16_t(scale[1]), fp16_t(scale[2])};
    fp16_t bias_half[3]  = {fp16_t(bias[0]), fp16_t(bias[1]), fp16_t(bias[2])};
#if (defined TNN_USE_NEON) && (TNN_ARM82) && (!defined TNN_ARM82_SIMU) && (__aarch64__)
    float16x8_t bias_neon_b = vdupq_n_f16(bias_half[0]);
    float16x8_t bias_neon_g = vdupq_n_f16(bias_half[1]);
    float16x8_t bias_neon_r = vdupq_n_f16(bias_half[2]);
    uint8x8x3_t vi8x3;
    float16x8x3_t vf16;
    for (; i < hw - 7; i += 8) {
        float16x8x4_t vf16_0 = vld4q_f16(src + i * 8);
        float16x8x4_t vf16_1 = vld4q_f16(src + i * 8 + 32);
        vf16.val[0] = vuzp1q_f16(vf16_0.val[0], vf16_1.val[0]);
        vf16.val[1] = vuzp1q_f16(vf16_0.val[1], vf16_1.val[1]);
        vf16.val[2] = vuzp1q_f16(vf16_0.val[2], vf16_1.val[2]);

        vf16.val[0] = vaddq_f16(bias_neon_b, vmulq_n_f16(vf16.val[0], scale_half[0]));
        vf16.val[1] = vaddq_f16(bias_neon_g, vmulq_n_f16(vf16.val[1], scale_half[1]));
        vf16.val[2] = vaddq_f16(bias_neon_r, vmulq_n_f16(vf16.val[2], scale_half[2]));

        int16x8_t s16_0 = vcvtaq_s16_f16(vf16.val[reverse_channel ? 2 : 0]);
        int16x8_t s16_1 = vcvtaq_s16_f16(vf16.val[1]);
        int16x8_t s16_2 = vcvtaq_s16_f16(vf16.val[reverse_channel ? 0 : 2]);

        vi8x3.val[0] = vqmovun_s16(s16_0);
        vi8x3.val[1] = vqmovun_s16(s16_1);
        vi8x3.val[2] = vqmovun_s16(s16_2);

        vst3_u8(dst + i * 3, vi8x3);
    }
#endif
    for (; i < hw; ++i) {
        dst[3 * i + 0] = half2uint8(reverse_channel ? (scale_half[2] * fp16_t(src[8 * i + 2]) + bias_half[2]) :
                                                       (scale_half[0] * fp16_t(src[8 * i + 0]) + bias_half[0]));
        dst[3 * i + 1] = half2uint8(scale_half[1] * fp16_t(src[8 * i + 1]) + bias_half[1]);
        dst[3 * i + 2] = half2uint8(reverse_channel ? (scale_half[0] * fp16_t(src[8 * i + 0]) + bias_half[0]) :
                                                       (scale_half[2] * fp16_t(src[8 * i + 2]) + bias_half[2]));
    }
}


template void BlobToBGRImpl<true>(const fp16_t *src, uint8_t *dst, const float *scale, const float *bias, int hw);
template void BlobToBGRImpl<false>(const fp16_t *src, uint8_t *dst, const float *scale, const float *bias, int hw);

template <> void ScaleBias(fp16_t *src, int channel, int hw, const float *scale, const float *bias, fp16_t *dst) {
    if (dst == nullptr) {
        dst = src;
    }

    RawBuffer scale_buffer(ROUND_UP(channel, 8) * sizeof(fp16_t));
    RawBuffer bias_buffer(ROUND_UP(channel, 8) * sizeof(fp16_t));
    Float2Half(scale_buffer.force_to<fp16_t *>(), scale, channel);
    Float2Half(bias_buffer.force_to<fp16_t *>(), bias, channel);
    auto local_scale = scale_buffer.force_to<fp16_t *>();
    auto local_bias  = bias_buffer.force_to<fp16_t *>();

    for (int z = 0; z < UP_DIV(channel, 8); ++z) {
        auto src_z   = src + z * hw * 8;
        auto dst_z   = dst + z * hw * 8;

        auto v_scale = Half8::load(local_scale + z * 8);
        auto v_bias  = Half8::load(local_bias + z * 8);
        for (int s = 0; s < hw; ++s) {
            Half8 dst_v = v_bias;
            Half8::mla(dst_v, Half8::load(src_z + s * 8), v_scale);
            Half8::save(dst_z + s * 8, dst_v);
        }
    }
}

}  // namespace TNN_NS