/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include <assert.h>

/**
 This is a simple mpegts demuxer with following assumptions/limitations
 - single program transport stream only (video-only segment or audio-only segment)
 - assumes PAT/PMT delivered in single packet
 */
#define TSDEMUX_PACKET_SIZE 188
#define TSDEMUX_STREAMTYPE_VIDEO 0xe0
#define TSDEMUX_STREAMTYPE_AUDIO 0xc0
#define TSDEMUX_MINIMAL_PARSING false

struct TsPart
{
	long start;
	double pts;
	double dts;
	double duration;
};

class TsDemux
{
private:
	bool firstPtsOnly;
	
	int mediaType;
	unsigned char *ptr; // raw data (mpegts)
	size_t len;
	
	long bits_read; // read head for parsing mpegts
	long bytes_written; // write head, for rewriting demuxed pes back into original buffer

	std::vector<TsPart> tsPart;
	
	struct ts
	{
		int transport_error_indicator;
		int payload_unit_start_indicator;
		int transport_priority;
		int pid;
		int transport_scrambling_control;
		int has_adaptation_field;
		int has_payload;
		int continuity_counter;
		int splice_countdown;
		int transport_private_data_length;
		int transport_stream_id;
		int version_number;
		int current_next_indicator;
		int section_number;
		int last_section_number ;
		int section_length;
		int program_number;
		int program_map_pid;
		int elementary_pid;
		int pcr_pid;
		int stream_type;
		int crc;
		long long pcr;
		long long opcr;
	} ts;
	
	struct pes
	{
		int stream_id;
		int pes_packet_length;
		int scrambling_control;
		int pes_priority;
		int data_alignment_indicator;
		int cpwrite;
		int original_or_copy;
		int escr_ext;
		int es_rate;
		int additional_copy_info;
		int pes_crc;
		int user_data;
		int pack_field_length;
		int packet_sequence_counter;
		int mpeg1_mpeg2_identifier;
		int original_stuffing_length;
		int p_std_buffer_scale;
		int p_std_buffer_size;
		int pes_extension_field_length;
		long long escr;
	} pes;
	
private:
	void writeByte( unsigned char value )
	{
		ptr[bytes_written++] = value;
	}
	
	int readBit( void )
	{
		int rc = 0;
		long byte_offset = bits_read/8;
		if( byte_offset < len )
		{
			long bit_index = bits_read&7;
			bits_read++;
			if( ptr[byte_offset] & (0x80>>bit_index) )
			{
				rc = 1;
			}
		}
		else
		{
			rc = -1;
		}
		return rc;
	}
	
	long long readBigNumber( int n )
	{
		assert( n>32 );
		long long rc = 0;
		while( n>0 )
		{
			rc <<= 1;
			int bit = readBit();
			if( bit<0 )
			{
				rc = -1;
				break;
			}
			rc |= bit;
			n--;
		}
		return rc;
	}
	
	int readBits( int n )
	{
		assert( n<=32 );
		int rc = 0;
		while( n>0 )
		{
			rc <<= 1;
			int bit = readBit();
			if( bit<0 )
			{
				rc = -1;
				break;
			}
			rc |= bit;
			n--;
		}
		return rc;
	}
	
	int readByte()
	{
		return readBits(8);
	}
	
	void skipBits( int n )
	{
		bits_read += n;
	}
	
	void parseAdaptationExtension( void )
	{
		int adaptation_extension_length = readByte();
		(void) adaptation_extension_length;
		if( TSDEMUX_MINIMAL_PARSING )
		{
			skipBits( adaptation_extension_length*8 );
		}
		else
		{
			int legal_time_window_flag = readBit();
			int piecewise_rate_flag = readBit();
			int seamless_splice_flag = readBit();
			skipBits(5); // reserved
			if( legal_time_window_flag )
			{
				int ltw_valid = readBit(); (void)ltw_valid;
				int ltw_offset = readBits( 15 ); (void)ltw_offset;
			}
			if( piecewise_rate_flag )
			{
				skipBits( 2 ); // reserved
				int piecewise_rate = readBits( 22 ); (void)piecewise_rate;
			}
			if( seamless_splice_flag )
			{
				int splice_type = readBits( 4 ); (void)splice_type;
				long long dts_next_access_unit = readBigNumber( 36 ); (void)dts_next_access_unit;
			}
		}
	}
	
