/* Gnome-Streamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


/*#define GST_DEBUG_ENABLED*/
#include <gstmpegdemux.h>

/* elementfactory information */
static GstElementDetails mpeg_demux_details = {
  "MPEG System Parser",
  "Filter/Parser/System",
  "Demultiplexes MPEG1 and MPEG2 System Streams",
  VERSION,
  "Erik Walthinsen <omega@cse.ogi.edu>\n"
  "Wim Taymans <wim.taymans@chello.be>",
  "(C) 1999",
};

/* MPEG2Demux signals and args */
enum {
  /* FILL ME */
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_BIT_RATE,
  ARG_MPEG2,
  /* FILL ME */
};

GST_PADTEMPLATE_FACTORY (sink_factory,
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_CAPS_NEW (
    "mpeg_demux_sink",
    "video/mpeg",
      "mpegversion",  GST_PROPS_INT_RANGE (1, 2),
      "systemstream", GST_PROPS_BOOLEAN (TRUE)
  )
);

GST_PADTEMPLATE_FACTORY (audio_factory,
  "audio_[1-32]",
  GST_PAD_SRC,
  GST_PAD_SOMETIMES,
  GST_CAPS_NEW (
    "mpeg_demux_audio",
    "audio/mp3",
    NULL
  )
);

GST_PADTEMPLATE_FACTORY (video_mpeg1_factory,
  "video_[0-15]",
  GST_PAD_SRC,
  GST_PAD_SOMETIMES,
  GST_CAPS_NEW (
    "mpeg_demux_video_mpeg1",
    "video/mpeg",
      "mpegversion",  GST_PROPS_INT (1),
      "systemstream",  GST_PROPS_BOOLEAN (FALSE)
  )
);

GST_PADTEMPLATE_FACTORY (video_mpeg2_factory,
  "video_[0-15]",
  GST_PAD_SRC,
  GST_PAD_SOMETIMES,
  GST_CAPS_NEW (
    "mpeg_demux_video_mpeg2",
    "video/mpeg",
      "mpegversion",  GST_PROPS_INT (2),
      "systemstream",  GST_PROPS_BOOLEAN (FALSE)
  )
);


GST_PADTEMPLATE_FACTORY (private1_factory,
  "private_stream_1.[0-7]",
  GST_PAD_SRC,
  GST_PAD_SOMETIMES,
  GST_CAPS_NEW (
    "mpeg_demux_private1",
    "audio/a52",
    NULL
  )
);

GST_PADTEMPLATE_FACTORY (private2_factory,
  "private_stream_2",
  GST_PAD_SRC,
  GST_PAD_SOMETIMES,
  GST_CAPS_NEW (
    "mpeg_demux_private2",
    "unknown/unknown",
    NULL
  )
);

GST_PADTEMPLATE_FACTORY (subtitle_factory,
  "subtitle_stream_[0-15]",
  GST_PAD_SRC,
  GST_PAD_SOMETIMES,
  GST_CAPS_NEW (
    "mpeg_demux_subtitle",
    "video/mpeg",
    NULL
  )
);

static void 	gst_mpeg_demux_class_init	(GstMPEGDemuxClass *klass);
static void 	gst_mpeg_demux_init		(GstMPEGDemux *mpeg_demux);

static gboolean gst_mpeg_demux_parse_packhead 	(GstMPEGParse *mpeg_parse, GstBuffer *buffer);
static gboolean gst_mpeg_demux_parse_syshead 	(GstMPEGParse *mpeg_parse, GstBuffer *buffer);
static gboolean gst_mpeg_demux_parse_packet 	(GstMPEGParse *mpeg_parse, GstBuffer *buffer);
static gboolean gst_mpeg_demux_parse_pes 	(GstMPEGParse *mpeg_parse, GstBuffer *buffer);
static void	gst_mpeg_demux_send_data 	(GstMPEGParse *mpeg_parse, GstData *data);

static GstMPEGParseClass *parent_class = NULL;
/*static guint gst_mpeg_demux_signals[LAST_SIGNAL] = { 0 };*/

