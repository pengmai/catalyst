// Copyright 2022-2023 Xanadu Quantum Technologies Inc.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstring>

#include <Exception.hpp>

extern "C" {
void *_mlir_memref_to_llvm_alloc(size_t size);
void *_mlir_memref_to_llvm_aligned_alloc(size_t alignment, size_t size);
void _mlir_memref_to_llvm_free(void *ptr);
}

template <typename T, size_t R> struct MemRefT {
    T *data_allocated;
    T *data_aligned;
    size_t offset;
    size_t sizes[R];
    size_t strides[R];
};

template <typename T, size_t R>
void memref_copy(MemRefT<T, R> *dst, MemRefT<T, R> *src, __attribute__((unused)) size_t bytes)
{
    char *srcPtr = (char *)src->data_allocated + dst->offset * sizeof(T);
    char *dstPtr = (char *)dst->data_allocated + dst->offset * sizeof(T);

    size_t *indices = static_cast<size_t *>(alloca(sizeof(size_t) * R));
    size_t *srcStrides = static_cast<size_t *>(alloca(sizeof(size_t) * R));
    size_t *dstStrides = static_cast<size_t *>(alloca(sizeof(size_t) * R));

    // Initialize index and scale strides.
    for (size_t rankp = 0; rankp < R; ++rankp) {
        indices[rankp] = 0;
        srcStrides[rankp] = src->strides[rankp] * sizeof(T);
        dstStrides[rankp] = dst->strides[rankp] * sizeof(T);
    }

    long writeIndex = 0;
    long readIndex = 0;
    __attribute__((unused)) size_t totalWritten = 0;
    for (;;) {
        memcpy(dstPtr + writeIndex, srcPtr + readIndex, sizeof(T));
        totalWritten += sizeof(T);
        RT_FAIL_IF(totalWritten > bytes, "wrote more than needed");
        // Advance index and read position.
        for (int64_t axis = R - 1; axis >= 0; --axis) {
            // Advance at current axis.
            size_t newIndex = ++indices[axis];
            readIndex += srcStrides[axis];
            writeIndex += dstStrides[axis];
            // If this is a valid index, we have our next index, so continue copying.
            if (src->sizes[axis] != newIndex)
                break;
            // We reached the end of this axis. If this is axis 0, we are done.
            if (axis == 0)
                return;
            // Else, reset to 0 and undo the advancement of the linear index that
            // this axis had. Then continue with the axis one outer.
            indices[axis] = 0;
            readIndex -= src->sizes[axis] * srcStrides[axis];
            writeIndex -= dst->sizes[axis] * dstStrides[axis];
        }
    }
}
