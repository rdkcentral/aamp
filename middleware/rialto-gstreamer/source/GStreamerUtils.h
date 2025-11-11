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

#ifndef GSTREAMERUTILS_H
#define GSTREAMERUTILS_H

#include <gst/gst.h>
#include <stdint.h>

class GstMappedBuffer
{
public:
    explicit GstMappedBuffer(GstBuffer *buffer, GstMapFlags flags);
    ~GstMappedBuffer();
    uint8_t *data();
    size_t size() const;
    explicit operator bool() const;

private:
    GstBuffer *m_buffer;
    GstMapInfo m_info;
    bool m_isMapped;
};

class GstRefSample
{
public:
    explicit GstRefSample(GstSample *sample = nullptr);
    GstRefSample(const GstRefSample &) = delete;
    GstRefSample(GstRefSample &&) = delete;
    GstRefSample &operator=(const GstRefSample &) = delete;
    GstRefSample &operator=(GstRefSample &&) = delete;
    ~GstRefSample();
    explicit operator bool() const;
    GstBuffer *getBuffer() const;
    GstCaps *getCaps() const;

private:
    GstSample *m_sample;
};
#endif // GSTREAMERUTILS_H