GType
mpeg_demux_get_type (void)
{
  static GType mpeg_demux_type = 0;

  if (!mpeg_demux_type) {
    static const GTypeInfo mpeg_demux_info = {
      sizeof(GstMPEGDemuxClass),      
      NULL,
      NULL,
      (GClassInitFunc)gst_mpeg_demux_class_init,
      NULL,
      NULL,
      sizeof(GstMPEGDemux),
      0,
      (GInstanceInitFunc)gst_mpeg_demux_init,
    };
    mpeg_demux_type = g_type_register_static(GST_TYPE_MPEG_PARSE, "GstMPEGDemux", &mpeg_demux_info, 0);
  }
  return mpeg_demux_type;
}

static void
gst_mpeg_demux_class_init (GstMPEGDemuxClass *klass) 
{
  GstMPEGParseClass *mpeg_parse_class;

  parent_class = g_type_class_ref (GST_TYPE_MPEG_PARSE);

  mpeg_parse_class = (GstMPEGParseClass *) klass;

  mpeg_parse_class->parse_packhead	= gst_mpeg_demux_parse_packhead;
  mpeg_parse_class->parse_syshead	= gst_mpeg_demux_parse_syshead;
  mpeg_parse_class->parse_packet	= gst_mpeg_demux_parse_packet;
  mpeg_parse_class->parse_pes		= gst_mpeg_demux_parse_pes;
  mpeg_parse_class->send_data		= gst_mpeg_demux_send_data;
}

static void
gst_mpeg_demux_init (GstMPEGDemux *mpeg_demux)
{
  gint i;
  GstMPEGParse *mpeg_parse = GST_MPEG_PARSE (mpeg_demux);

  gst_element_remove_pad (GST_ELEMENT (mpeg_parse), mpeg_parse->sinkpad);
  mpeg_parse->sinkpad = gst_pad_new_from_template(
		  GST_PADTEMPLATE_GET (sink_factory), "sink");
  gst_element_add_pad (GST_ELEMENT (mpeg_parse), mpeg_parse->sinkpad);
  gst_element_remove_pad (GST_ELEMENT (mpeg_parse), mpeg_parse->srcpad);

  /* zero counters (should be done at RUNNING?)*/
  mpeg_demux->last_pts = 0;

  /* i think everything is already zero'd, but oh well*/
  for (i=0;i<NUM_PRIVATE_1_PADS;i++) {
    mpeg_demux->private_1_pad[i] = NULL;
    mpeg_demux->private_1_offset[i] = 0;
  }
  for (i=0;i<NUM_SUBTITLE_PADS;i++) {
    mpeg_demux->subtitle_pad[i] = NULL;
    mpeg_demux->subtitle_offset[i] = 0;
  }
  mpeg_demux->private_2_pad = NULL;
  mpeg_demux->private_2_offset = 0;
  for (i=0;i<NUM_VIDEO_PADS;i++) {
    mpeg_demux->video_pad[i] = NULL;
    mpeg_demux->video_offset[i] = 0;
  }
  for (i=0;i<NUM_AUDIO_PADS;i++) {
    mpeg_demux->audio_pad[i] = NULL;
    mpeg_demux->audio_offset[i] = 0;
  }

  GST_FLAG_SET (mpeg_demux, GST_ELEMENT_EVENT_AWARE);
}

static void
gst_mpeg_demux_send_data (GstMPEGParse *mpeg_parse, GstData *data)
{
  if (GST_IS_BUFFER (data)) {
    gst_buffer_unref (GST_BUFFER (data));
  }
  else {
    GstEvent *event = GST_EVENT (data);

    gst_pad_event_default (mpeg_parse->sinkpad, event);
  }
}

static gboolean
gst_mpeg_demux_parse_packhead (GstMPEGParse *mpeg_parse, GstBuffer *buffer)
{
  guint8 *buf;

  parent_class->parse_packhead (mpeg_parse, buffer);

  GST_DEBUG (0, "mpeg_demux: in parse_packhead\n");

  buf = GST_BUFFER_DATA (buffer);
  /* do something usefull here */

  return TRUE;
}