	void parseAdaptationField( void )
	{
		int adaptation_field_length = readByte();
		if( TSDEMUX_MINIMAL_PARSING )
		{
			skipBits( adaptation_field_length*8 );
		}
		else
		{
			long fin = bits_read+adaptation_field_length*8;
			int discontinuity_indicator = readBit(); (void)discontinuity_indicator;
			int random_access_indicator = readBit(); (void)random_access_indicator;
			int elementary_stream_priority_indicator = readBit(); (void)elementary_stream_priority_indicator;
			int pcr_flag = readBit();
			int opcr_flag = readBit();
			int splicing_point_flag = readBit();
			int transport_private_data_flag = readBit();
			int adaptation_field_extension_flag = readBit();
			if( pcr_flag )
			{
				ts.pcr = readBigNumber( 48 );
			}
			if( opcr_flag )
			{
				ts.opcr = readBigNumber( 48 );
			}
			if( splicing_point_flag )
			{
				ts.splice_countdown = readByte();
			}
			if( transport_private_data_flag )
			{
				ts.transport_private_data_length = readByte();
			}
			if( adaptation_field_extension_flag )
			{
				parseAdaptationExtension();
			}
			while( bits_read < fin )
			{
				int stuffing_byte = readByte();
				(void)stuffing_byte;
				//assert( stuffing_byte == 0xff );
			}
		}
	}
	
	void parseSectionHeader( int expected_tableid )
	{
		int tableid = readByte();
		assert( tableid == expected_tableid );
		
		int section_syntax_indicator = readBit();
		assert( section_syntax_indicator == 1 );
		skipBits( 1 ); // reserved_future_use
		skipBits( 2 ); // reserved
		ts.section_length = readBits(12);
		
		ts.transport_stream_id = readBits(16);

		skipBits(2); // reserved
		ts.version_number = readBits(5);
		ts.current_next_indicator = readBit();

		ts.section_number = readByte();

		ts.last_section_number = readByte();
	}
	
	void parseCRC()
	{
		ts.crc = readBits(32);
	}
	
	void parsePAT()
	{
		parseSectionHeader( 0x00 );
		for( int i=0; i<1; i++ )
		{
			ts.program_number = readBits(16);
			skipBits(3); // reserved
			ts.program_map_pid = readBits(13);
		}
		parseCRC();
	}
		
	void parseSDT()
	{
		parseSectionHeader( 0x42 );
		int original_network_id = readBits(16); (void)original_network_id;
		skipBits(8); // reserved_future_use
		for( int i=0; i<1; i++ )
		{
			int service_id = readBits(16); (void)service_id;
			skipBits(6); // reserved_future_use
			int EIT_schedule_flag = readBit();
			assert( EIT_schedule_flag==0 );
			int EIT_present_following_flag = readBit();
			assert( EIT_present_following_flag==0 );
			int running_status = readBits(3); (void)running_status;
			int free_CA_mode = readBit(); (void)free_CA_mode;
			int descriptors_loop_length = readBits(12);
			for( int j=0; j<descriptors_loop_length; j++ )
			{
				int data = readByte(); (void)data;
			}
		}
		parseCRC();
	}
	
	void parsePMT()
	{
		parseSectionHeader( 0x02 );
		int reserved = readBits(3); assert( reserved == 0x07 );
		ts.pcr_pid = readBits( 13 );
		reserved = readBits( 4 ); assert( reserved == 0xf );
		reserved = readBits( 2 ); assert( reserved == 0 );
		int program_info_length = readBits( 10 );
		skipBits( program_info_length*8 );
		for( int program_count=0; program_count<2; program_count++ )
		{
			bool found = false;
			int stream_type = readByte();
			reserved = readBits(3); (void)reserved;
			int elementary_pid = readBits(13);
			reserved = readBits(4);
			assert( reserved == 0xf );
			reserved = readBits(2);
			assert( reserved == 0 );
			int es_info_length = readBits( 10 );
			skipBits( es_info_length*8 );
			//printf( "elementary_pid=0x%x stream_type=0x%x\n", elementary_pid, stream_type );
			switch( stream_type )
			{
				case 0x03: // eSTREAM_TYPE_MPEG1_AUDIO
				case 0x04: // eSTREAM_TYPE_MPEG2_AUDIO
				case 0x0f: // eSTREAM_TYPE_AAC_ADTS
				case 0x11: // eSTREAM_TYPE_AAC_LATM
				case 0x80: // eSTREAM_TYPE_ATSC_VIDEO
				case 0x81: // eSTREAM_TYPE_ATSC_AC3
				case 0x82: // eSTREAM_TYPE_HDMV_DTS
				case 0x83: // eSTREAM_TYPE_LPCM_AUDIO
				case 0x84: // eSTREAM_TYPE_ATSC_AC3PLUS
				case 0x86: // eSTREAM_TYPE_DTSHD_AUDIO
				case 0x87: // eSTREAM_TYPE_ATSC_EAC3
				case 0x8A: // eSTREAM_TYPE_DTS_AUDIO
				case 0x91: // eSTREAM_TYPE_AC3_AUDIO
				case 0x94: // eSTREAM_TYPE_SDDS_AUDIO1
					if( mediaType == eMEDIATYPE_AUDIO )
					{
						found = true;
					}
					break;
					
				case 0x02: //eSTREAM_TYPE_MPEG2_VIDEO
				case 0x1b: // eSTREAM_TYPE_H264
				case 0x24: // eSTREAM_TYPE_HEVC_VIDEO
					if( mediaType == eMEDIATYPE_VIDEO )
					{
						found = true;
					}
					break;
					
				default:
					assert(0);
					break;
			}
			if( found )
			{
				ts.stream_type = stream_type;
				ts.elementary_pid = elementary_pid;
				return;
			}
		}
		assert(0);
	}
	
