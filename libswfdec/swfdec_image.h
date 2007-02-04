/* Swfdec
 * Copyright (C) 2003-2006 David Schleef <ds@schleef.org>
 *		 2005-2006 Eric Anholt <eric@anholt.net>
 *		 2006-2007 Benjamin Otte <otte@gnome.org>
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

#ifndef _SWFDEC_IMAGE_H_
#define _SWFDEC_IMAGE_H_

#include <cairo.h>
#include <libswfdec/swfdec_cached.h>
#include <libswfdec/swfdec_decoder.h>

G_BEGIN_DECLS
//typedef struct _SwfdecImage SwfdecImage;
typedef struct _SwfdecImageClass SwfdecImageClass;

#define SWFDEC_TYPE_IMAGE                    (swfdec_image_get_type())
#define SWFDEC_IS_IMAGE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_IMAGE))
#define SWFDEC_IS_IMAGE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_IMAGE))
#define SWFDEC_IMAGE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_IMAGE, SwfdecImage))
#define SWFDEC_IMAGE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_IMAGE, SwfdecImageClass))

typedef enum {
  SWFDEC_IMAGE_TYPE_UNKNOWN = 0,
  SWFDEC_IMAGE_TYPE_JPEG,
  SWFDEC_IMAGE_TYPE_JPEG2,
  SWFDEC_IMAGE_TYPE_JPEG3,
  SWFDEC_IMAGE_TYPE_LOSSLESS,
  SWFDEC_IMAGE_TYPE_LOSSLESS2,
} SwfdecImageType;

struct _SwfdecImage {
  SwfdecCached		cached;

  guint8 *		data;		/* image data in CAIRO_FORMAT_ARGB32 but NOT premultiplied */	
  int			width;
  int			height;
  int			rowstride;
  cairo_surface_t *	surface;	/* surface that owns the data pointer or NULL (doesn't always work) */

  SwfdecImageType	type;
  SwfdecBuffer *	jpegtables;
  SwfdecBuffer *	raw_data;
};

struct _SwfdecImageClass {
  SwfdecCachedClass	cached_class;

};

GType			swfdec_image_get_type		(void);

cairo_surface_t *	swfdec_image_create_surface	(SwfdecImage *		image);
cairo_surface_t *	swfdec_image_create_surface_transformed 
							(SwfdecImage *		image,
							 const SwfdecColorTransform *trans);

int swfdec_image_jpegtables (SwfdecSwfDecoder * s);
int tag_func_define_bits_jpeg (SwfdecSwfDecoder * s);
int tag_func_define_bits_jpeg_2 (SwfdecSwfDecoder * s);
int tag_func_define_bits_jpeg_3 (SwfdecSwfDecoder * s);
int tag_func_define_bits_lossless (SwfdecSwfDecoder * s);
int tag_func_define_bits_lossless_2 (SwfdecSwfDecoder * s);

G_END_DECLS
#endif
