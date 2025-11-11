/*
 * Copyright (C) 2022   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef GSTREAMERMSEUTILS_H
#define GSTREAMERMSEUTILS_H

#include <gst/gst.h>

#include <MediaCommon.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>
void rialto_mse_sink_setup_supported_caps(GstElementClass *elementClass,
                                          const std::vector<std::string> &supportedMimeType);
std::optional<firebolt::rialto::Layout> rialto_mse_sink_convert_layout(const gchar *layoutStr);
std::optional<firebolt::rialto::Format> rialto_mse_sink_convert_format(const gchar *formatStr);
std::shared_ptr<firebolt::rialto::CodecData> get_codec_data(const GstStructure *structure);
firebolt::rialto::SegmentAlignment get_segment_alignment(const GstStructure *s);
bool get_dv_profile(const GstStructure *s, uint32_t &dvProfile);
firebolt::rialto::StreamFormat get_stream_format(const GstStructure *structure);
#endif // GSTREAMERMSEUTILS_H
