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

/**
 * @file AampcliVirtualChannelMap.cpp
 * @brief Aampcli VirtualChannelMap handler
 */

#include "AampcliVirtualChannelMap.h"

VirtualChannelMap mVirtualChannelMap;

const char* g_initialize_player_mpd = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="A5EK5E17WypyY6sQHCnkf_HD_1080p_SDR" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT0H0M1S" mediaPresentationDuration="PT1.92S">
  <Period id="1" start="PT0H0M0.000S">
    <BaseURL>https://vod0-gb-s8-prd-ak.cdn01.skycdp.com/v2/bmff/cenc/t/ipvod6/cd2f7f30-27ef-4d12-bef8-1afa7839b023/1754019057/M/HD/</BaseURL>
    <AdaptationSet id="10002" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="600e81c1-a08b-56af-3ada-66433c152d5f"/>
      <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
        <cenc:pssh>AAAVFnBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAFPZleUo0TlhRalV6STFOaUk2SWxkMWVXVTFTMWd5U1VGVE1Yb3RXbFZDWTJWU1RUazBiVlpxV2pscmRFbHRjMFpHVms1bFUwOTBVbmNpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklsTkxXVlZMTFVORlRrTXRWazlFVlVoRUxVMVZURlJKUzBWWklpd2lZMjl1ZEdWdWRGUjVjR1VpT2lKMmIyUjFhR1FpTENKamIyNTBaVzUwUTJ4aGMzTnBabWxqWVhScGIyNGlPaUp6YTNsMWExWnZaRlZvWkNJc0ltTnJiVkJ2YkdsamVTSTZJbE5MV1ZWTExVTkZUa010Vms5RVZVaEVMVTFWVEZSSlMwVlpJaXdpWTI5dWRHVnVkRWxrSWpvaVFUVkZTelZGTVRkWGVYQjVXVFp6VVVoRGJtdG1YMGhFWHpFd09EQndYMU5FVWlJc0ltTnZiblJsYm5STFpYbHpJam9pVnpOemFWcElTblJUTWxZMVUxZFJhVTlwU1RWT1ZFVjRUMFJzYTFwRE1EUmFiVkV6VEZSV2FWbDZSWFJOUkdocVRYa3dlRTlVU1RSTk1sRTFUa1JrYTFscVoybE1RMG93WTIxR2FtRjVTVFpsZVVvd1pWaENiRWxxYjJsWldGWnJZVmM0YVdaVGQybFpNblIwVkZkV01GbFhVbWhrUjBWcFQybEtVbUZyVGtSUlZtUk9WRlZHVlZOV2FFVldSV3Q0VkZWU2JtUXdNVlZSVkVKT1VrWldNMVJ0ZUhaVVZVWlZWVlZXUmxSVldUTmtia0pvVFc1a2NXTkVaSFJXYld4WFZYcFdUazFyYUcxTlJURkNWa1pXUmxKck5XeGpWR00xVWtad1FsbFViSEJpUlZKWFlVVldOVlJJVmtoUk1rNWFWVlJDVkdWclVrSlNWRXBEVTFWbmVWUlZiRWxsYTFKQ1kwZHdhRTFxUVRKWk1HTTFZekpHV0ZScVZrVlJia0kyV1ZST2MwMVhSalZOVjNCaFZucFdjVlJHYUdGa2JIQkpWbTA1WVZGNlJqQmFSbVEwVFVkR1dHUkhlR3hWV0dST1YxUktNR1JGT1hWUmJscHBVako0Y1ZwV1ZuTmhNRkpEWTBod2FFMHlkM2haV0d0NFlXeHdXRTVYY0UxWFJuQXlWMnRvVjJJeGNFUk5XRkpyVmpObmQxbFdaREJpUjFaU1pEQjBXazFxYkRGYVJXUlhaRmRTUldOSVFtRlJXR1J3VjFaU1YySkhSalpXYlhoT1ZrZFJlbHBXYUVOT1YxWlZWMjV3YWxZeWFIRlpiVEV3WWxabmVXRkhkRmxsYTFZelZEQlNRMlF4WjNwVWJYUnFXak5rUzFkcmFFdGtSVGwwWkVkNGJGWlhlSEpTUlU1U1RsVTFWVkpZYUZCU1IzaHlWMnROZDA1R2NIUlZWRTVOVmtaYWNGZFljRVprUlRGRllVZHdUbVZVUWpSVU1WSktUa1V3ZVZWVVZrOVNSMUp5VjFkd2JsUlZaSFJVYmxwcFlteEtjMWx0TlZKT2JVVjVWbXBXVTFJeFdqVlpWbWhoWVVkU1NHSklXbWxoTTFKeldsWldjMkV3VWtSVGJuQm9UVEozZUZsWWEzaGtNREZGVWxoU1drMXVVakJVUm1oRFlXeHdXRTVYY0d0aVZHeHlXa1prYjJFd2VGVlJXR1JPVlhwR2NWbFVTbE5qYTFKQ1VsUk9SVkV3Y0RaWlZFNXpUVmRHTlUxWVpFNVNSVll3VjFSS01HUkZlRmxSYlhCaFZucFdjVnBITURWaE1sSllZVWQwVFZaRlJqTlVWMnQ0WkVad1NFMVlTV2xtVTNnM1NXMVNlV0pWZEd4bFZXeHJTV3B2YVU1cVFYZGFWR2Q0V1hwRmRGbFVRVFJaYVRBeFRtMUdiVXhVVG1oYVIwVjBUbXBaTUUxNlRtcE5WRlY1V2tSV2JVbHBkMmxrU0Vwb1dUSnphVTl1YzJsa1NHeDNXbE5KTmtsdVduQmFSMVoyU1dsM2FXSlhiSFZWUjJ3MFdsZDRla2xxYjNkTVEwcDBXVmhvVVdGWWFHeGlTRTFwVDJwVk0wNXVNSE5KYlU1eVlsVXhiR1JIUm10WldGSm9TV3B2YVZWWGNFUlJNRVpZVkZVeFFsWkZiRmxTUmxKS1RWVXhSVm96WkU1V1JVVjNWRlZTVm1Rd05YTmlNREZDVmtaR1JsSlZVWGxaTW5neVRrUk9jbUl5U2tOVU1VNU1VWHBuTVZveFNtcGphMVpPVVZaU1ZsSlZXbEJpU0ZKeFYwYzFjMU13WXpGYU0wSnRZbXhXU0ZKV1JsQk9NV1JYWlVad1psRnRUa1ZSVlZWNVVXdHNTVTFyTVVwVFNIQkZVVmhDY1ZsVVNYZE9iVTVJVDFoT2FGWXdOREZTUlVwM1pXMUZlbUpFUm1obFZFWnhWMnhqTVdGcmVGbFhibHBoVTBaYWRsZHJUWGhrUjFKWVpVUkNhRll6VW5OYVZrWXpWRlpyZVdSSVVsQmlhMG95V1d0a2MyRnRWbFppUjNSRlVXNUNObGxVVG5OTlYwWTFUVmR3WVZaNlZuRlVSbWhoWkd4d1NWWnRPV0ZSZWtZd1drWmtORTFIUmxoa1IzaHNWVmhrVEZkVVNUVmtWMUpJVm01V2ExSklRbmRYYTBZellWWnNWVlp0ZUdobGJGcHpWRlpTYTAweVZsbFJhbFpzVmtad05sa3haRzloYlVwMFpFY3hXVTF0YUhKWFNIQkdaREE1UlZGdVpGbE5NRFZ5V1RKa00xTnNjRWxUYmxKUVlsaFNjMXBXVm5OaE1GSkVWVlJLVGxKRlNuTlVNRkpIWVdzeFZFMVhhRTVTUjJod1ZFWlNWazFzYkZoWFdGSk9UV3RhY2xkV1RYZE5helZ4VlZod1RrMXJNVFJVYkZKTFlUQTFXRmRWTVVoaVZUVXlXVzAxVTJKSFNuVlZWRnBvVFd4Wk1WVnJaRmRsVjBaWlYyMW9hMUl5ZURKWmJYUXdZa2RXVm1KSGRFVlJNSEEyV1ZST2MwMVhSalZOV0dST1VrVldNRmRVU2pCa1JYaFpVVzF3WVZaNlZuRmFSekExWVRKU1dHRkhkRTFXUlVZelZGWk5lR0Z0UlhsVmJrcEZVVlZWZWxKRlRrdGxiVVY2WWtSR2FHVlVSak5VVlZKR1pFWnJlV1JJVWsxWFJVcHhWMnhqTVdGdFVuUlBWM1JyVmpKb2NsUkdVa0prTURGd1RWaFNZVko2Um5sSmJqQnpaWGxLYTJOdE1VeGFXR3hLV2tOSk5rbHFRVFZPVkVreldsUlJla3hYV1hkT1ZFRjBXa1JSZWsxVE1XaFBWMXByVEZSTmVWbHFRbXROUjFFeFRucGpNazFEU1hOSmJsSjVXVmRPY2tscWNEZEpibEkxWTBkVmFVOXBTakpoVjFKc1lubEpjMGx0TVhCaWJFSndaVWRXYzJONVNUWk9WR016VEVOS2RGbFlhRkZoV0doc1lraE5hVTlxWTNsTlNEQnpTVzFPY21KVk1XeGtSMFpyV1ZoU2FFbHFiMmxWVjNCRVVUQkdXRlJWTVVKV1JXeFpVa1pTU2sxVk1VVmFNMlJPVmtWRmQxUlZVbFprTURWellqQXhRbFpHUmtaU1ZWSnZVMFZzVlZJemFGcFNSVGgwV2toVk1GVkhWWGRNVlhSM1RqTmtUbEZXVWxaU1ZWcElZekpaZDFkV1dsaFVSWFJzVm14T05XSnFXalJpUlhCQ1YyeFdRMUpWVmtOVk1qRkZVVlZWZVZGcmJFbE5hekZLVTBod1JWRllRbkZaVkVsM1RtMU9TRTlZVG1oV01EUXhVa1ZLZDJWdFJYcGlSRVpvWlZSR2NWZHNZekZoYTNoWlYyNWFZVk5HV25aWGEwMTRaRWRTV0dWRVFtaFdNMUp6V2xaR00xUldhM2xrU0ZKUVltdEtNbGxyWkhOaGJWWldZa2QwUlZGdVFqWlpWRTV6VFZkR05VMVhjR0ZXZWxaeFZFWm9ZV1JzY0VsV2JUbGhVWHBHTUZwR1pEUk5SMFpZWkVkNGJGVllaRXhYVkVrMVpGZFNTRlp1Vm10U1NFSjNWMnRHTTJGV2JGVldiWGhvWld4YWMxUldVbXROTWxaWlVXcFdiRlpHY0RaWk1XUnZZVzFLZEdSSE1WbE5iV2h5VjBod1JtUXdPVVZSYm1SWlRUQTFjbGt5WkROVGJIQkpVMjVTVUdKWVVuTmFWbFp6WVRCU1JGVllaRkJXUmxZMVZHcEtWazFGTVRWTlZ6Rk9Va1pXTTFSR1pGSk5SVEUyVWxoU1dsWkhlSFJYYTAxM1pXc3hkRk5ZWkdGU1JVcHlWR3hTYWswd05YRlJWVEZJWWxVMU1sbHROVk5pUjBwMVZWUmFhRTFzV1RGVmEyUlhaVmRHV1ZkdGFHdFNNbmd5V1cxME1HSkhWbFppUjNSRlVUQndObGxVVG5OTlYwWTFUVmhrVGxKRlZqQlhWRW93WkVWNFdWRnRjR0ZXZWxaeFdrY3dOV0V5VWxoaFIzUk5Wa1ZHTTFSV1RYaGhiVVY1Vlc1S1JWRlZWWHBTUlU1TFpXMUZlbUpFUm1obFZFWXpWRlZTUm1SR2EzbGtTRkpOVjBWS2NWZHNZekZoYlZKMFQxZDBhMVl5YUhKVVJsSkNaREF4Y0UxWVVtRlNla1o1U1c0d2MyVjVTbXRqYlRGTVdsaHNTbHBEU1RaSmFtTjNUMVJSTUU1cVl6Sk1WR2N3VFZSTmRGcEhSbWhPZVRGclRVZFZOVXhVVW1oTlYwVXpUVWROZWxwcVFtMU9RMGx6U1c1U2VWbFhUbkpKYW5BM1NXNVNOV05IVldsUGFVb3lZVmRTYkdKNVNYTkpiVEZ3WW14Q2NHVkhWbk5qZVVrMlRucEplRXhEU25SWldHaFJZVmhvYkdKSVRXbFBha1YzVDBSQ09VeERTbXBoTWpGT1dsaFNhRnBIUmpCWlUwazJTV3hHY1ZFd1RrSldNREZPVVZaU1NsZEZVbFZUVkVaT1VrZGtNMVJXVWtKTlJURkZWbGhrVDJKSE9VNVJWbEpTVWxWV1FtTldSbEpSTWs1VFdsZDRZVmRGUms1a1JYaHpaRE5vUkU5WFpISlVWVVpWVmxWV1IxTXdSbmxPVm14UlZXNUdjVlJYY0ZCYVYzQjZUVEJHUms1R1FuZFZWbTk1VFZWb1dWSkZSa1pOYTBwS1UwUktUbE5WYURaU1JVWjNZVzFGZVUxRVdtcFNlbXg2V1Zaa1QwNVZVa05qU0hCb1RUSjNlRmxZYTNoaGJIQllUbGR3VFZkR2NESlhhMmhYWWpGd1JFMVlVbXRXTTJkM1dWWmtNR0pIVmxKa01ERmFUVzVTTUZReU5VTmtiVXBJWWtkd2JGWlhlSEpTUlVwM1pXMUZlbUpFUm1obFZFWnhWMnhqTVdGcmVGbFhibHBoVTBaYWRsZHJUWGhrUjFKWVpVUkNhRll6VW5OYVZrWXpVekZyZVU5WVZtdFNNVm94V2tWU2QyTkdjRUprTW14YVZrWmFjMWxZY0ZkaVJURlZXa1JPYkZkRlNURmFWbEpoWlcxT1dHRkhjR2xpV0ZKMFYwUktiMkV4YURaU1dHUlFVa1ZLTTFkRVRrOWhNazV1WkRCd1lWTkZjREJVTWpFd1lrZFdWbUpIZEVWUk1VVjZWRlZTY2sxRk5VVlhWRTVQWVZSQk1GUnJVa1psYTNoWVZXMW9XbFpIVGpCWGExSkRZa1U1VkUxRVFscFdSVnB2Vkc1d1EyRnJNSGxYV0dSaFlXeEdUbEl5TVU5a2JVcDFWVzE0YVdKc1JUSlpWRXBYVGxaS1NGWnViR2hYUm5CdldrVmtjMlJ0U25Ka1IzaHNWbGQ0Y2xKRlRrdGxiVVY2WWtSR2FHVlVSak5VVlZKR1pFWnJlV1JJVWsxWFJVcHhWMnhqTVdGdFVuUlBWM1JyVmpKb2NsUkdVa0prTURGVVRWZHdhRTFzU25sU1JVWkdUVEJTUkZOdWNHaE5NbmQ0V1ZocmVHUXdNVVZTV0ZKYVRXNVNNRlJHYUVOaGJIQllUbGR3YTJKVWJISmFSbVJ2WVRCNFZWRllaRTVoVkVZd1YydGplR05wU2psWVVUMDlJaXdpYVhOeklqb2lRMDQ5VURBd01UQXdNekF3TURBd01EYzFMQ0JQVlQxMWNtNDZjR0Z5ZEc1bGNqcHphM2wxYXpwd2Eya3RZM010YVc1ME9tTnJiUzFqY0dsNExDQlBQVU52YldOaGMzUWdRMjl1ZG1WeVoyVmtJRkJ5YjJSMVkzUnpJRXhNUXl3Z1REMVFhR2xzWVdSbGJIQm9hV0VzSUZOVVBWQkJMQ0JEUFZWVElpd2libUptSWpveE56VTBNREl4TVRBMkxDSnBZWFFpT2pFM05UUXdNakV4TURZc0luWmxjbk5wYjI0aU9pSXpJaXdpYldWemMyRm5aVlI1Y0dVaU9pSmpiMjUwWlc1MFRXVjBZV1JoZEdFaUxDSmhkV1FpT2lKa2JITWlMQ0pxZEdraU9pSjZiMmxpWVZCWmExcDVlREpXVjAxMWJHSm9lSEYzUFQwaWZRLmMzNzZtdXpfVTBRbmxnY19iLTh3cElPZHRaU1dPcDkwZmthVzNDZVhib3NIUnNLdnR3X3lrcGdQMTA2QU4wWFg3YWVZN1BHbVFSWVk5cFNwWWpaYUhTZHp4STV0RWppYmFsNlpPM2ozRFRBX0hRVzMzVDU2Z1ZTb29IZHBUMG1HS1dZNTJJNXloU0N2bThTNkVqWmlJVzRQZTNYMXkzM0d3ejg1aDVKUUs1YkN5bU9oUENhaDEtcm5jZGppN1pfcEpuanFCemt0WUxvMmpnZDgxbEtCYk1FRWNiWC11cXFDRzJneWZZS2JOSlVzUVRaM3NjMlZKLW5DUTlZR2xRc2czNHZsZWhISlRCQlpPbUZTbFlwRFdKTUVmWnJBSVhFWE5ldndNdm1sT2NmcVV3Qjc2ZG9naWUzMHlvckE3emhwSklNUzB1U1JxbGxxUU8tWlNUWFlHdw==</cenc:pssh>
      </ContentProtection>
      <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
        <cenc:pssh>AAAA3HBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAALwSJDk1MTE4OWRkLThmZDctNWJjMS0wOGMzLTE5MjgzZDk0N2RiOBIkNjAwZTgxYzEtYTA4Yi01NmFmLTNhZGEtNjY0MzNjMTUyZDVmEiQwOTUyN2U0My1mMDUwLWQ0MzEtYTlmZC0zMmIwZDBkNTc3NjASJDcwOTQ0Njc2LTg0MTMtZGFhNy1kMGU5LTRhMWE3MGMzZjBmNCIiQTVFSzVFMTdXeXB5WTZzUUhDbmtmX0hEXzEwODBwX1NEUg==</cenc:pssh>
      </ContentProtection>
      <SupplementalProperty schemeIdUri="urn:mpeg:dash:adaptation-set-switching:2016" value="20002,30002" />
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc-0-header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc-0-frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="4078"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="LE1" bandwidth="500000" codecs="hvc1.2.4.L93.b0" width="640" height="360" frameRate="25">
      </Representation>
    </AdaptationSet>
  </Period>
  <SupplementalProperty schemeIdUri="urn:scte:dash:powered-by" value="viper-mod_super8-4.19.1-1"/>
