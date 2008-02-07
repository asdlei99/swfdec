/* Swfdec
 * Copyright (C) 2003-2006 David Schleef <ds@schleef.org>
 *		 2005-2006 Eric Anholt <eric@anholt.net>
 *		      2006 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef _SWFDEC_INTERNAL_H_
#define _SWFDEC_INTERNAL_H_

#include <swfdec/swfdec_types.h>
#include <swfdec/swfdec_codec_audio.h>
#include <swfdec/swfdec_codec_video.h>

G_BEGIN_DECLS


/* audio codecs */

SwfdecAudioDecoder *	swfdec_audio_decoder_adpcm_new		(guint			type, 
								 SwfdecAudioFormat	format);
#ifdef HAVE_MAD
SwfdecAudioDecoder *	swfdec_audio_decoder_mad_new		(guint		type, 
								 SwfdecAudioFormat	format);
#endif
#ifdef HAVE_FFMPEG
SwfdecAudioDecoder *	swfdec_audio_decoder_ffmpeg_new		(guint			type, 
								 SwfdecAudioFormat	format);
#endif
#ifdef HAVE_GST
SwfdecAudioDecoder *	swfdec_audio_decoder_gst_new		(guint			type, 
								 SwfdecAudioFormat	format);
char *			swfdec_audio_decoder_gst_missing      	(guint			codec,
								 SwfdecAudioFormat	format);
#else
#define swfdec_audio_decoder_gst_missing(codec) NULL
#endif

/* video codecs */

SwfdecVideoDecoder *	swfdec_video_decoder_screen_new		(guint			format);
SwfdecVideoDecoder *	swfdec_video_decoder_vp6_alpha_new    	(guint			format);
#ifdef HAVE_FFMPEG
SwfdecVideoDecoder *	swfdec_video_decoder_ffmpeg_new		(guint			format);
#endif
#ifdef HAVE_GST
SwfdecVideoDecoder *	swfdec_video_decoder_gst_new		(guint			format);
char *			swfdec_video_decoder_gst_missing      	(guint			codec);
#else
#define swfdec_video_decoder_gst_missing(codec) NULL
#endif

/* AS engine setup code */

void			swfdec_player_preinit_global		(SwfdecAsContext *	context);
void			swfdec_net_stream_init_context		(SwfdecPlayer *		player);
void			swfdec_sprite_movie_init_context	(SwfdecPlayer *		player);
void			swfdec_video_movie_init_context		(SwfdecPlayer *		player);

G_END_DECLS
#endif