static gboolean
gst_mpeg_demux_parse_syshead (GstMPEGParse *mpeg_parse, GstBuffer *buffer)
{
  GstMPEGDemux *mpeg_demux = GST_MPEG_DEMUX (mpeg_parse);
  guint16 header_length;
  guchar *buf;

  GST_DEBUG (0, "mpeg_demux: in parse_syshead\n");

  buf = GST_BUFFER_DATA (buffer);
  buf += 4;

  header_length = GUINT16_FROM_BE (*(guint16 *) buf);
  GST_DEBUG (0, "mpeg_demux: header_length %d\n", header_length);
  buf += 2;

  /* marker:1==1 ! rate_bound:22 | marker:1==1*/
  buf += 3;

  /* audio_bound:6==1 ! fixed:1 | constrained:1*/
  buf += 1;

  /* audio_lock:1 | video_lock:1 | marker:1==1 | video_bound:5 */
  buf += 1;

  /* apacket_rate_restriction:1 | reserved:7==0x7F */
  buf += 1;

  if (!GST_MPEG_PARSE_IS_MPEG2 (mpeg_demux)) {
    gint stream_count = (header_length - 6) / 3;
    gint i, j=0;

    GST_DEBUG (0, "mpeg_demux::parse_syshead: number of streams=%d \n",
	       stream_count);

    for (i = 0; i < stream_count; i++) {
      gint stream_num;
      guint8 stream_id;
      gboolean STD_buffer_bound_scale;
      guint16 STD_buffer_size_bound;
      guint32 buf_byte_size_bound;
      gchar *name = NULL;
      GstPad **outpad = NULL;
      GstPadTemplate *newtemp = NULL;

      stream_id = *buf++;
      if (!(stream_id & 0x80)) {
	GST_DEBUG (0, "mpeg_demux::parse_syshead: error in system header length\n");
	return FALSE;
      }

      /* check marker bits */
      if ((*buf & 0xC0) != 0xC0) {
	GST_DEBUG (0,
		   "mpeg_demux::parse_syshead: expecting placeholder bit values '11' after stream id\n");
	return FALSE;
      }

      STD_buffer_bound_scale = *buf & 0x20;
      STD_buffer_size_bound = (*buf++ & 0x1F) << 8;
      STD_buffer_size_bound |= *buf++;

      if (STD_buffer_bound_scale == 0) {
	buf_byte_size_bound = STD_buffer_size_bound * 128;
      }
      else {
	buf_byte_size_bound = STD_buffer_size_bound * 1024;
      }

      /* private_stream_1 */
      if (stream_id == 0xBD) {
	name = NULL;
	outpad = NULL;
      }
      /* private_stream_2 */
      else if (stream_id == 0xBF) {
	name = g_strdup_printf ("private_stream_2");
	stream_num = 0;
	outpad = &mpeg_demux->private_2_pad;
	newtemp = GST_PADTEMPLATE_GET (private2_factory);
      }
      /* Audio */
      else if ((stream_id >= 0xC0) && (stream_id <= 0xDF)) {
	name = g_strdup_printf ("audio_%02d", stream_id & 0x1F);
	stream_num = stream_id & 0x1F;
	outpad = &mpeg_demux->audio_pad[stream_num];
	newtemp = GST_PADTEMPLATE_GET (audio_factory);
      }
      /* Video */
      else if ((stream_id >= 0xE0) && (stream_id <= 0xEF)) {
	name = g_strdup_printf ("video_%02d", stream_id & 0x0F);
	stream_num = stream_id & 0x0F;
	outpad = &mpeg_demux->video_pad[stream_num];
        if (!GST_MPEG_PARSE_IS_MPEG2 (mpeg_demux)) {
          newtemp = GST_PADTEMPLATE_GET (video_mpeg1_factory);
	}
	else {
	  newtemp = GST_PADTEMPLATE_GET (video_mpeg2_factory);
	}
      }

      GST_DEBUG (0, "mpeg_demux::parse_syshead: stream ID 0x%02X (%s)\n", stream_id, name);
      GST_DEBUG (0, "mpeg_demux::parse_syshead: STD_buffer_bound_scale %d\n", STD_buffer_bound_scale);
      GST_DEBUG (0, "mpeg_demux::parse_syshead: STD_buffer_size_bound %d or %d bytes\n",
		 STD_buffer_size_bound, buf_byte_size_bound);

      /* create the pad and add it to self if it does not yet exist
       * this should trigger the NEW_PAD signal, which should be caught by
       * the app and used to attach to desired streams.
       */
      if (outpad && *outpad == NULL) {
	*outpad = gst_pad_new_from_template (newtemp, name);
	gst_pad_set_caps (*outpad, gst_pad_get_padtemplate_caps (*outpad));
	gst_element_add_pad (GST_ELEMENT (mpeg_demux), (*outpad));
      }
      else {
	/* we won't be needing this. */
	if (name)
	  g_free (name);
      }

      mpeg_demux->STD_buffer_info[j].stream_id = stream_id;
      mpeg_demux->STD_buffer_info[j].STD_buffer_bound_scale =
	STD_buffer_bound_scale;
      mpeg_demux->STD_buffer_info[j].STD_buffer_size_bound =
	STD_buffer_size_bound;

      j++;
    }
  }

  return TRUE;
}

