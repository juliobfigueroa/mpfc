/******************************************************************
 * Copyright (C) 2003 by SG Software.
 ******************************************************************/

/* FILE NAME   : disk_writer.c
 * PURPOSE     : SG MPFC. Disk writer output plugin functions
 *               implementation.
 * PROGRAMMER  : Sergey Galanov
 * LAST UPDATE : 30.10.2004
 * NOTE        : Module prefix 'dw'.
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
#include <sys/soundcard.h>
#include "types.h"
#include "file.h"
#include "outp.h"
#include "pmng.h"
#include "wnd.h"
#include "wnd_dialog.h"
#include "wnd_editbox.h"

/* Header size */
#define DW_HEAD_SIZE 44

/* Output file handler */
static file_t *dw_fd = NULL;

/* Song parameters */
static short dw_channels = 2;
static long dw_freq = 44100;
static dword dw_fmt = 0;
static long dw_file_size = 0;

/* Plugin data */
static pmng_t *dw_pmng = NULL;
static cfg_node_t *dw_cfg, *dw_root_cfg;

/* Plugin description */
static char *dw_desc = "Disk Writer plugin";

/* Plugin author */
static char *dw_author = "Sergey E. Galanov <sgsoftware@mail.ru>";

/* Logger object */
static logger_t *dw_log = NULL;

/* Start plugin */
bool_t dw_start( void )
{
	char name[MAX_FILE_NAME], full_name[MAX_FILE_NAME];
	char *str;
	int i;

	/* Get output file name */
	str = cfg_get_var(dw_root_cfg, "cur-song-name");
	if (str == NULL)
		return FALSE;
	util_strncpy(name, str, sizeof(name));
	str = strrchr(name, '.');
	if (str != NULL)
		strcpy(str, ".wav");
	else
		strcat(name, ".wav");
	util_replace_char(name, ':', '_');
	str = cfg_get_var(dw_cfg, "path");
	if (str != NULL)
		snprintf(full_name, sizeof(full_name), "%s/%s", str, name);
	else
		snprintf(full_name, sizeof(full_name), "%s", name);

	/* Try to open file */
	dw_fd = file_open(full_name, "w+b", NULL);
	if (dw_fd == NULL)
	{
		logger_error(dw_log, 1, _("Unable to create file %s"), full_name);
		return FALSE;
	}

	/* Leave space for header */
	file_seek(dw_fd, DW_HEAD_SIZE, SEEK_SET);
	dw_file_size = DW_HEAD_SIZE;
	return TRUE;
} /* End of 'dw_start' function */

/* Write WAV header */
static void dw_write_head( void )
{
	long chunksize1 = 16, chunksize2 = dw_file_size - DW_HEAD_SIZE;
	short format_tag = 1;
	short databits, block_align;
	long avg_bps;
	
	if (dw_fd == NULL)
		return;
	
	/* Set some variables */
	chunksize1 = 16;
	chunksize2 = dw_file_size - DW_HEAD_SIZE;
	format_tag = 1;
	databits = (dw_fmt == AFMT_U8 || dw_fmt == AFMT_S8) ? 8 : 16;
	block_align = databits * dw_channels / 8;
	avg_bps = dw_freq * block_align;

	/* Write header */
	file_seek(dw_fd, 0, SEEK_SET);
	file_write("RIFF", 1, 4, dw_fd);
	file_write(&dw_file_size, 4, 1, dw_fd);
	file_write("WAVE", 1, 4, dw_fd);
	file_write("fmt ", 1, 4, dw_fd);
	file_write(&chunksize1, 4, 1, dw_fd);
	file_write(&format_tag, 2, 1, dw_fd);
	file_write(&dw_channels, 2, 1, dw_fd);
	file_write(&dw_freq, 4, 1, dw_fd);
	file_write(&avg_bps, 4, 1, dw_fd);
	file_write(&block_align, 2, 1, dw_fd);
	file_write(&databits, 2, 1, dw_fd);
	file_write("data", 1, 4, dw_fd);
	file_write(&chunksize2, 4, 1, dw_fd);
} /* End of 'dw_write_head' function */

/* Stop plugin */
void dw_end( void )
{
	if (dw_fd != NULL)
	{
		dw_write_head();
		file_close(dw_fd);
		dw_fd = NULL;
	}
} /* End of 'dw_end' function */

/* Play stream */
void dw_play( void *buf, int size )
{
	if (dw_fd == NULL)
		return;
	
	file_write(buf, 1, size, dw_fd);
	dw_file_size += size;
} /* End of 'dw_play' function */

/* Set channels number */
void dw_set_channels( int ch )
{
	dw_channels = ch;
} /* End of 'dw_set_channels' function */

/* Set playing frequency */
void dw_set_freq( int freq )
{
	dw_freq = freq;
} /* End of 'dw_set_freq' function */

/* Set playing format */
void dw_set_fmt( dword fmt )
{
	dw_fmt = fmt;
} /* End of 'dw_set_bits' function */

/* Handle 'ok_clicked' message for configuration dialog */
wnd_msg_retcode_t dw_on_configure( wnd_t *wnd )
{
	editbox_t *eb = EDITBOX_OBJ(dialog_find_item(DIALOG_OBJ(wnd), "path"));
	assert(eb);
	cfg_set_var(dw_cfg, "path", EDITBOX_TEXT(eb));
	return WND_MSG_RETCODE_OK;
} /* End of 'dw_on_configure' function */

/* Launch configuration dialog */
void dw_configure( wnd_t *parent )
{
	dialog_t *dlg;

	dlg = dialog_new(parent, _("Configure disk writer plugin"));
	filebox_new_with_label(WND_OBJ(dlg->m_vbox), _("P&ath to store: "), 
			"path", cfg_get_var(dw_cfg, "path"), 'a', 50);
	wnd_msg_add_handler(WND_OBJ(dlg), "ok_clicked", dw_on_configure);
	dialog_arrange_children(dlg);
} /* End of 'dw_configure' function */

/* Exchange data with main program */
void plugin_exchange_data( plugin_data_t *pd )
{
	pd->m_desc = dw_desc;
	pd->m_author = dw_author;
	pd->m_configure = dw_configure;
	OUTP_DATA(pd)->m_start = dw_start;
	OUTP_DATA(pd)->m_end = dw_end;
	OUTP_DATA(pd)->m_play = dw_play;
	OUTP_DATA(pd)->m_set_channels = dw_set_channels;
	OUTP_DATA(pd)->m_set_freq = dw_set_freq;
	OUTP_DATA(pd)->m_set_fmt = dw_set_fmt;
	OUTP_DATA(pd)->m_flags = OUTP_NO_SOUND;
	dw_pmng = pd->m_pmng;
	dw_cfg = pd->m_cfg;
	dw_root_cfg = pd->m_root_cfg;
	dw_log = pd->m_logger;
} /* End of 'plugin_exchange_data' function */

/* End of 'writer.c' file */

