/******************************************************************
 * Copyright (C) 2003 by SG Software.
 ******************************************************************/

/* FILE NAME   : wav.c
 * PURPOSE     : SG MPFC. WAV input plugin functions 
 *               implementation.
 * PROGRAMMER  : Sergey Galanov
 * LAST UPDATE : 12.07.2003
 * NOTE        : Module prefix 'wav'.
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
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, 
 * MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/soundcard.h>
#include "types.h"
#include "inp.h"
#include "song_info.h"
#include "util.h"
#include "wav.h"

/* Currently playing file descriptor and name */
FILE *wav_fd = NULL;
char wav_fname[256];

/* Current seek value */
int wav_seek_val = -1;

/* Current song audio parameters */
int wav_channels = 0, wav_freq = 0, wav_avg_bps = 0;
dword wav_fmt = 0;
int wav_file_size = 0;
int wav_block_align = 0;

/* Song length */
int wav_len = 0;

/* Data offset in file */
int wav_data_offset = 0;

/* Current time */
int wav_time = 0;

/* Start play function */
bool_t wav_start( char *filename )
{
	char riff[4], riff_type[4];
	long file_size;
	void *buf = NULL;
	dword data_size = 0;
	
	/* Try to open file */
	wav_fd = fopen(filename, "rb");
	if (wav_fd == NULL)
		return FALSE;
	strcpy(wav_fname, filename);

	/* Read WAV file header */
	fread(riff, 1, sizeof(riff), wav_fd);
	fread(&wav_file_size, 1, sizeof(file_size), wav_fd);
	fread(riff_type, 1, sizeof(riff_type), wav_fd);

	/* Check file validity */
	if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F' ||
			riff_type[0] != 'W' || riff_type[1] != 'A' ||
			riff_type[2] != 'V' || riff_type[3] != 'E')
	{
		wav_end();
		return FALSE;
	}

	/* Read chunks until 'data' */
	while (!wav_read_next_chunk(wav_fd, (void **)(&buf), &data_size));

	/* Check format */
	if (!data_size || buf == NULL || 
			!(WAV_FMT_GET_FORMAT(buf) == 1))
	{
		free(buf);
		wav_end();
		return FALSE;
	}

	/* Save song parameters */
	wav_data_offset = ftell(wav_fd);
	wav_channels = WAV_FMT_GET_CHANNELS(buf);
	wav_freq = WAV_FMT_GET_SAMPLES_PER_SEC(buf);
	wav_avg_bps = WAV_FMT_GET_AVG_BYTES_PER_SEC(buf);
	wav_block_align = WAV_FMT_GET_BLOCK_ALIGN(buf);
	switch (WAV_FMT_GET_FORMAT(buf))
	{
	case 1:
		wav_fmt = (WAV_PCM_FMT_GET_BPS(buf) == 8) ?
			AFMT_U8 : AFMT_S16_LE;
		break;
	}

	wav_len = data_size / wav_avg_bps;
	wav_seek_val = -1;
	wav_time = 0;
	free(buf);
	return TRUE;
} /* End of 'wav_start' function */

/* End playing function */
void wav_end( void )
{
	/* Close file */
	if (wav_fd != NULL)
	{
		strcpy(wav_fname, "");
		fclose(wav_fd);
		wav_time = 0;
		wav_fd = NULL;
	}
} /* End of 'wav_end' function */

/* Get supported formats function */
void wav_get_formats( char *buf )
{
	strcpy(buf, "wav");
} /* End of 'wav_get_formats' function */

/* Get song length */
int wav_get_len( char *filename )
{
	FILE *fd;
	void *buf;
	dword data_size;
	int len;

	/* If we are playing this file now - return its length */
	if (!strcmp(filename, wav_fname))
		return wav_len;

	/* Read file header */
	fd = fopen(filename, "rb");
	if (fd == NULL)
		return 0;
	fseek(fd, 12, SEEK_SET);

	/* Read needed information */
	while (!wav_read_next_chunk(fd, (void **)(&buf), &data_size));

	/* Check return */
	if (buf == NULL || !data_size)
	{
		free(buf);
		fclose(fd);
		return 0;
	}

	/* Extract song length */
	len = data_size / WAV_FMT_GET_AVG_BYTES_PER_SEC(buf);
	free(buf);
	return len;
} /* End of 'wav_get_len' function */

/* Get stream function */
int wav_get_stream( void *buf, int size )
{
	if (wav_fd != NULL)
	{
		if (wav_seek_val != -1)
		{
			fseek(wav_fd, wav_data_offset + wav_seek_val * wav_avg_bps, 
					SEEK_SET);
			wav_seek_val = -1;
		}
		
		memset(buf, 0, size);
		size = fread(buf, 1, size, wav_fd);
		wav_time = (ftell(wav_fd) - wav_data_offset) / wav_avg_bps;
	}
	else
		size = 0;
	return size;
} /* End of 'wav_get_stream' function */

