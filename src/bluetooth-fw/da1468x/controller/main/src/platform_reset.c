/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "die.h"
#include "reboot_reason.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

//! Reset function provided for Dialog ROM to call into.
void platform_reset_func(uint32_t error) {
  RebootReason reason = {
    .code = RebootReasonCode_DialogPlatformReset,
    .extra = error,
  };
  reboot_reason_set(&reason);

  printf("PLF RST %"PRIx32"\n", error);
  reset_due_to_software_failure();
}
