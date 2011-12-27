/******************************************************************
 * Copyright (C) 2005 - 2011 by SG Software.
 *
 * SG MPFC. Cue format support plugin.
 * $Id$ 
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

//#include "config.h"
#include <ctype.h>
#define __MPFC_OUTER__
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>
#include <sys/types.h>
#include <libgen.h>
#include <dirent.h>
#include <libcue/libcue.h>
#include "types.h"
#include "plp.h"
#include "pmng.h"
#include "util.h"

/* Plugins manager */
static pmng_t *cue_pmng = NULL;

/* Plugin description */
static char *cue_desc = "Cue format support plugin";

/* Plugin author */
static char *cue_author = "Sergey E. Galanov <sgsoftware@mail.ru>";

/* Logger */
static logger_t *cue_log = NULL;

/* Get supported formats function */
void cue_get_formats( char *extensions, char *content_type )
{
	if (extensions != NULL)
		strcpy(extensions, "cue");
	if (content_type != NULL)
		strcpy(content_type, "");
} /* End of 'cue_get_formats' function */

/* Cue sheets often have a .wav file specified
 * while actually relating to an encoded file
 * Try to fix this.
 * By the way return full name */
static char *cue_fix_wrong_file_ext( char *dir, char **name )
{
	size_t dir_len = strlen(dir) + 1; /* +1 means the additional '/' */
	size_t name_len = strlen(*name);

	/* Buid full path */
	char *path = (char*)malloc(dir_len + name_len + 
			cue_pmng->m_media_ext_max_len + 1);
	if (!path)
		return NULL;
	sprintf(path, "%s/%s", dir, *name);

	/* File exists */
	struct stat st;
	if (!stat(path, &st))
		return path;

	/* Find extension start */
	int ext_pos = name_len - 1;
	for ( ; ext_pos >= 0; ext_pos-- )
	{
		/* No extension found */
		if ((*name)[ext_pos] == '/')
		{
			ext_pos = -1;
			break;
		}
		if ((*name)[ext_pos] == '.')
			break;
	}
	if (ext_pos < 0)
		return path;
	ext_pos++;

	/* Try supported extensions */
	char *ext_start = path + dir_len + ext_pos;
	char *ext = pmng_first_media_ext(cue_pmng);
	for ( ; ext; ext = pmng_next_media_ext(ext) )
	{
		/* Try this extension */
		strcpy(ext_start, ext);
		if (!stat(path, &st))
		{
			/* Replace extension in the name */
			free(*name);
			(*name) = strdup(path + dir_len);
			return path;
		}
	}
	assert(!ext);

	/* Revert to the original (non-existant) path */
	strcpy(path + dir_len, *name);
	return path;
} /* End of 'cue_fix_wrong_file_ext' function */

/* Parse playlist and handle its contents */
plp_status_t cue_for_each_item( char *pl_name, void *ctx, plp_func_t f )
{
	/* Load cue sheet */
	FILE *fd = fopen(pl_name, "rt");
	if (!fd)
	{
		logger_error(cue_log, 0, "cue: failed to load cue sheet %s", pl_name);
		return PLP_STATUS_FAILED;
	}
	Cd *cd = cue_parse_file(fd);
	if (!cd)
	{
		logger_error(cue_log, 0, "cue: failed to load cue sheet %s", pl_name);
		fclose(fd);
		return PLP_STATUS_FAILED;
	}
	fclose(fd);

	/* Handle tracks */
	int num_tracks = cd_get_ntrack(cd);
	for ( int i = 1; i <= num_tracks; ++i )
	{
		Track *track = cd_get_track(cd, i);

		song_metadata_t metadata = SONG_METADATA_EMPTY;

		/* Set bounds */
		long start = track_get_start(track);
		long len = track_get_length(track);
		metadata.m_start_time = start / 75;
		metadata.m_end_time = (len == 0) ? -1 : (start + len) / 75;

		/* Set song info */
		song_info_t *si = si_new();
		si_set_album(si, cdtext_get(PTI_TITLE, cd_get_cdtext(cd)));
		si_set_year(si, rem_get(REM_DATE, cd_get_rem(cd)));
		si_set_artist(si, cdtext_get(PTI_PERFORMER, track_get_cdtext(track)));
		si_set_name(si, cdtext_get(PTI_TITLE, track_get_cdtext(track)));

		char track_num_str[10];
		snprintf(track_num_str, sizeof(track_num_str), "%02d", i);
		si_set_track(si, track_num_str);
		metadata.m_song_info = si;

		f(ctx, track_get_filename(track), &metadata);
	}

	/* Free memory */
	cd_delete(cd);
	return PLP_STATUS_OK;
} /* End of 'cue_for_each_item' function */

/* Exchange data with main program */
void plugin_exchange_data( plugin_data_t *pd )
{
	pd->m_desc = cue_desc;
	pd->m_author = cue_author;
	PLIST_DATA(pd)->m_get_formats = cue_get_formats;
	PLIST_DATA(pd)->m_for_each_item = cue_for_each_item;
	cue_pmng = pd->m_pmng;
	cue_log = pd->m_logger;
} /* End of 'plugin_exchange_data' function */

/* End of 'cue.c' file */

