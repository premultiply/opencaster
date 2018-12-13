/*  
 * Copyright (C) 2009-2013  Lorenzo Pallara, l.pallara@avalpa.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _BSD_SOURCE 1

#include <stdio.h> 
#include <stdio_ext.h> 
#include <unistd.h> 
#include <netinet/ether.h>
#include <netinet/in.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

#define EBU_UNIT_SIZE 46


void stamp_ts (unsigned long long int ts, unsigned char* buffer) 
{
	if (buffer) {
		buffer[0] = ((ts >> 29) & 0x0F) | 0x21;
		buffer[1] = (ts >> 22) & 0xFF; 
		buffer[2] = ((ts >> 14) & 0xFF ) | 0x01;
		buffer[3] = (ts >> 7) & 0xFF;
		buffer[4] = ((ts << 1) & 0xFF ) | 0x01;
	}
}


int main(int argc, char *argv[])
{
	unsigned long long int pts_stamp = 3600;
	unsigned char* pes_packet;
	int byte_read = 0;
	int file_es = 0;
	int packet_index = 0;
	int txtunitperpes = 0;
	int pts_increment = 1800;
	int txtunit = 0;
	int fieldmarker = 1;
	int buffer_offset = 0;
	
	/*  Parse args */
	if (argc > 2) {
		file_es = open(argv[1], O_RDONLY);
		txtunitperpes = atoi(argv[2]);
	} 
	if (argc > 3 ) {
		pts_stamp = strtoull(argv[3],0,0);
	}
	if (argc > 4) {
		pts_increment = atoi(argv[4]);
	}
	
	if (file_es == 0) {
		fprintf(stderr, "Usage: 'txt2pes txt.es txt_units_per_pes_packet [pts_offset [pts_increment]] > pes'\n");
		fprintf(stderr, "txt_unit_per_pes_packet increase bit rate, minimum is 1, max is 24\n");
		fprintf(stderr, "Set pts_increment to 0 to disable pts stamping\n");
		fprintf(stderr, "Default pts_offset 3600 and increment is 1800, means 2 fields or 1 frame\n");
		fprintf(stderr, "txt.es is 42 byte units of ebu teletext coding\n");
		return 2;
	} 
	
	unsigned short pes_size = ((txtunitperpes + 1) * EBU_UNIT_SIZE);
	pes_packet = malloc(pes_size);
	
	fprintf(stderr, "pes packet size without 6 byte header is %d\n", pes_size - 6);
	fprintf(stderr, "pes bitrate is %d bit/s\n", ((pes_size-1)/184+1) * 188 * 8 * (90000/(pts_increment > 0 ? pts_increment : 1800)));
	
	/* Set some init. values */
	memset(pes_packet, 0xFF, pes_size);
	pes_packet[0] = 0x00;
	pes_packet[1] = 0x00;
	pes_packet[2] = 0x01; /* prefix */
	pes_packet[3] = 0xBD; /* data txt */
	unsigned short temp = htons(pes_size - 6);
	memcpy(pes_packet + 4, &temp, sizeof(unsigned short)); 
	pes_packet[6] = 0x8F;
	if (pts_increment > 0) {
		pes_packet[7] = 0x2 << 6; /* flags */ //pts_dts_flags: 0x2 (2) => PTS fields shall be present in the PES packet header
	} else {
		pes_packet[7] = 0x0 << 6; /* flags */ //pts_dts_flags: 0x0 (0) => no PTS or DTS fields shall be present in the PES packet header
	}

	pes_packet[8] = 0x24; /* header size */
	/* 31 0xFF stuffing is here */
	pes_packet[45] = 0x10; /* ebu teletext */
	packet_index = EBU_UNIT_SIZE;
	
	/* Process the es txt file */
	byte_read = 1;
	while (byte_read) {
		byte_read = read(file_es, pes_packet + packet_index + 4 + buffer_offset, EBU_UNIT_SIZE - 4 - buffer_offset);
		if (byte_read != 0) {
			buffer_offset += byte_read;
			if (buffer_offset == (EBU_UNIT_SIZE - 4)) {
				pes_packet[packet_index + 0] = 0x02; // data_unit_id(8)
				pes_packet[packet_index + 1] = 0x2c; // data_unit_length(8)
				pes_packet[packet_index + 2] = (0x40) | (fieldmarker << 5) | (((22 - txtunitperpes + txtunit)) & 0x1f); // reserved_future_use(2), field_parity(1), line_offset(5)
				pes_packet[packet_index + 3] = 0xe4; // framing_code(8)
				packet_index += EBU_UNIT_SIZE;
				buffer_offset = 0;
				txtunit++;
				if (packet_index == pes_size) {
					if (pts_increment > 0) {
						stamp_ts(pts_stamp, pes_packet + 9);
						pts_stamp += pts_increment;
					}
					write(STDOUT_FILENO, pes_packet, pes_size);
					packet_index = EBU_UNIT_SIZE;
					fieldmarker ^= 1;
					txtunit = 0;
				}
			}
		} else {
			close(file_es);
			file_es = open(argv[1], O_RDONLY);
			byte_read = 1;
		}
	}
	
	if(pes_packet) {
		free(pes_packet);
	}
	
	return 0;
}