	bool parseMpegTsHeader()
	{
		bool rc = false;
		int sync_byte = readByte();
		if( sync_byte>=0 )
		{
			assert( sync_byte == 0x47 );
			ts.transport_error_indicator = readBit(); // corrupt packet
			ts.payload_unit_start_indicator = readBit();
			ts.transport_priority = readBit();
			ts.pid = readBits( 13 );
			ts.transport_scrambling_control = readBits( 2 );
			ts.has_adaptation_field = readBit();
			ts.has_payload = readBit();
			ts.continuity_counter = readBits( 4 );
			rc = true;
		}
		return rc;
	}
	
	long long parseTimestamp()
	{
		long long rc = 0;
		
		rc |= ((long long)readBits(3))<<30;
		assert( readBit() );
		
		rc |= ((long long)readBits(15))<<15;
		assert( readBit() );
		
		rc |= readBits(15);
		assert( readBit() );
		return rc;
	}
	
	void parseOptionalPesHeader( void )
	{
		TsPart part;
		part.start = bytes_written;
		
		int marker_bits = readBits(2);
		assert( marker_bits == 0x2 );
		pes.scrambling_control = readBits(2);
		assert( pes.scrambling_control == 0x0 );
		pes.pes_priority = readBit();
		pes.data_alignment_indicator = readBit();
		pes.cpwrite = readBit();
		pes.original_or_copy = readBit();
		
		int pts_dts_indicator = readBits(2);
		// 0: no PTS/DTA
		// 2: PTS-only
		// 3: PTS followed by DTS
		int escr_flag = readBit();
		int es_rate_flag = readBit();
		int dsm_trick_mode_flag = readBit();
		int additional_copy_info_flag = readBit();
		int pes_crc_flag = readBit();
		int pes_extension_flag = readBit();
		int pes_header_length = readByte();
		if( TSDEMUX_MINIMAL_PARSING )
		{
			skipBits( pes_header_length*8 );
		}
		else
		{
			long fin = bits_read + pes_header_length*8;
			long long timestamp;
			switch( pts_dts_indicator )
			{
				case 2:
					assert( readBits(4) == 2 );
					timestamp = parseTimestamp();
					//printf( "pts=%lld\n", timestamp );
					part.pts = timestamp/90000.0;
					part.dts = part.pts;
					break;
					
				case 3:
					assert( readBits(4) == 3 );
					timestamp = parseTimestamp();
					//printf( "pts=%lld\n", timestamp );
					part.pts = timestamp/90000.0;
					
					assert( readBits(4) == 1 );
					timestamp = parseTimestamp();
					//printf( "dts=%lld\n", timestamp );
					part.dts = timestamp/90000.0;
					break;
					
				default:
					assert(0);
					break;
			}
			if( escr_flag )
			{// elementary stream clock reference
				assert( readBits(2) == 0 );
				pes.escr = parseTimestamp();
				pes.escr_ext = readBits(9);
				assert( readBit()==1 );
			}
			if( es_rate_flag )
			{
				assert( readBit()==1 );
				pes.es_rate = readBits(22);
				assert( readBit()==1 );
			}
			assert( dsm_trick_mode_flag == 0 );
			if( additional_copy_info_flag )
			{
				assert( readBit()==1 );
				pes.additional_copy_info = readBits(7);
			}
			
			if( pes_crc_flag )
			{
				pes.pes_crc = readBits(16);
			}
			
			if( pes_extension_flag )
			{
				int pes_private_data_flag = readBit();
				int pack_header_field_flag = readBit();
				int program_packet_sequence_counter_flag = readBit();
				int p_std_buffer_flag = readBit();
				assert( readBits(3) == 0x7 );
				int pes_extension_flag2 = readBit();
				
				if( pes_private_data_flag )
				{
					pes.user_data = readBits(16);
				}
				if( pack_header_field_flag )
				{
					pes.pack_field_length = readByte();
				}
				if( program_packet_sequence_counter_flag )
				{
					assert( readBit() );
					pes.packet_sequence_counter = readBits(7 );
					
					assert( readBit() );
					pes.mpeg1_mpeg2_identifier = readBit();
					pes.original_stuffing_length = readBits(6);
				}
				if( p_std_buffer_flag )
				{
					assert( readBits(2)==1 );
					pes.p_std_buffer_scale = readBit();
					pes.p_std_buffer_size = readBits(13);
				}
				if( pes_extension_flag2 )
				{
					assert( readBit() );
					pes.pes_extension_field_length = readBits(7);
					int reserved = readByte();
					(void)reserved;
				}
			} // pes_extension_flag
			
			while( bits_read < fin )
			{ // stuffing bytes
				assert( readByte()==0xff );
			}
		}
		tsPart.push_back(part);
	}
	
