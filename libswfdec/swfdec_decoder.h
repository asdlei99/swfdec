
#ifndef __SWFDEC_DECODER_H__
#define __SWFDEC_DECODER_H__

#include <glib.h>
#include <zlib.h>
#include <libart_lgpl/libart.h>
#include "bits.h"

#include "swfdec_types.h"


#define SWF_COLOR_SCALE_FACTOR		(1/256.0)
#define SWF_TRANS_SCALE_FACTOR		(1/63356.0)
#define SWF_SCALE_FACTOR		(1/20.0)
#define SWF_TEXT_SCALE_FACTOR		(1/1024.0)

enum
{
  SWF_STATE_INIT1 = 0,
  SWF_STATE_INIT2,
  SWF_STATE_PARSETAG,
  SWF_STATE_EOF,
};

struct swfdec_decoder_struct
{
  int version;
  int length;
  int width, height;
  int parse_width, parse_height;
  double rate;
  int n_frames;
  char *buffer;
  int frame_number;

  void *sound_buffer;
  int sound_len;
  int sound_offset;

  int colorspace;
  int no_render;
  int compressed;

  /* End of legacy elements */

  char *input_data;
  int input_data_len;
  z_stream *z;

  int stride;
  int bytespp;
  void (*callback) (void *, int, int, ArtSVPRenderAAStep *, int);
  void (*compose_callback) (void *, int, int, ArtSVPRenderAAStep *, int);

  double scale_factor;
  double transform[6];

  /* where we are in the top-level state engine */
  int state;

  /* where we are in global parsing */
  bits_t parse;

  /* temporary state while parsing */
  bits_t b;

  /* defined objects */
  GList *objects;

  int tag;
  int tag_len;
  int (*func) (SwfdecDecoder * s);

  SwfdecSound *stream_sound_obj;

  /* rendering state */
  unsigned int bg_color;
  ArtIRect irect;
  ArtIRect drawrect;

  SwfdecSprite *main_sprite;
  SwfdecSprite *parse_sprite;

  SwfdecRender *render;

  double flatness;
  int disable_render;

  unsigned char *tmp_scanline;

  unsigned char *jpegtables;
  unsigned int jpegtables_len;

  GList *sound_buffers;
  GList *stream_sound_buffers;

  int pixels_rendered;
};

SwfdecDecoder *swf_init (void);
SwfdecDecoder *swfdec_decoder_new (void);

int swf_addbits (SwfdecDecoder * s, unsigned char *bits, int len);
int swf_parse (SwfdecDecoder * s);
int swf_parse_header (SwfdecDecoder * s);
int swf_parse_tag (SwfdecDecoder * s);
int tag_func_ignore (SwfdecDecoder * s);

unsigned char * swfdec_decoder_render (SwfdecDecoder *s, int frame);
void swfdec_decoder_eof (SwfdecDecoder *s);


#endif