static gboolean
gst_mpeg_demux_parse_packet (GstMPEGParse *mpeg_parse, GstBuffer *buffer)
{
  GstMPEGDemux *mpeg_demux = GST_MPEG_DEMUX (mpeg_parse);
  guint8 id;
  guint16 headerlen;

  guint16 packet_length;
  gboolean STD_buffer_bound_scale;
  guint16 STD_buffer_size_bound;
  guint64 dts;
  guint8 ps_id_code;

  guint16 datalen;
  gulong outoffset = 0;   /* wrong XXX FIXME */

  GstPad **outpad = NULL;
  GstBuffer *outbuf;
  guint8 *buf, *basebuf;

  GST_DEBUG (0,"mpeg_demux::parse_packet: in parse_packet\n");

  basebuf = buf = GST_BUFFER_DATA (buffer);
  id = *(buf+3);
  buf += 4;

  /* start parsing */
  packet_length = GUINT16_FROM_BE (*((guint16 *)buf));

  GST_DEBUG (0,"mpeg_demux: got packet_length %d\n", packet_length);
  headerlen = 2;
  buf += 2;

  /* loop through looping for stuffing bits, STD, PTS, DTS, etc */
  do {
    guint8 bits = *buf++;

    /* stuffing bytes */
    switch (bits & 0xC0) {
      case 0xC0:
        if (bits == 0xff) {
          GST_DEBUG (0,"mpeg_demux::parse_packet: have stuffing byte\n");
        } else {
          GST_DEBUG (0,"mpeg_demux::parse_packet: expected stuffing byte\n");
        }
        headerlen++;
	break;
      case 0x40:
        GST_DEBUG (0,"mpeg_demux::parse_packet: have STD\n");

        STD_buffer_bound_scale =  bits & 0x20;
        STD_buffer_size_bound  = (bits & 0x1F) << 8;
        STD_buffer_size_bound |=  *buf++;

        headerlen += 2;
	break;
      case 0x00:
        switch (bits & 0x30) {
	  case 0x20:
            /* pts:3 ! 1 ! pts:15 ! 1 | pts:15 ! 1 */
            mpeg_demux->last_pts  = (bits & 0x0E)   << 29;
            mpeg_demux->last_pts |=  *buf++         << 22;
            mpeg_demux->last_pts |= (*buf++ & 0xFE) << 14;
            mpeg_demux->last_pts |=  *buf++         <<  7;
            mpeg_demux->last_pts |= (*buf++ & 0xFE) >>  1;

            GST_DEBUG (0,"mpeg_demux::parse_packet: PTS = %llu\n", mpeg_demux->last_pts);
            headerlen += 5;
	    goto done;
	  case 0x30:
            /* pts:3 ! 1 ! pts:15 ! 1 | pts:15 ! 1 */
            mpeg_demux->last_pts  = (bits & 0x0E)   << 29;
            mpeg_demux->last_pts |=  *buf++         << 22;
            mpeg_demux->last_pts |= (*buf++ & 0xFE) << 14;
            mpeg_demux->last_pts |=  *buf++         <<  7;
            mpeg_demux->last_pts |= (*buf++ & 0xFE) >>  1;

            /* sync:4 ! pts:3 ! 1 ! pts:15 ! 1 | pts:15 ! 1 */
            dts  = (*buf++ & 0x0E) << 29;
            dts |=  *buf++         << 22;
            dts |= (*buf++ & 0xFE) << 14;
            dts |=  *buf++         <<  7;
            dts |= (*buf++ & 0xFE) >>  1;

            GST_DEBUG (0,"mpeg_demux::parse_packet: PTS = %llu, DTS = %llu\n", mpeg_demux->last_pts, dts);
            headerlen += 10;
	    goto done;
	  case 0x00:
            GST_DEBUG (0,"mpeg_demux::parse_packet: have no pts/dts\n");
            GST_DEBUG (0,"mpeg_demux::parse_packet: got trailer bits %x\n", (bits & 0x0f));
            if ((bits & 0x0f) != 0xf) {
              GST_DEBUG (0,"mpeg_demux::parse_packet: not a valid packet time sequence\n");
    	      return FALSE;
            }
            headerlen++;
          default:
	    goto done;
	}
      default:
	goto done;
    } 
  } while (1);
  GST_DEBUG (0,"mpeg_demux::parse_packet: done with header loop\n");

done:
  /* calculate the amount of real data in this packet */
  datalen = packet_length - headerlen+2;
  GST_DEBUG (0,"mpeg_demux::parse_packet: headerlen is %d, datalen is %d\n",
        headerlen,datalen);

  /* private_stream_1 */
  if (id == 0xBD) {
    /* first find the track code */
    ps_id_code = *(basebuf + headerlen);
    /* make sure it's valid */
    if ((ps_id_code >= 0x80) && (ps_id_code <= 0x87)) {
      GST_DEBUG (0,"mpeg_demux::parse_packet: 0x%02X: we have a private_stream_1 (AC3) packet, track %d\n",
            id, ps_id_code - 0x80);
      outpad = &mpeg_demux->private_1_pad[ps_id_code - 0x80];
      /* scrap first 4 bytes (so-called "mystery AC3 tag") */
      headerlen += 4;
      datalen -= 4;
    }
  /* private_stream_1 */
  } else if (id == 0xBF) {
    GST_DEBUG (0,"mpeg_demux::parse_packet: 0x%02X: we have a private_stream_2 packet\n", id);
    outpad = &mpeg_demux->private_2_pad;
  /* audio */
  } else if ((id >= 0xC0) && (id <= 0xDF)) {
    GST_DEBUG (0,"mpeg_demux::parse_packet: 0x%02X: we have an audio packet\n", id);
    outpad = &mpeg_demux->audio_pad[id & 0x1F];
    outoffset = mpeg_demux->audio_offset[id & 0x1F];
    mpeg_demux->audio_offset[id & 0x1F] += datalen;
  /* video */
  } else if ((id >= 0xE0) && (id <= 0xEF)) {
    GST_DEBUG (0,"mpeg_demux::parse_packet: 0x%02X: we have a video packet\n", id);
    outpad = &mpeg_demux->video_pad[id & 0x0F];
    outoffset = mpeg_demux->video_offset[id & 0x1F];
    mpeg_demux->video_offset[id & 0x1F] += datalen;
  }

  /* if we don't know what it is, bail */
  if (outpad == NULL) {
    GST_DEBUG (0,"mpeg_demux::parse_packet: unknown packet id 0x%02X !!\n", id);
    /* return total number of bytes */
    return FALSE;
  }

  /* FIXME, this should be done in parse_syshead */
  if ((*outpad) == NULL) {
    GST_DEBUG (0,"mpeg_demux::parse_packet: unexpected packet id 0x%02X!!\n", id);
    /* return total number of bytes */
    return FALSE;
  }

  /* create the buffer and send it off to the Other Side */
  if (GST_PAD_CONNECTED(*outpad) && datalen > 0) {
    /* if this is part of the buffer, create a subbuffer */
    GST_DEBUG (0,"mpeg_demux::parse_packet: creating subbuffer len %d\n", datalen);

    outbuf = gst_buffer_create_sub (buffer, headerlen+4, datalen);

    GST_BUFFER_OFFSET (outbuf) = outoffset;
    GST_BUFFER_TIMESTAMP (outbuf) = (mpeg_demux->last_pts * 100LL)/9LL;
    GST_DEBUG (0,"mpeg_demux::parse_packet: pushing buffer of len %d id %d, ts %lld\n", 
		    datalen, id, GST_BUFFER_TIMESTAMP (outbuf));
    gst_pad_push ((*outpad),outbuf);
  }

  return TRUE;
}

