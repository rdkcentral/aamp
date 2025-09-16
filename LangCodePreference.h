/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#ifndef LANG_CODE_PREFERENCE
#define LANG_CODE_PREFERENCE

/**
 *  @brief Language Code Preference types
 */
typedef enum
{
    ISO639_NO_LANGCODE_PREFERENCE, /**< AAMP will not normalize language codes - the client must use
                                      language codes that match the manifest when specifying audio
                                      or subtitle tracks, for example. */
    ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE,
    ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE,
    ISO639_PREFER_2_CHAR_LANGCODE
} LangCodePreference;

#endif // LANG_CODE_PREFERENCE

