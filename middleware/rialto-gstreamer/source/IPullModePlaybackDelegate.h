/*
 * Copyright (C) 2025   
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

#pragma once

#include "IPlaybackDelegate.h"

class IPullModePlaybackDelegate : public IPlaybackDelegate
{
public:
    IPullModePlaybackDelegate() = default;
    ~IPullModePlaybackDelegate() override = default;

    IPullModePlaybackDelegate(const IPullModePlaybackDelegate &) = delete;
    IPullModePlaybackDelegate(IPullModePlaybackDelegate &&) = delete;
    IPullModePlaybackDelegate &operator=(const IPullModePlaybackDelegate &) = delete;
    IPullModePlaybackDelegate &operator=(IPullModePlaybackDelegate &&) = delete;

    virtual void setSourceId(int32_t sourceId) = 0;
    virtual void handleFlushCompleted() = 0;
    virtual GstRefSample getFrontSample() const = 0;
    virtual void popSample() = 0;
    virtual bool isEos() const = 0;
    virtual void lostState() = 0;
    virtual bool isReadyToSendData() const = 0;
};
