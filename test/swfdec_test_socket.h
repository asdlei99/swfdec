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

#ifndef _SWFDEC_TEST_SOCKET_H_
#define _SWFDEC_TEST_SOCKET_H_

#include <swfdec/swfdec.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS


typedef struct _SwfdecTestSocket SwfdecTestSocket;
typedef struct _SwfdecTestSocketClass SwfdecTestSocketClass;

#define SWFDEC_TYPE_TEST_SOCKET                    (swfdec_test_socket_get_type())
#define SWFDEC_IS_TEST_SOCKET(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_TEST_SOCKET))
#define SWFDEC_IS_TEST_SOCKET_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_TEST_SOCKET))
#define SWFDEC_TEST_SOCKET(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_TEST_SOCKET, SwfdecTestSocket))
#define SWFDEC_TEST_SOCKET_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_TEST_SOCKET, SwfdecTestSocketClass))
#define SWFDEC_TEST_SOCKET_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_TEST_SOCKET, SwfdecTestSocketClass))

struct _SwfdecTestSocket
{
  SwfdecAsObject	as_object;

  GMainContext *	context;	/* the main context we're running in */
  SoupSocket *		socket;		/* the socket we're servicing */
  gboolean		listening;	/* TRUE if it's a listening socket */
  gboolean		connected;	/* TRUE if the connection is still alive (valid for both types) */

  /* for listening sockets */
  GSList *		connections;	/* connections that still need to be given out */
};

struct _SwfdecTestSocketClass
{
  SwfdecAsObjectClass	as_object_class;
};

GType		swfdec_test_socket_get_type	(void);


G_END_DECLS
#endif