static gboolean
gst_mpeg_demux_parse_pes (GstMPEGParse *mpeg_parse, GstBuffer *buffer)
{
  GstMPEGDemux *mpeg_demux = GST_MPEG_DEMUX (mpeg_parse);
  guint8 id;

  guint16 packet_length;
  guint8 header_data_length = 0;

  guint16 datalen;
  gulong outoffset = 0;   /* wrong XXX FIXME  */
  guint16 headerlen;
  guint8 ps_id_code = 0x80;

  GstPad **outpad = NULL;
  GstBuffer *outbuf;
  GstPadTemplate *newtemp = NULL;
  guint8 *buf, *basebuf;

  GST_DEBUG (0,"mpeg_demux: in parse_pes\n");

  basebuf = buf = GST_BUFFER_DATA (buffer);
  id = *(buf+3);
  buf += 4;

  /* start parsing */
  packet_length = GUINT16_FROM_BE (*((guint16 *)buf));

  GST_DEBUG (0,"mpeg_demux: got packet_length %d\n", packet_length);
  buf += 2;

  /* we don't operate on: program_stream_map, padding_stream, */
  /* private_stream_2, ECM, EMM, or program_stream_directory  */
  if ((id != 0xBC) && (id != 0xBE) && (id != 0xBF) && (id != 0xF0) &&
      (id != 0xF1) && (id != 0xFF)) 
  {
    guchar flags1 = *buf++;
    guchar flags2 = *buf++;

    if ((flags1 & 0xC0) != 0x80) {
      return FALSE;
    }

    header_data_length = *buf++;

    GST_DEBUG (0,"mpeg_demux: header_data_length is %d\n",header_data_length);

    /* check for PTS */
    if ((flags2 & 0x80)) {
    /*if ((flags2 & 0x80) && id == 0xe0) { */
      mpeg_demux->last_pts  = (*buf++ & 0x0E) << 29;
      mpeg_demux->last_pts |=  *buf++         << 22;
      mpeg_demux->last_pts |= (*buf++ & 0xFE) << 14;
      mpeg_demux->last_pts |=  *buf++         <<  7;
      mpeg_demux->last_pts |= (*buf++ & 0xFE) >>  1;
      GST_DEBUG (0, "mpeg_demux::parse_packet: %x PTS = %llu\n", id, (mpeg_demux->last_pts*1000000LL)/90000LL);
    }
    if ((flags2 & 0x40)) {
      GST_DEBUG (0, "mpeg_demux::parse_packet: %x DTS foundu\n", id);
      buf += 5;
    }
    if ((flags2 & 0x20)) {
      GST_DEBUG (0, "mpeg_demux::parse_packet: %x ESCR foundu\n", id);
      buf += 6;
    }
    if ((flags2 & 0x10)) {
      guint32 es_rate;

      es_rate  = (*buf++ & 0x07) << 14;
      es_rate |= (*buf++       ) << 7;
      es_rate |= (*buf++ & 0xFE) >> 1;
      GST_DEBUG (0, "mpeg_demux::parse_packet: %x ES Rate foundu\n", id);
    }
    /* FIXME: lots of PES parsing missing here... */

  }

  /* calculate the amount of real data in this PES packet */
  /* constant is 2 bytes packet_length, 2 bytes of bits, 1 byte header len */
  headerlen = 5 + header_data_length;
  /* constant is 2 bytes of bits, 1 byte header len */
  datalen = packet_length - (3 + header_data_length);
  GST_DEBUG (0,"mpeg_demux: headerlen is %d, datalen is %d\n",
        headerlen, datalen);

  /* private_stream_1 */
  if (id == 0xBD) {
    /* first find the track code */
    ps_id_code = *(basebuf + headerlen + 4);
    /* make sure it's valid */
    if ((ps_id_code >= 0x80) && (ps_id_code <= 0x87)) {
      GST_DEBUG (0,"mpeg_demux: we have a private_stream_1 (AC3) packet, track %d\n",
            ps_id_code - 0x80);
      outpad = &mpeg_demux->private_1_pad[ps_id_code - 0x80];
      /* scrap first 4 bytes (so-called "mystery AC3 tag") */
      headerlen += 4;
      datalen -= 4;
      outoffset = mpeg_demux->private_1_offset[ps_id_code - 0x80];
      mpeg_demux->private_1_offset[ps_id_code - 0x80] += datalen;
    }
    else if ((ps_id_code >= 0x20) && (ps_id_code <= 0x2f)) {
      GST_DEBUG (0,"mpeg_demux: we have a subtitle_stream packet, track %d\n",
            ps_id_code - 0x20);
      outpad = &mpeg_demux->subtitle_pad[ps_id_code - 0x20];
      headerlen += 1;
      datalen -= 1;
      outoffset = mpeg_demux->subtitle_offset[ps_id_code - 0x20];
      mpeg_demux->subtitle_offset[ps_id_code - 0x20] += datalen;
    }
  /* private_stream_1 */
  } else if (id == 0xBF) {
    GST_DEBUG (0,"mpeg_demux: we have a private_stream_2 packet\n");
    outpad = &mpeg_demux->private_2_pad;
    outoffset = mpeg_demux->private_2_offset;
    mpeg_demux->private_2_offset += datalen;
  /* audio */
  } else if ((id >= 0xC0) && (id <= 0xDF)) {
    GST_DEBUG (0,"mpeg_demux: we have an audio packet\n");
    outpad = &mpeg_demux->audio_pad[id - 0xC0];
    outoffset = mpeg_demux->audio_offset[id & 0x1F];
    mpeg_demux->audio_offset[id & 0x1F] += datalen;
  /* video */
  } else if ((id >= 0xE0) && (id <= 0xEF)) {
    GST_DEBUG (0,"mpeg_demux: we have a video packet\n");
    outpad = &mpeg_demux->video_pad[id - 0xE0];
    outoffset = mpeg_demux->video_offset[id & 0x0F];
    mpeg_demux->video_offset[id & 0x0F] += datalen;
  }

  /* if we don't know what it is, bail */
  if (outpad == NULL)
    return TRUE;

  /* create the pad and add it if we don't already have one. 		*/
  /* this should trigger the NEW_PAD signal, which should be caught by 	*/
  /* the app and used to attach to desired streams.			*/
  if ((*outpad) == NULL) {
    gchar *name = NULL;

    /* we have to name the stream approriately */
    if (id == 0xBD) {
      if (ps_id_code >= 0x80 && ps_id_code <= 0x87) {
        name = g_strdup_printf("private_stream_1.%d",ps_id_code - 0x80);
	newtemp = GST_PADTEMPLATE_GET (private1_factory);
      }
      else if (ps_id_code >= 0x20 && ps_id_code <= 0x2f) {
        name = g_strdup_printf("subtitle_stream_%d",ps_id_code - 0x20);
        newtemp = GST_PADTEMPLATE_GET (subtitle_factory);
      }
      else {
        name = g_strdup_printf("unknown_stream_%d",ps_id_code);
      }
    }
    else if (id == 0xBF) {
      name = g_strdup ("private_stream_2");
      newtemp = GST_PADTEMPLATE_GET (private2_factory);
    }
    else if ((id >= 0xC0) && (id <= 0xDF)) {
      name = g_strdup_printf("audio_%02d",id - 0xC0);
      newtemp = GST_PADTEMPLATE_GET (audio_factory);
    }
    else if ((id >= 0xE0) && (id <= 0xEF)) {
      name = g_strdup_printf("video_%02d",id - 0xE0);
      newtemp = GST_PADTEMPLATE_GET (video_mpeg2_factory);
    }
    else {
      name = g_strdup_printf("unknown");
    }

    if (newtemp) {
      /* create the pad and add it to self */
      (*outpad) = gst_pad_new_from_template (newtemp, name);
      gst_pad_set_caps((*outpad), gst_pad_get_padtemplate_caps (*outpad));
      gst_element_add_pad(GST_ELEMENT(mpeg_demux),(*outpad));
    }
    else {
      g_warning ("mpeg_demux: cannot create pad %s, no template for %02x", name, id);
    }
    if (name)
      g_free (name);
  }

  /* create the buffer and send it off to the Other Side */
  if (GST_PAD_CONNECTED(*outpad)) {
    /* if this is part of the buffer, create a subbuffer */
    GST_DEBUG (0,"mpeg_demux: creating subbuffer len %d\n", datalen);

    outbuf = gst_buffer_create_sub (buffer, headerlen+4, datalen);
    GST_BUFFER_OFFSET(outbuf) = outoffset;
    GST_BUFFER_TIMESTAMP(outbuf) = (mpeg_demux->last_pts*100LL)/9LL;

    gst_pad_push((*outpad),outbuf);
  }

  /* return total number of bytes */
  return TRUE;
}

