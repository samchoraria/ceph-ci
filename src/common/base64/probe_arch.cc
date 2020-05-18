/*
 * Copyright 2020 Liu Changcheng <changcheng.liu@aliyun.com>
 * Author: Liu Changcheng <changcheng.liu@aliyun.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "probe_arch.h"
#include "intel_simd.h"

int spec_probe_arch(void) {
    if (spec_arch_probed) {
        return 1;
    }
#if defined(__i386__) || defined(__x86_64__)
    spec_arch_intel_probe();
#endif
    spec_arch_probed = 1;
    return 1;
}

// do this once using the magic of c++.
int spec_arch_probed = spec_probe_arch();
