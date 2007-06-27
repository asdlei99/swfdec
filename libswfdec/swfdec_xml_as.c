/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "swfdec_xml.h"
#include "swfdec_as_native_function.h"
#include "swfdec_as_object.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"

static void
swfdec_xml_do_load (SwfdecAsContext *cx, SwfdecAsObject *obj, guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecXml *xml = SWFDEC_XML (obj);
  const char *url;

  url = swfdec_as_value_to_string (cx, &argv[0]);
  swfdec_xml_load (xml, url);
}

void
swfdec_xml_init_context (SwfdecPlayer *player, guint version)
{
  SwfdecAsContext *context;
  SwfdecAsObject *stream, *proto;
  SwfdecAsValue val;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  context = SWFDEC_AS_CONTEXT (player);
  stream = SWFDEC_AS_OBJECT (swfdec_as_object_add_function (context->global, 
      SWFDEC_AS_STR_XML, 0, NULL, 0));
  if (stream == NULL)
    return;
  swfdec_as_native_function_set_construct_type (SWFDEC_AS_NATIVE_FUNCTION (stream), SWFDEC_TYPE_XML);
  proto = swfdec_as_object_new (context);
  /* set the right properties on the NetStream object */
  SWFDEC_AS_VALUE_SET_OBJECT (&val, proto);
  swfdec_as_object_set_variable (stream, SWFDEC_AS_STR_prototype, &val);
  /* set the right properties on the NetStream.prototype object */
  SWFDEC_AS_VALUE_SET_OBJECT (&val, stream);
  swfdec_as_object_set_variable (proto, SWFDEC_AS_STR_constructor, &val);
  swfdec_as_object_add_function (proto, SWFDEC_AS_STR_load, SWFDEC_TYPE_XML,
      swfdec_xml_do_load, 1);
}