/* Seek song */
void wav_seek( int val )
{
	if (wav_fd != NULL)
	{
		wav_seek_val = val;
	}
} /* End of 'wav_seek' function */

/* Get audio parameters */
void wav_get_audio_params( int *ch, int *freq, dword *fmt )
{
	*ch = wav_channels;
	*freq = wav_freq;
	*fmt = wav_fmt;
} /* End of 'wav_get_audio_params' function */

/* Get current time */
int wav_get_cur_time( void )
{
	return wav_time;
} /* End of 'wav_get_cur_time' function */

/* Get song information */
bool_t wav_get_info( char *filename, song_info_t *info )
{
	int size, samplerate, bps, channels, block_align, bits, len;
	
	/* Get audio parameters */
	if (!strcmp(filename, wav_fname))
	{
		samplerate = wav_freq;
		bps = wav_avg_bps;
		channels = wav_channels;
		block_align = wav_block_align;
		bits = (wav_fmt == AFMT_U8 || wav_fmt == AFMT_S8) ? 8 : 16;
		size = wav_file_size;
		len = wav_len;
		return TRUE;
	}
	else
	{
		FILE *fd;
		byte *buf;
		dword data_size = 0;

		/* Read file header */
		fd = fopen(filename, "rb");
		if (fd == NULL)
			return FALSE;
		fseek(fd, 4, SEEK_SET);
		fread(&size, 1, 4, fd);
		fseek(fd, 4, SEEK_CUR);
		while (!wav_read_next_chunk(fd, (void **)(&buf), &data_size));
		if (!data_size || buf == NULL || !(WAV_FMT_GET_FORMAT(buf) == 1))
		{
			fclose(fd);
			return FALSE;
		}
		fclose(fd);

		/* Get parameters */
		channels = WAV_FMT_GET_CHANNELS(buf);
		samplerate = WAV_FMT_GET_SAMPLES_PER_SEC(buf);
		bps = WAV_FMT_GET_AVG_BYTES_PER_SEC(buf);
		block_align = WAV_FMT_GET_BLOCK_ALIGN(buf);
		bits = WAV_PCM_FMT_GET_BPS(buf);
		len = data_size / bps;
		free(buf);
	}

	/* Fill info */
	memset(info, 0, sizeof(*info));
	info->m_only_own = TRUE;
	sprintf(info->m_own_data, 
			"File size: %i bytes\n"
			"Length: %i seconds\n"
			"Bits/sample: %i\n"
			"Format: PCM\n"
			"Channels: %i\n"
			"Samplerate: %i Hz\n"
			"Bytes/sec: %i\n"
			"Block align: %i",
			size, len, bits, channels, samplerate, bps, block_align);
	return TRUE;
} /* End of 'wav_get_info' function */ 

/* Get functions list */
void inp_get_func_list( inp_func_list_t *fl )
{
	fl->m_start = wav_start;
	fl->m_end = wav_end;
	fl->m_get_stream = wav_get_stream;
	fl->m_get_len = wav_get_len;
	fl->m_seek = wav_seek;
	fl->m_get_audio_params = wav_get_audio_params;
	fl->m_get_formats = wav_get_formats;
	fl->m_get_cur_time = wav_get_cur_time;
	fl->m_get_info = wav_get_info;
} /* End of 'inp_get_func_list' function */

/* Read the next chunk. Returns TRUE when 'data' chunk is read */
bool_t wav_read_next_chunk( FILE *fd, void **fmt_buf, dword *data_size )
{
	char chunk_id[4];
	long chunk_size;
	
	if (fd == NULL || feof(fd))
		return TRUE;

	/* Read chunk header */
	fread(chunk_id, 1, sizeof(chunk_id), fd);
	fread(&chunk_size, 1, sizeof(chunk_size), fd);

	/* Parse chunk */
	if (!strncmp(chunk_id, "data", 4))
	{
		(*data_size) = chunk_size;
		return TRUE;
	}
	else if (!strncmp(chunk_id, "fmt ", 4))
	{
		(*fmt_buf) = malloc(chunk_size);
		if ((*fmt_buf) == NULL)
			return FALSE;
		fread(*fmt_buf, 1, chunk_size, fd);
	}
	else
	{
		fseek(fd, chunk_size, SEEK_CUR);
	}

	return FALSE;
} /* End of 'wav_read_next_chunk' function */

/* End of 'wav.c' file */

