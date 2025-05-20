/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define FAKE_FUNCTION(x) \
void x(); \
void x() {}

extern "C" {
FAKE_FUNCTION(ccGetAttributes)
FAKE_FUNCTION(ccGetCapability)
FAKE_FUNCTION(ccSetAttributes)
FAKE_FUNCTION(ccSetCCState)
FAKE_FUNCTION(ccSetAnalogChannel)
FAKE_FUNCTION(ccSetDigitalChannel)
FAKE_FUNCTION(media_closeCaptionStart)
FAKE_FUNCTION(media_closeCaptionStop)
FAKE_FUNCTION(vlGfxInit)
FAKE_FUNCTION(vlhal_cc_Register)
FAKE_FUNCTION(vlMpeosCCManagerInit)
}
