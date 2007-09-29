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

#include "swfdec_rectangle.h"

/**
 * SECTION:SwfdecRectangle
 * @title: SwfdecRectangle
 * @short_description: handling regions on the screen
 *
 * This section describes how regions are handled in Swfdec. Regions are
 * important when tracking which parts of the screen have been invalidated and 
 * need to be repainted. See SwfdecPlayer::invalidate for an example.
 */

/**
 * SwfdecRectangle:
 * @x: x coordinate of top-left point
 * @y: y coordinate of top-left point
 * @width: width of rectangle or 0 for empty
 * @height: height of rectangle or 0 for empty
 *
 * This structure represents a rectangular region. It is identical to the
 * #GdkRectangle structure.
 */

static gpointer
swfdec_rectangle_copy (gpointer src)
{
  return g_memdup (src, sizeof (SwfdecRectangle));
}

GType
swfdec_rectangle_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_boxed_type_register_static ("SwfdecRectangle", 
       swfdec_rectangle_copy, g_free);

  return type;
}

/**
 * swfdec_rectangle_intersect:
 * @dest: the rectangle to take the result or %NULL 
 * @a: first rectangle to intersect
 * @b: second rectangle to intersect
 *
 * Intersects the rectangles @a and @b and puts the result into @dest. It is
 * allowed if @dest is the same as @a or @b.
 *
 * Returns: %TRUE if the intersection is not empty.
 **/
gboolean
swfdec_rectangle_intersect (SwfdecRectangle *dest, const SwfdecRectangle *a,
    const SwfdecRectangle *b)
{
  SwfdecRectangle tmp;

  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  tmp.x = MAX (a->x, b->x);
  tmp.y = MAX (a->y, b->y);
  tmp.width = MIN (a->x + a->width, b->x + b->width) - tmp.x;
  tmp.height = MIN (a->y + a->height, b->y + b->height) - tmp.y;

  if (tmp.width <= 0 && tmp.height <= 0) {
    if (dest)
      dest->x = dest->y = dest->width = dest->height = 0;
    return FALSE;
  }

  if (dest != NULL)
    *dest = tmp;
  return TRUE;
}

/**
 * swfdec_rectangle_union:
 * @dest: destination to take the union
 * @a: first rectangle to union
 * @b: second rectangle to union
 *
 * Computes the smallest rectangle that contains both @a and @b and puts it in 
 * @dest.
 **/
void
swfdec_rectangle_union (SwfdecRectangle *dest, const SwfdecRectangle *a,
    const SwfdecRectangle *b)
{
  g_return_if_fail (dest != NULL);
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);

  dest->x = MIN (a->x, b->x);
  dest->y = MIN (a->y, b->y);
  dest->width = MAX (a->x + a->width, b->x + b->width) - dest->x;
  dest->height = MIN (a->y + a->height, b->y + b->height) - dest->y;
}

/**
 * swfdec_rectangle_contains:
 * @container: the supposedly bigger rectangle
 * @content: the supposedly smaller rectangle
 *
 * Checks if @container contains the whole rectangle @content.
 *
 * Returns: %TRUE if @container contains @content.
 **/
gboolean
swfdec_rectangle_contains (const SwfdecRectangle *container, const SwfdecRectangle *content)
{
  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (content != NULL, FALSE);

  return container->x <= content->x &&
      container->y <= content->y &&
      container->x + container->width >= content->x + content->width &&
      container->y + container->height >= content->y + content->height;
}