static void
_queue_discontinuous (GstPad *pad, gpointer ign)
{
  /* ugh, GstBuffer cast is wrong here */
  gst_pad_push (pad, (GstBuffer*) gst_event_new (GST_EVENT_DISCONTINUOUS));
}

static void
_queue_eos (GstPad *pad, gpointer ign)
{
  /* ugh, GstBuffer cast is wrong here */
  gst_pad_push (pad, (GstBuffer*) gst_event_new (GST_EVENT_EOS));
}

static void
_forall_pads (GstMPEGDemux *mpeg_demux, GFunc fun, gpointer user_data)
{
  GstPad *pad;
  gint i;

  /* events should be refcnt'd XXX */

  for (i=0;i<NUM_PRIVATE_1_PADS;i++)
    {
      pad = mpeg_demux->private_1_pad[i];
      if (pad && GST_PAD_CONNECTED(pad))
	(*fun) (pad, user_data);
    }
  for (i=0;i<NUM_SUBTITLE_PADS;i++)
    {
      pad = mpeg_demux->subtitle_pad[i];
      if (pad && GST_PAD_CONNECTED(pad))
	(*fun) (pad, user_data);
    }

  pad = mpeg_demux->private_2_pad;
  if (pad && GST_PAD_CONNECTED(pad))
    (*fun) (pad, user_data);
    
  for (i=0;i<NUM_VIDEO_PADS;i++)
    {
      pad = mpeg_demux->video_pad[i];
      if (pad && GST_PAD_CONNECTED(pad))
	(*fun) (pad, user_data);
    }
  for (i=0;i<NUM_AUDIO_PADS;i++)
    {
      pad = mpeg_demux->audio_pad[i];
      if (pad && GST_PAD_CONNECTED(pad))
	(*fun) (pad, user_data);
    }
}

gboolean
gst_mpeg_demux_plugin_init (GModule *module, GstPlugin *plugin)
{
  GstElementFactory *factory;

  /* this filter needs the bytestream package */
  if (!gst_library_load ("gstbytestream")) {
    gst_info ("mpeg_demux:: could not load support library: 'gstbytestream'\n");
    return FALSE;
  }

  /* create an elementfactory for the mpeg_demux element */
  factory = gst_elementfactory_new ("mpegdemux", GST_TYPE_MPEG_DEMUX,
                                    &mpeg_demux_details);
  g_return_val_if_fail (factory != NULL, FALSE);

  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (sink_factory));
  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (audio_factory));
  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (video_mpeg1_factory));
  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (video_mpeg2_factory));
  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (private1_factory));
  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (private2_factory));
  gst_elementfactory_add_padtemplate (factory, GST_PADTEMPLATE_GET (subtitle_factory));

  gst_plugin_add_feature (plugin, GST_PLUGIN_FEATURE (factory));

  return TRUE;
}
