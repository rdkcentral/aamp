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

#ifndef MEDIAPLAYERMANAGER_H
#define MEDIAPLAYERMANAGER_H

#include "GStreamerMSEMediaPlayerClient.h"
#include <map>

class MediaPlayerManager
{
public:
    MediaPlayerManager();
    ~MediaPlayerManager();

    std::shared_ptr<GStreamerMSEMediaPlayerClient> getMediaPlayerClient() const;
    bool attachMediaPlayerClient(const GstObject *gstBinParent, const uint32_t maxVideoWidth = 0,
                                 const uint32_t maxVideoHeight = 0);
    void releaseMediaPlayerClient();
    bool hasControl();

private:
    struct MediaPlayerClientInfo
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client;
        void *controller;
        uint32_t refCount;
    };

    void createMediaPlayerClient(const GstObject *gstBinParent, const uint32_t maxVideoWidth,
                                 const uint32_t maxVideoHeight);
    bool acquireControl(MediaPlayerClientInfo &mediaPlayerClientInfo);

    std::weak_ptr<GStreamerMSEMediaPlayerClient> m_client;
    const GstObject *m_currentGstBinParent;

    static std::mutex m_mediaPlayerClientsMutex;
    static std::map<const GstObject *, MediaPlayerClientInfo> m_mediaPlayerClientsInfo;
};

#endif // MEDIAPLAYERMANAGER_H