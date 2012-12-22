/*
    Copyright (C) 2002 Robert Lipe, robertlipe@usa.net
    Copyright (C) 2012 Guilhem Bonnefille, guilhem.bonnefille@gmail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111 USA

 */
#include "defs.h"
#include "xmlgeneric.h"

static char* encoded_points = NULL;
static char* instructions = NULL;
static route_head** routehead;
static int* routecount;
static short_handle desc_handle;

#define MYNAME "google"
#define MY_CBUF 4096

#if ! HAVE_LIBEXPAT
static void
google_rd_init(const char* fname)
{
  fatal(MYNAME ": This build excluded Google Maps support because expat was not installed.\n");
}

static void
google_read(void)
{
}
#else

static xg_callback      goog_points, goog_poly_e;
static xg_callback      goog_instr;

static
xg_tag_mapping google_map[] = {
  { goog_points,  cb_cdata,       "/DirectionsResponse/route/overview_polyline/points" },
  { goog_poly_e,  cb_end,         "/DirectionsResponse/route/overview_polyline" },
  { goog_points,  cb_cdata,       "/DirectionsResponse/route/leg/step/polyline/points" },
  { goog_poly_e,  cb_end,         "/DirectionsResponse/route/leg/step" },
  { goog_instr,  cb_cdata,        "/DirectionsResponse/route/leg/step/html_instructions" },
  { NULL, (xg_cb_type)0,              NULL }
};

void goog_points(const char* args, const char** unused)
{
  if (args) {
    if (encoded_points) {
      encoded_points = xstrappend(encoded_points, args);
    } else {
      encoded_points = xstrdup(args);
    }
  }
}

void goog_instr(const char* args, const char** unused)
{
  if (args) {
    if (instructions) {
      instructions = xstrappend(instructions, args);
    } else {
      instructions = xstrdup(args);
    }
  }
}

static int goog_segroute = 0;
static int goog_step = 0;

static long decode_goog64(char** str)
{
  long result = 0;
  unsigned char c = 0;
  unsigned char shift = 0;

  if (!(**str)) {
    return 0;
  }

  do {
    c = (unsigned char)(*(*str)++)-'?';
    result |= (c & 31)<<shift;
    shift += 5;
  } while (c & ~31);

  if (result & 1) {
    result = ~result;
  }
  return result/2;
}

void goog_poly_e(const char* args, const char** unused)
{
  long lat = 0;
  long lon = 0;
  char* str = encoded_points;

  routehead[goog_segroute] = route_head_alloc();
  if (strcmp (args, "overview_polyline") == 0) {
    routehead[goog_segroute]->rte_name = (char*) xstrdup("overview");
    routehead[goog_segroute]->rte_desc = (char*) xstrdup("Overview");
  } else {
	goog_step++;
    routehead[goog_segroute]->rte_name = (char*) xmalloc(8);
    sprintf(routehead[goog_segroute]->rte_name, "step%03d", goog_step);
    if (instructions == NULL) {
      routehead[goog_segroute]->rte_desc = (char*) xmalloc(9);
      sprintf(routehead[goog_segroute]->rte_desc, "Step %d", goog_step);
    } else {
      utf_string utf;
      utf.is_html = 1;
      utf.utfstring = instructions;
      routehead[goog_segroute]->rte_desc = strip_html(&utf);
      xfree(instructions);
      instructions = NULL;
	}
  }
  route_add_head(routehead[goog_segroute]);

  while (str && *str) {
    lat += decode_goog64(&str);
    lon += decode_goog64(&str);

    {
      waypoint* wpt_tmp = waypt_new();
      wpt_tmp->latitude = lat / 100000.0;
      wpt_tmp->longitude = lon / 100000.0;
      /* FIXME no need for name
      wpt_tmp->shortname = (char*) xmalloc(7);
      sprintf(wpt_tmp->shortname, "\\%5.5x", serial++);
      */
      route_add_wpt(routehead[goog_segroute], wpt_tmp);
    }
  }

  if (encoded_points) {
    xfree(encoded_points);
    encoded_points = NULL;
  }
  if (instructions) {
    xfree(instructions);
    instructions = NULL;
  }
}

static void
google_rd_init(const char* fname)
{
  desc_handle = mkshort_new_handle();
  setshort_length(desc_handle, 12);

  xml_init(fname, google_map, "ISO-8859-1");
}

static void
google_read(void)
{
  routehead = (route_head**)xmalloc(sizeof(route_head*));
  goog_segroute = 0;
  xml_read();
  xfree(routehead);
  xfree(routecount);

  if (encoded_points) {
    xfree(encoded_points);
    encoded_points = NULL;
  }
  if (instructions) {
    xfree(instructions);
    instructions = NULL;
  }
}
#endif

static void
google_rd_deinit(void)
{
  xml_deinit();
  mkshort_del_handle(&desc_handle);
}

ff_vecs_t google_vecs = {
  ff_type_file,
  { ff_cap_none, ff_cap_read, ff_cap_none},
  google_rd_init,
  NULL,
  google_rd_deinit,
  NULL,
  google_read,
  NULL,
  NULL,
  NULL,
  CET_CHARSET_UTF8, 1	/* CET-REVIEW */
};