	void parsePES( void )
	{
		if( ts.payload_unit_start_indicator )
		{
			int start_code_prefix = readBits(24);
			if( start_code_prefix == 0x000001 )
			{
				pes.stream_id = readByte();
				assert( pes.stream_id == TSDEMUX_STREAMTYPE_VIDEO || pes.stream_id == TSDEMUX_STREAMTYPE_AUDIO );
				switch( mediaType )
				{
					case eMEDIATYPE_VIDEO:
						assert( pes.stream_id == TSDEMUX_STREAMTYPE_VIDEO );
						break;
					case eMEDIATYPE_AUDIO:
						assert( pes.stream_id == TSDEMUX_STREAMTYPE_AUDIO );
						break;
					default:
						assert(0);
						break;
				}
				pes.pes_packet_length = readBits(16);
				parseOptionalPesHeader();
			}
		}
		
		if( !firstPtsOnly )
		{
			while( bits_read%(TSDEMUX_PACKET_SIZE*8) )
			{
				int data = readByte();
				writeByte( data );
			}
		}
	}
	
	void parseTs( void )
	{
		while( parseMpegTsHeader() )
		{
			assert( ts.transport_error_indicator==0 );
			assert( ts.transport_scrambling_control==0 );
			if( ts.has_adaptation_field )
			{
				parseAdaptationField();
			}
			if( ts.has_payload )
			{
				if( ts.payload_unit_start_indicator )
				{
					if( ts.elementary_pid==0 || ts.pid != ts.elementary_pid )
					{ // packet data type is PSI (not PES)
						int payload_pointer = readByte();
						assert( payload_pointer == 0 ); // not yet handled
					}
				}
			}
			if( ts.pid == 0x00 )
			{
				parsePAT(); // 13
			}
			else if( ts.pid == 0x11 )
			{
				parseSDT(); // 37
			}
			else if( ts.pid == ts.program_map_pid )
			{
				parsePMT(); // 18
			}
			else if( ts.pid == ts.elementary_pid )
			{
				parsePES();
			}
			if( firstPtsOnly && tsPart.size()>0 )
			{ // short circuit
				return;
			}
			int excessBits = bits_read%(TSDEMUX_PACKET_SIZE*8);
			if( excessBits )
			{
				bits_read += TSDEMUX_PACKET_SIZE*8 - excessBits;
			}
		}
	}
	
public:
	TsDemux( int mediaType, gpointer ptr, size_t len, bool firstPtsOnly=false ): mediaType(mediaType), ptr((unsigned char *)ptr), len(len), bits_read(), bytes_written(), ts(), pes(), firstPtsOnly(firstPtsOnly)
	{
		parseTs();
	}
	
	int count( void )
	{
		return (int)tsPart.size();
	}
	
	unsigned char * getPtr( int part )
	{
		return &ptr[tsPart[part].start];
	}
	
	size_t getLen( int part )
	{
		size_t len;
		size_t start = tsPart[part].start;
		part++;
		if( part<tsPart.size() )
		{
			len = tsPart[part].start - start;
		}
		else
		{
			len = bytes_written - start;
		}
		return len;
	}
	
	double getPts( int part )
	{
		return tsPart[part].pts;
	}
	
	double getDts( int part )
	{
		return tsPart[part].dts;
	}
	
	double getDuration( int part )
	{
		return tsPart[1].dts - tsPart[0].dts; // FIXME
	}
	

	~TsDemux()
	{
	}
	
	TsDemux(const TsDemux & other): ptr(), len(), bits_read(), bytes_written(), ts(), pes(), tsPart()
	{ // stub copy constructor
		assert(0);
	}
	
	TsDemux& operator=(const TsDemux & other)
	{ // stub move constructor
		assert(0);
	}
};
