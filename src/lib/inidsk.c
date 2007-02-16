/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:     src/lib/inidsk.c                                           *
 * Created:       2004-12-13 by Hampa Hug <hampa@hampa.ch>                   *
 * Copyright:     (C) 2004-2007 Hampa Hug <hampa@hampa.ch>                   *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

/* $Id$ */


#include <stdlib.h>

#include <lib/inidsk.h>
#include <lib/log.h>

#include <devices/block/blkcow.h>
#include <devices/block/blkdosem.h>
#include <devices/block/blkpart.h>
#include <devices/block/blkpce.h>
#include <devices/block/blkram.h>
#include <devices/block/blkraw.h>


int dsk_insert (disks_t *dsks, const char *str)
{
	unsigned i;
	unsigned drv;
	char     buf[16];
	disk_t   *dsk;

	i = 0;
	while ((i < 16) && (str[i] != 0)) {
		if (str[i] == ':') {
			buf[i] = 0;
			break;
		}

		buf[i] = str[i];

		i += 1;
	}

	if ((i == 0) || (str[i] == 0)) {
		return (1);
	}

	drv = strtoul (buf, NULL, 0);
	str = str + i + 1;

	dsk = dsk_auto_open (str, 0, 0);
	if (dsk == NULL) {
		return (1);
	}

	dsk_set_drive (dsk, drv);

	if (dsks_add_disk (dsks, dsk)) {
		dsk_del (dsk);
		return (1);
	}

	return (0);
}

disk_t *ini_get_disk (ini_sct_t *sct)
{
	disk_t        *dsk, *cow;
	ini_val_t     *val;
	unsigned      drive;
	unsigned long c, h, s;
	unsigned long ofs;
	unsigned long vc, vh, vs;
	int           ro;
	const char *type, *fname, *cowname;

	ini_get_uint16 (sct, "drive", &drive, 0);
	ini_get_string (sct, "type", &type, "auto");

	ini_get_string (sct, "file", &fname, NULL);
	ini_get_string (sct, "cow", &cowname, NULL);

	ini_get_uint32 (sct, "offset", &ofs, 0);

	ini_get_uint32 (sct, "c", &c, 80);
	ini_get_uint32 (sct, "h", &h, 2);
	ini_get_uint32 (sct, "s", &s, 18);

	ini_get_uint32 (sct, "visible_c", &vc, 0);
	ini_get_uint32 (sct, "visible_h", &vh, 0);
	ini_get_uint32 (sct, "visible_s", &vs, 0);

	ini_get_sint16 (sct, "readonly", &ro, 0);

	pce_log_tag (MSG_INF,
		"DISK:", "drive=%u type=%s chs=%u/%u/%u vchs=%u/%u/%u %s file=%s\n",
		drive, type,
		c, h, s, vc, vh, vs,
		(ro ? "ro" : "rw"),
		(fname != NULL) ? fname : "<>"
	);

	if (strcmp (type, "ram") == 0) {
		dsk = dsk_ram_open (fname, c, h, s, ro);
	}
	else if (strcmp (type, "image") == 0) {
		dsk = dsk_img_open (fname, c, h, s, ofs, ro);
	}
	else if (strcmp (type, "dosemu") == 0) {
		dsk = dsk_dosemu_open (fname, ro);
	}
	else if (strcmp (type, "pce") == 0) {
		dsk = dsk_pce_open (fname, ro);
	}
	else if (strcmp (type, "partition") == 0) {
		ini_sct_t     *p;
		unsigned long start;
		unsigned long blk_i, blk_n;
		const char    *fname;

		dsk = dsk_part_open (c, h, s, ro);

		if (dsk != NULL) {
			p = ini_sct_find_sct (sct, "partition");
			while (p != NULL) {
				start = ini_get_lng_def (p, "offset", 0);
				blk_i = ini_get_lng_def (p, "block_start", 0);
				blk_n = ini_get_lng_def (p, "block_count", 0);
				fname = ini_get_str (p, "file");
				ro = ini_get_lng_def (p, "readonly", 0);

				if (fname != NULL) {
					if (dsk_part_add_partition (dsk, fname, start, blk_i, blk_n, ro)) {
						pce_log (MSG_ERR, "*** adding partition failed\n");
					}
				}

				p = ini_sct_find_next (p, "partition");
			}
		}
	}
	else if (strcmp (type, "auto") == 0) {
		dsk = dsk_auto_open (fname, ofs, ro);
	}
	else {
		dsk = NULL;
	}

	if (dsk == NULL) {
		pce_log (MSG_ERR, "*** loading drive 0x%02x failed\n", drive);
		return (NULL);
	}

	if ((dsk->c != c) || (dsk->h != h) || (dsk->s != s)) {
		pce_log_tag (MSG_INF,
			"DISK:", "drive=%u chs=%u/%u/%u vchs=%u/%u/%u\n",
			drive,
			dsk->c, dsk->h, dsk->s,
			dsk->visible_c, dsk->visible_h, dsk->visible_s
		);
	}

	dsk_set_drive (dsk, drive);

	val = ini_sct_find_val (sct, "cow");
	while (val != NULL) {
		const char *cname;

		cname = ini_val_get_str (val);
		if (cname == NULL) {
			dsk_del (dsk);
			return (NULL);
		}

		pce_log_tag (MSG_INF,
			"DISK:", "drive=%u cow=%s rw\n",
			drive, cname
		);

		cow = dsk_cow_new (dsk, cowname);

		if (cow == NULL) {
			pce_log (MSG_ERR,
				"*** loading drive 0x%02x failed (cow)\n",
				drive
			);
			dsk_del (dsk);
			return (NULL);
		}
		else {
			dsk_set_readonly (cow, 0);

			dsk = cow;
		}

		val = ini_val_find_next (val, "cow");
	}

	vc = (vc == 0) ? dsk->c : vc;
	vh = (vh == 0) ? dsk->h : vh;
	vs = (vs == 0) ? dsk->s : vs;

	dsk_set_visible_chs (dsk, vc, vh, vs);

	return (dsk);
}

disks_t *ini_get_disks (ini_sct_t *ini)
{
	ini_sct_t  *sct;
	disk_t     *dsk;
	disks_t    *dsks;

	dsks = dsks_new();

	sct = ini_sct_find_sct (ini, "disk");

	while (sct != NULL) {
		dsk = ini_get_disk (sct);

		if (dsk != NULL) {
			dsks_add_disk (dsks, dsk);
		}
		else {
			pce_log (MSG_ERR, "*** loading drive failed\n");
		}

		sct = ini_sct_find_next (sct, "disk");
	}

	return (dsks);
}
