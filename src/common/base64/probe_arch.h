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

#ifndef SPEC_ARCH_PROBE_H
#define SPEC_ARCH_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

extern int spec_arch_probed;  /* non-zero if we've probed features */

extern int spec_probe_arch(void);

#ifdef __cplusplus
}
#endif

#endif