</MPD>)";

// coverity[ +tainted_string_sanitize_content : arg-0 ]
static bool sanitize(const char *data, size_t size)
{
	size_t length = strnlen(data, size);
	return ((length > 0) && (length < size));
}

void VirtualChannelMap::add(VirtualChannelInfo& channelInfo)
{
	if( !channelInfo.name.empty() )
	{
		if( !channelInfo.uri.empty() )
		{
			if( !VIRTUAL_CHANNEL_VALID(channelInfo.channelNumber) )
			{ // No explicit channel number set; automatically populate
				channelInfo.channelNumber = mMostRecentlyImportedVirtualChannelNumber+1;
			}

			if (isPresent(channelInfo) )
			{ // skip if duplicate
				return;
			}
			mMostRecentlyImportedVirtualChannelNumber = channelInfo.channelNumber;
		}
	}
	mVirtualChannelMap.push_back(channelInfo);
}

VirtualChannelInfo* VirtualChannelMap::find(const int channelNumber)
{
	VirtualChannelInfo *found = NULL;
	for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
	{
		VirtualChannelInfo &existingChannelInfo = *it;
		if (channelNumber == existingChannelInfo.channelNumber)
		{
			found = &existingChannelInfo;
			break;
		}
	}
	return found;
}
bool VirtualChannelMap::isPresent(const VirtualChannelInfo& channelInfo)
{
	bool didWarn = false;
	for( auto it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
	{
		const VirtualChannelInfo &existingChannelInfo = *it;
		if(channelInfo.channelNumber == existingChannelInfo.channelNumber)
		{ // forbid repeated virtual channel number!
			printf("[AAMPCLI] duplicate channel number: %d: '%s'\n", channelInfo.channelNumber, channelInfo.uri.c_str() );
			return true;
		}
		if(channelInfo.uri == existingChannelInfo.uri )
		{
			if( !didWarn )
			{ // warn for same url appearing more than once
				printf("[AAMPCLI] duplicate URL: %d: '%s'\n", channelInfo.channelNumber, channelInfo.uri.c_str() );
				didWarn = true;
			}
		}
	}
	return false;
}

// NOTE: prev() and next() are IDENTICAL other than the direction of the iterator. They could be collapsed using a template,
// but will all target compilers support this, it wouldn't save much code space, and may make the code harder to understand.
// Can not simply use different runtime iterators, as the types of each in C++ are not compatible (really).
VirtualChannelInfo* VirtualChannelMap::prev()
{
	VirtualChannelInfo *pPrevChannel = NULL;
	VirtualChannelInfo *pLastChannel = NULL;
	bool prevFound = false;

	// mCurrentlyTunedChannel is 0 for manually entered urls, not having used mVirtualChannelMap yet or empty
	if (mCurrentlyTunedChannel == 0 && mVirtualChannelMap.size() > 0)
	{
		prevFound = true;  // return the last valid channel
	}

	for(std::list<VirtualChannelInfo>::reverse_iterator it = mVirtualChannelMap.rbegin(); it != mVirtualChannelMap.rend(); ++it)
	{
		VirtualChannelInfo &existingChannelInfo = *it;
		if (VIRTUAL_CHANNEL_VALID(existingChannelInfo.channelNumber) ) // skip group headings
		{
			if ( pLastChannel == NULL )
			{ // remember this channel for wrap case
				pLastChannel = &existingChannelInfo;
			}
			if ( prevFound )
			{
				pPrevChannel = &existingChannelInfo;
				break;
			}
			else if ( existingChannelInfo.channelNumber == mCurrentlyTunedChannel )
			{
				prevFound = true;
			}
		}
	}
	if (prevFound && pPrevChannel == NULL)
	{
		pPrevChannel = pLastChannel;  // if we end up here we are probably at the first channel -- wrap to back
	}
	return pPrevChannel;
}

VirtualChannelInfo* VirtualChannelMap::next()
{
	VirtualChannelInfo *pNextChannel = NULL;
	VirtualChannelInfo *pFirstChannel = NULL;
	bool nextFound = false;

	// mCurrentlyTunedChannel is 0 for manually entered urls, not using mVirtualChannelMap
	if (mCurrentlyTunedChannel == 0 && mVirtualChannelMap.size() > 0)
	{
		nextFound = true; // return the first valid channel
	}

	for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
	{
		VirtualChannelInfo &existingChannelInfo = *it;
		if (VIRTUAL_CHANNEL_VALID(existingChannelInfo.channelNumber) ) // skip group headings
		{
			if ( pFirstChannel == NULL )
			{ // remember this channel for wrap case
				pFirstChannel = &existingChannelInfo;
			}
			if ( nextFound )
			{
				pNextChannel = &existingChannelInfo;
				break;
			}
			else if ( existingChannelInfo.channelNumber == mCurrentlyTunedChannel )
			{
				nextFound = true;
			}
		}
	}
	if (nextFound && pNextChannel == NULL)
	{
		pNextChannel = pFirstChannel;  // if we end up here we are probably at the last channel -- wrap to front
	}
	return pNextChannel;
}

void VirtualChannelMap::print(unsigned long start, unsigned long end, unsigned long tail)
{
	if (mVirtualChannelMap.empty())
	{
		return;
	}

	printf("[AAMPCLI] aampcli.cfg virtual channel map:\n");

	int numCols = 0;
	unsigned long lineCount = 0;
	unsigned long mapSize = mVirtualChannelMap.size();
	if(end == ULLONG_MAX || end >= mapSize)
	{
		end = mapSize - 1;
	}
	if(start >= mapSize)
	{
		start = 0;
	}
	if(tail)
	{
		if(tail >= mapSize)
		{
			start = 0;
		}
		else
		{
			start = mapSize - tail;
		}
	}

	for ( auto it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it )
	{
		if(lineCount < start)
		{
			//Skip over lines we don't want to display when a range or tail is specified.
			lineCount++;
			continue;
		}
		const VirtualChannelInfo &pChannelInfo = *it;
		std::string channelName = pChannelInfo.name.c_str();
		size_t len = channelName.length();
		int maxNameLen = 20;
		if( len>maxNameLen )
		{
			len = maxNameLen;
			channelName = channelName.substr(0,len);
		}
		if( pChannelInfo.uri.empty() )
		{
			if( numCols!=0 )
			{
				printf( "\n" );
			}
			printf( "%s\n", channelName.c_str() );
			numCols = 0;
			//Increment displayed lines here & check, as the "continue" bypasses the normal check.
			lineCount++;
			if(lineCount > end)
			{
				break;
			}			
			continue;
		}
		printf("%4d: %s", pChannelInfo.channelNumber, channelName.c_str() );
		if( numCols>=4 )
		{ // four virtual channels per row
			printf("\n");
			numCols = 0;
		}
		else
		{
			while( len<maxNameLen )
			{ // pad each column to 20 characters, for clean layout
				printf( " " );
				len++;
			}
			numCols++;
		}
		//Check if we've reached the end of lines we want to display
		lineCount++;
		if(lineCount > end)
		{
			break;
		}
	}
	printf("\n\n");
}

void VirtualChannelMap::setCurrentlyTunedChannel(int value)
{
	mCurrentlyTunedChannel = value;
}

void VirtualChannelMap::showList(unsigned long start, unsigned long end, unsigned long tail)
{
	printf("******************************************************************************************\n");
	printf("*   Virtual Channel Map\n");
	printf("******************************************************************************************\n");
	print(start, end, tail);
}

void VirtualChannelMap::tuneToChannel( VirtualChannelInfo &channel, PlayerInstanceAAMP *playerInstanceAamp, bool bAutoPlay)
{
	setCurrentlyTunedChannel(channel.channelNumber);
	const char *name = channel.name.c_str();
	const char *locator = channel.uri.c_str();
	printf( "TUNING to '%s' %s\n", name, locator );
	playerInstanceAamp->Tune(
    "fakeTune.mpd?fakeTune=true",                // mainManifestUrl
    true,                   // autoPlay
    "VOD", // contentType
    true,                   // bFirstAttempt
    false,                  // bFinalAttempt
    "trace-id-123",         // traceUUID
    false,                  // audioDecoderStreamSync
    nullptr,                // refreshManifestUrl
    0,                      // mpdStichingMode
    "session-id",           // sid
    g_initialize_player_mpd); // manifestData

}

std::string VirtualChannelMap::getNextFieldFromCSV( const char **pptr )
{
	const char *ptr = *pptr;
	const char *delim = ptr;
	const char *next = ptr;

	if (!isprint(*ptr) && *ptr != '\0')
	{  // Skip BOM UTF-8 start codes and not end of string
		while (!isprint(*ptr) && *ptr != '\0')
		{
			ptr++;
		}
		delim = ptr;
	}

	if( *ptr=='\"' )
	{ // Skip startquote
		ptr++;
		delim  = strchr(ptr,'\"');
		if( delim )
		{
			next = delim+1; // skip endquote
		}
		else
		{
			delim = ptr;
		}
	}
	else
	{  // Include space and greater chars and not , and not end of string
		while( *delim >= ' ' && *delim != ',' && *delim != '\0')
		{
			delim++;
		}
		next = delim;
	}

	if( *next==',' ) next++;
	*pptr = next;

	return std::string(ptr,delim-ptr);
}

void VirtualChannelMap::loadVirtualChannelMapFromCSV( FILE *f )
{
	char buf[MAX_BUFFER_LENGTH];
	while (fgets(buf, sizeof(buf), f))
	{
		// CID:280549 - Untrusted loop bound
		if (sanitize(buf, sizeof(buf)))
		{
			VirtualChannelInfo channelInfo;
			const char *ptr = buf;
			std::string channelNumber = getNextFieldFromCSV( &ptr );
			// invalid input results in 0 -- !VIRTUAL_CHANNEL_VALID, will be auto assigned
			channelInfo.channelNumber = atoi(channelNumber.c_str());
			channelInfo.name = getNextFieldFromCSV(&ptr);
			channelInfo.uri = getNextFieldFromCSV(&ptr);
			if (!channelInfo.name.empty() && !channelInfo.uri.empty())
			{
				add( channelInfo );
			}
			else
			{ // no name, no uri, no service
				//printf("[AAMPCLI] can not parse virtual channel '%s'\n", buf);
			}
		}
	}
}

/**
 * @brief Parse config entries for aamp-cli, and update mVirtualChannelMap
 *        based on the config.
 * @param f File pointer to config to process
 */
void VirtualChannelMap::loadVirtualChannelMapLegacyFormat( FILE *f )
{
	char buf[MAX_BUFFER_LENGTH];
	while (fgets(buf, sizeof(buf), f))
	{
		// CID:280433 - Untrusted loop bound
		if (!sanitize(buf, sizeof(buf)))
		{
			// Skip blank or overlong line
			continue;
		}

		const char *ptr = buf;
		ptr = skipwhitespace(ptr);
		if( *ptr=='#' )
		{ // comment line
			continue;
		}

		if( *ptr=='*' )
		{ // skip "*" character, if present
			ptr = skipwhitespace(ptr+1);
		}
		else
		{ // not a virtual channel
			continue;
		}

		VirtualChannelInfo channelInfo;		// extract channel number
		// invalid input results in 0 -- !VIRTUAL_CHANNEL_VALID, will be auto assigned
		channelInfo.channelNumber = atoi(ptr);
		while( *ptr>='0' && *ptr<='9' ) ptr++;
		ptr = skipwhitespace(ptr);

		// extract name
		const char *delim = ptr;
		while( *delim>' ' )
		{
			delim++;
		}
		channelInfo.name = std::string(ptr,delim-ptr);

		// extract locator
		ptr = skipwhitespace(delim);
		delim = ptr;
		while( *delim>' ' )
		{
			delim++;
		}
		channelInfo.uri = std::string(ptr,delim-ptr);

		add( channelInfo );
	}
} // loadVirtualChannelMapLegacyFormat

const char *VirtualChannelMap::skipwhitespace( const char *ptr )
{
	while( *ptr==' ' ) ptr++;
	return ptr;
}

