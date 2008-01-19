/* Swfdec
 * Copyright (C) 2006-2007 Benjamin Otte <otte@gnome.org>
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

#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <liboil/liboil.h>

#include "swfdec_player_internal.h"
#include "swfdec_as_frame_internal.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_audio_internal.h"
#include "swfdec_button_movie.h" /* for mouse cursor */
#include "swfdec_cache.h"
#include "swfdec_debug.h"
#include "swfdec_enums.h"
#include "swfdec_event.h"
#include "swfdec_flash_security.h"
#include "swfdec_initialize.h"
#include "swfdec_internal.h"
#include "swfdec_loader_internal.h"
#include "swfdec_marshal.h"
#include "swfdec_movie.h"
#include "swfdec_resource.h"
#include "swfdec_resource_request.h"
#include "swfdec_script_internal.h"
#include "swfdec_sprite_movie.h"
#include "swfdec_utils.h"

/*** gtk-doc ***/

/**
 * SECTION:SwfdecPlayer
 * @title: SwfdecPlayer
 * @short_description: main playback object
 *
 * A #SwfdecPlayer is the main object used for playing back Flash files through
 * Swfdec.
 *
 * A player interacts with the outside world in a multitude of ways. The most 
 * important ones are described below.
 *
 * Input is handled via the 
 * <link linkend="swfdec-SwfdecLoader">SwfdecLoader</link> class. A 
 * #SwfdecLoader is set on a new player using swfdec_player_set_loader().
 *
 * When the loader has provided enough data, you can start playing the file.
 * This is done in steps by calling swfdec_player_advance() - preferrably as 
 * often as swfdec_player_get_next_event() indicates. Or you can provide user input
 * to the player by calling for example swfdec_player_handle_mouse().
 *
 * You can use swfdec_player_render() to draw the current state of the player.
 * After that, connect to the SwfdecPlayer::invalidate signal to be notified of
 * changes.
 *
 * Audio output is handled via the 
 * <link linkend="swfdec-SwfdecAudio">SwfdecAudio</link> class. One 
 * #SwfdecAudio object is created for every output using the 
 * SwfdecPlayer::audio-added signal.
 */

/**
 * SwfdecPlayer:
 *
 * This is the base object used for playing Flash files.
 */

/**
 * SECTION:Enumerations
 * @title: Enumerations
 * @short_description: enumerations used in Swfdec
 *
 * This file lists all of the enumerations used in various parts of Swfdec.
 */

/**
 * SwfdecMouseCursor:
 * @SWFDEC_MOUSE_CURSOR_NORMAL: a normal mouse cursor
 * @SWFDEC_MOUSE_CURSOR_NONE: no mouse image
 * @SWFDEC_MOUSE_CURSOR_TEXT: a mouse cursor suitable for text editing
 * @SWFDEC_MOUSE_CURSOR_CLICK: a mouse cursor for clicking a hyperlink or a 
 *                             button
 *
 * This enumeration describes the possible types for the SwfdecPlayer::mouse-cursor
 * property.
 */

/**
 * SwfdecAlignment:
 * @SWFDEC_ALIGNMENT_TOP_LEFT: top left
 * @SWFDEC_ALIGNMENT_TOP: top
 * @SWFDEC_ALIGNMENT_TOP_RIGHT: top right
 * @SWFDEC_ALIGNMENT_LEFT: left
 * @SWFDEC_ALIGNMENT_CENTER: center
 * @SWFDEC_ALIGNMENT_RIGHT: right
 * @SWFDEC_ALIGNMENT_BOTTOM_LEFT: left
 * @SWFDEC_ALIGNMENT_BOTTOM: bottom
 * @SWFDEC_ALIGNMENT_BOTTOM_RIGHT: bottom right
 *
 * These are the possible values for the alignment of an unscaled movie.
 */

/**
 * SwfdecScaleMode:
 * @SWFDEC_SCALE_SHOW_ALL: Show the whole content as large as possible
 * @SWFDEC_SCALE_NO_BORDER: Fill the whole area, possibly cropping parts
 * @SWFDEC_SCALE_EXACT_FIT: Fill the whole area, don't keep aspect ratio
 * @SWFDEC_SCALE_NONE: Do not scale the movie at all
 *
 * Describes how the movie should be scaled if the given size doesn't equal the
 * movie's size.
 */

/**
 * SwfdecKey:
 * @SWFDEC_KEY_BACKSPACE: the backspace key
 * @SWFDEC_KEY_TAB: the tab key
 * @SWFDEC_KEY_CLEAR: the clear key
 * @SWFDEC_KEY_ENTER: the enter key
 * @SWFDEC_KEY_SHIFT: the shift key
 * @SWFDEC_KEY_CONTROL: the control key
 * @SWFDEC_KEY_ALT: the alt key
 * @SWFDEC_KEY_CAPS_LOCK: the caps lock key
 * @SWFDEC_KEY_ESCAPE: the escape key
 * @SWFDEC_KEY_SPACE: the space key
 * @SWFDEC_KEY_PAGE_UP: the page up key
 * @SWFDEC_KEY_PAGE_DOWN: the page down key
 * @SWFDEC_KEY_END: the end key
 * @SWFDEC_KEY_HOME: the home key
 * @SWFDEC_KEY_LEFT: the left key
 * @SWFDEC_KEY_UP: the up key
 * @SWFDEC_KEY_RIGHT: the right key
 * @SWFDEC_KEY_DOWN: the down key
 * @SWFDEC_KEY_INSERT: the insert key
 * @SWFDEC_KEY_DELETE: the delete key
 * @SWFDEC_KEY_HELP: the help key
 * @SWFDEC_KEY_0: the 0 key
 * @SWFDEC_KEY_1: the 1 key
 * @SWFDEC_KEY_2: the 2 key
 * @SWFDEC_KEY_3: the 3 key
 * @SWFDEC_KEY_4: the 4 key
 * @SWFDEC_KEY_5: the 5 key
 * @SWFDEC_KEY_6: the 6 key
 * @SWFDEC_KEY_7: the 7 key
 * @SWFDEC_KEY_8: the 8 key
 * @SWFDEC_KEY_9: the 9 key
 * @SWFDEC_KEY_A: the ! key
 * @SWFDEC_KEY_B: the B key
 * @SWFDEC_KEY_C: the C key
 * @SWFDEC_KEY_D: the D key
 * @SWFDEC_KEY_E: the E key
 * @SWFDEC_KEY_F: the F key
 * @SWFDEC_KEY_G: the G key
 * @SWFDEC_KEY_H: the H key
 * @SWFDEC_KEY_I: the I key
 * @SWFDEC_KEY_J: the J key
 * @SWFDEC_KEY_K: the K key
 * @SWFDEC_KEY_L: the L key
 * @SWFDEC_KEY_M: the M key
 * @SWFDEC_KEY_N: the N key
 * @SWFDEC_KEY_O: the O key
 * @SWFDEC_KEY_P: the P key
 * @SWFDEC_KEY_Q: the Q key
 * @SWFDEC_KEY_R: the R key
 * @SWFDEC_KEY_S: the S key
 * @SWFDEC_KEY_T: the T key
 * @SWFDEC_KEY_U: the U key
 * @SWFDEC_KEY_V: the V key
 * @SWFDEC_KEY_W: the W key
 * @SWFDEC_KEY_X: the X key
 * @SWFDEC_KEY_Y: the Y key
 * @SWFDEC_KEY_Z: the Z key
 * @SWFDEC_KEY_NUMPAD_0: the 0 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_1: the 1 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_2: the 2 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_3: the 3 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_4: the 4 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_5: the 5 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_6: the 6 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_7: the 7 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_8: the 8 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_9: the 9 key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_MULTIPLY: the multiply key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_ADD: the add key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_SUBTRACT: the subtract key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_DECIMAL: the decimal key on the numeric keypad
 * @SWFDEC_KEY_NUMPAD_DIVIDE: the divide key on the numeric keypad
 * @SWFDEC_KEY_F1: the F1 key
 * @SWFDEC_KEY_F2: the F2 key
 * @SWFDEC_KEY_F3: the F3 key
 * @SWFDEC_KEY_F4: the F4 key
 * @SWFDEC_KEY_F5: the F5 key
 * @SWFDEC_KEY_F6: the F6 key
 * @SWFDEC_KEY_F7: the F7 key
 * @SWFDEC_KEY_F8: the F8 key
 * @SWFDEC_KEY_F9: the F9 key
 * @SWFDEC_KEY_F10: the F10 key
 * @SWFDEC_KEY_F11: the F11 key
 * @SWFDEC_KEY_F12: the F12 key
 * @SWFDEC_KEY_F13: the F13 key
 * @SWFDEC_KEY_F14: the F14 key
 * @SWFDEC_KEY_F15: the F15 key
 * @SWFDEC_KEY_NUM_LOCK: the num lock key
 * @SWFDEC_KEY_SEMICOLON: the semicolon key (on English keyboards)
 * @SWFDEC_KEY_EQUAL: the equal key (on English keyboards)
 * @SWFDEC_KEY_MINUS: the minus key (on English keyboards)
 * @SWFDEC_KEY_SLASH: the slash key (on English keyboards)
 * @SWFDEC_KEY_GRAVE: the grave key (on English keyboards)
 * @SWFDEC_KEY_LEFT_BRACKET: the left bracket key (on English keyboards)
 * @SWFDEC_KEY_BACKSLASH: the backslash key (on English keyboards)
 * @SWFDEC_KEY_RIGHT_BRACKET: the right bracket key (on English keyboards)
 * @SWFDEC_KEY_APOSTROPHE: the apostrophe key (on English keyboards)
 *
 * Lists all known key codes in Swfdec and their meanings on an English 
 * keyboard.
 */

/*** Timeouts ***/

static SwfdecTick
swfdec_player_get_next_event_time (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv = player->priv;

  if (priv->timeouts) {
    return ((SwfdecTimeout *) priv->timeouts->data)->timestamp - priv->time;
  } else {
    return G_MAXUINT64;
  }
}

/**
 * swfdec_player_add_timeout:
 * @player: a #SwfdecPlayer
 * @timeout: timeout to add
 *
 * Adds a timeout to @player. The timeout will be removed automatically when 
 * triggered, so you need to use swfdec_player_add_timeout() to add it again. 
 * The #SwfdecTimeout struct and callback does not use a data callback pointer. 
 * It's suggested that you use the struct as part of your own bigger struct 
 * and get it back like this:
 * <programlisting>
 * typedef struct {
 *   // ...
 *   SwfdecTimeout timeout;
 * } MyStruct;
 *
 * static void
 * my_struct_timeout_callback (SwfdecTimeout *timeout)
 * {
 *   MyStruct *mystruct = (MyStruct *) ((void *) timeout - G_STRUCT_OFFSET (MyStruct, timeout));
 *
 *   // do stuff
 * }
 * </programlisting>
 **/
void
swfdec_player_add_timeout (SwfdecPlayer *player, SwfdecTimeout *timeout)
{
  SwfdecPlayerPrivate *priv;
  GList *walk;
  SwfdecTick next_tick;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (timeout != NULL);
  g_return_if_fail (timeout->timestamp >= player->priv->time);
  g_return_if_fail (timeout->callback != NULL);

  priv = player->priv;
  SWFDEC_LOG ("adding timeout %p in %"G_GUINT64_FORMAT" msecs", timeout, 
      SWFDEC_TICKS_TO_MSECS (timeout->timestamp - priv->time));
  next_tick = swfdec_player_get_next_event_time (player);
  /* the order is important, on events with the same time, we make sure the new one is last */
  for (walk = priv->timeouts; walk; walk = walk->next) {
    SwfdecTimeout *cur = walk->data;
    if (cur->timestamp > timeout->timestamp)
      break;
  }
  priv->timeouts = g_list_insert_before (priv->timeouts, walk, timeout);
  if (next_tick != swfdec_player_get_next_event_time (player))
    g_object_notify (G_OBJECT (player), "next-event");
}

/**
 * swfdec_player_remove_timeout:
 * @player: a #SwfdecPlayer
 * @timeout: a timeout that should be removed
 *
 * Removes the @timeout from the list of scheduled timeouts. The timeout must 
 * have been added with swfdec_player_add_timeout() before.
 **/
void
swfdec_player_remove_timeout (SwfdecPlayer *player, SwfdecTimeout *timeout)
{
  SwfdecPlayerPrivate *priv;
  SwfdecTick next_tick;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (timeout != NULL);
  g_return_if_fail (timeout->timestamp >= player->priv->time);
  g_return_if_fail (timeout->callback != NULL);

  SWFDEC_LOG ("removing timeout %p", timeout);
  priv = player->priv;
  next_tick = swfdec_player_get_next_event_time (player);
  priv->timeouts = g_list_remove (priv->timeouts, timeout);
  if (next_tick != swfdec_player_get_next_event_time (player))
    g_object_notify (G_OBJECT (player), "next-event");
}

/*** Actions ***/

typedef struct {
  SwfdecMovie *		movie;		/* the movie to trigger the action on */
  SwfdecScript *	script;		/* script to execute or NULL to trigger action */
  SwfdecEventType	event;		/* the action to trigger */
} SwfdecPlayerAction;

typedef struct {
  gpointer             object;
  SwfdecActionFunc     func;
  gpointer             data;
} SwfdecPlayerExternalAction;

static void
swfdec_player_compress_actions (SwfdecRingBuffer *buffer)
{
  SwfdecPlayerAction *action, tmp;
  guint i = 0;

  for (i = swfdec_ring_buffer_get_n_elements (buffer); i > 0; i--) {
    action = swfdec_ring_buffer_pop (buffer);
    g_assert (action);
    if (action->movie == NULL)
      continue;
    tmp = *action;
    action = swfdec_ring_buffer_push (buffer);
    *action = tmp;
  }
  SWFDEC_INFO ("compresed action queue to %u elements", 
      swfdec_ring_buffer_get_n_elements (buffer));
  for (i = 0; i < swfdec_ring_buffer_get_n_elements (buffer); i++) {
    action = swfdec_ring_buffer_peek_nth (buffer, i);
    g_assert (action->movie != NULL);
  }
}

static void
swfdec_player_do_add_action (SwfdecPlayer *player, guint importance, SwfdecPlayerAction *act)
{
  SwfdecPlayerPrivate *priv = player->priv;
  SwfdecPlayerAction *action = swfdec_ring_buffer_push (priv->actions[importance]);
  if (action == NULL) {
    /* try to get rid of freed actions */
    swfdec_player_compress_actions (priv->actions[importance]);
    action = swfdec_ring_buffer_push (priv->actions[importance]);
    if (action == NULL) {
      if (swfdec_ring_buffer_get_size (priv->actions[importance]) == 256) {
	SWFDEC_WARNING ("256 levels of recursion were exceeded in one action list.");
      }
      swfdec_ring_buffer_set_size (priv->actions[importance],
	  swfdec_ring_buffer_get_size (priv->actions[importance]) + 16);
      action = swfdec_ring_buffer_push (priv->actions[importance]);
      g_assert (action);
    }
  }
  *action = *act;
}

/**
 * swfdec_player_add_event:
 * @player: a #SwfdecPlayer
 * @movie: the movie on which to trigger the event
 * @type: type of the event
 * @importance: importance of the event
 *
 * Adds an action to the @player. Actions are used by Flash player to solve
 * reentrancy issues. Instead of calling back into the Actionscript engine,
 * an action is queued for later execution. So if you're writing code that
 * is calling Actionscript code, you want to do this by using actions.
 **/
void
swfdec_player_add_action (SwfdecPlayer *player, SwfdecMovie *movie, SwfdecEventType type,
    guint importance)
{
  SwfdecPlayerAction action = { movie, NULL, type };

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (SWFDEC_IS_MOVIE (movie));
  g_return_if_fail (importance < SWFDEC_PLAYER_N_ACTION_QUEUES);

  SWFDEC_LOG ("adding action %s %u", movie->name, type);
  swfdec_player_do_add_action (player, importance, &action);
}

void
swfdec_player_add_action_script	(SwfdecPlayer *player, SwfdecMovie *movie,
    SwfdecScript *script, guint importance)
{
  SwfdecPlayerAction action = { movie, script, 0 };

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (SWFDEC_IS_MOVIE (movie));
  g_return_if_fail (script != NULL);
  g_return_if_fail (importance < SWFDEC_PLAYER_N_ACTION_QUEUES);

  SWFDEC_LOG ("adding action script %s %s", movie->name, script->name);
  swfdec_player_do_add_action (player, importance, &action);
}

/**
 * swfdec_player_remove_all_actions:
 * @player: a #SwfdecPlayer
 * @movie: movie pointer identifying the actions to be removed
 *
 * Removes all actions associated with @movie that have not yet been executed.
 * See swfdec_player_add_action() for details about actions.
 **/
void
swfdec_player_remove_all_actions (SwfdecPlayer *player, SwfdecMovie *movie)
{
  SwfdecPlayerAction *action;
  SwfdecPlayerPrivate *priv;
  guint i, j;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (SWFDEC_IS_MOVIE (movie));

  priv = player->priv;
  for (i = 0; i < SWFDEC_PLAYER_N_ACTION_QUEUES; i++) {
    for (j = 0; j < swfdec_ring_buffer_get_n_elements (priv->actions[i]); j++) {
      action = swfdec_ring_buffer_peek_nth (priv->actions[i], j);

      if (action->movie == movie) {
	SWFDEC_LOG ("removing action %p %u", 
	    action->movie, action->event);
	action->movie = NULL;
      }
    }
  }
}

static gboolean
swfdec_player_do_action (SwfdecPlayer *player)
{
  SwfdecPlayerAction *action;
  SwfdecPlayerPrivate *priv;
  guint i;

  priv = player->priv;
  for (i = 0; i < SWFDEC_PLAYER_N_ACTION_QUEUES; i++) {
    do {
      action = swfdec_ring_buffer_pop (priv->actions[i]);
      if (action == NULL)
	break;
    } while (action->movie == NULL); /* skip removed actions */
    if (action) {
      if (action->script) {
	swfdec_as_object_run_with_security (SWFDEC_AS_OBJECT (action->movie), 
	  action->script, SWFDEC_SECURITY (action->movie->resource));
      } else {
	swfdec_movie_execute (action->movie, action->event);
      }
      return TRUE;
    }
  }

  return FALSE;
}

static void
swfdec_player_perform_external_actions (SwfdecPlayer *player)
{
  SwfdecPlayerExternalAction *action;
  SwfdecPlayerPrivate *priv = player->priv;
  guint i;

  /* remove timeout if it exists - do this before executing stuff below */
  if (priv->external_timeout.callback) {
    swfdec_player_remove_timeout (player, &priv->external_timeout);
    priv->external_timeout.callback = NULL;
  }

  /* we need to query the number of current actions so newly added ones aren't
   * executed in here */
  for (i = swfdec_ring_buffer_get_n_elements (priv->external_actions); i > 0; i--) {
    action = swfdec_ring_buffer_pop (priv->external_actions);
    g_assert (action != NULL);
    /* skip removed actions */
    if (action->object == NULL) 
      continue;
    action->func (action->object, action->data);
    swfdec_player_perform_actions (player);
  }
}

static void
swfdec_player_trigger_external_actions (SwfdecTimeout *advance)
{
  SwfdecPlayerPrivate *priv = (SwfdecPlayerPrivate *) ((void *) ((guint8 *) advance - G_STRUCT_OFFSET (SwfdecPlayerPrivate, external_timeout)));

  priv->external_timeout.callback = NULL;
  swfdec_player_perform_external_actions (priv->player);
}

void
swfdec_player_add_external_action (SwfdecPlayer *player, gpointer object, 
    SwfdecActionFunc action_func, gpointer action_data)
{
  SwfdecPlayerExternalAction *action;
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (object != NULL);
  g_return_if_fail (action_func != NULL);

  SWFDEC_LOG ("adding external action %p %p %p", object, action_func, action_data);
  priv = player->priv;
  action = swfdec_ring_buffer_push (priv->external_actions);
  if (action == NULL) {
    /* FIXME: limit number of actions to not get inf loops due to scripts? */
    swfdec_ring_buffer_set_size (priv->external_actions,
	swfdec_ring_buffer_get_size (priv->external_actions) + 16);
    action = swfdec_ring_buffer_push (priv->external_actions);
    g_assert (action);
  }
  action->object = object;
  action->func = action_func;
  action->data = action_data;
  if (!priv->external_timeout.callback) {
    /* trigger execution immediately.
     * But if initialized, keep at least 100ms from when the last external 
     * timeout triggered. This is a crude method to get around infinite loops
     * when script actions executed by external actions trigger another external
     * action that would execute instantly.
     */
    if (priv->initialized) {
      priv->external_timeout.timestamp = MAX (priv->time,
	  priv->external_timeout.timestamp + SWFDEC_MSECS_TO_TICKS (100));
    } else {
      priv->external_timeout.timestamp = priv->time;
    }
    priv->external_timeout.callback = swfdec_player_trigger_external_actions;
    swfdec_player_add_timeout (player, &priv->external_timeout);
  }
}

void
swfdec_player_remove_all_external_actions (SwfdecPlayer *player, gpointer object)
{
  SwfdecPlayerExternalAction *action;
  SwfdecPlayerPrivate *priv;
  guint i;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (object != NULL);

  priv = player->priv;
  for (i = 0; i < swfdec_ring_buffer_get_n_elements (priv->external_actions); i++) {
    action = swfdec_ring_buffer_peek_nth (priv->external_actions, i);

    if (action->object == object) {
      SWFDEC_LOG ("removing external action %p %p %p", 
	  action->object, action->func, action->data);
      action->object = NULL;
    }
  }
}

/*** SwfdecPlayer ***/

enum {
  INVALIDATE,
  ADVANCE,
  HANDLE_KEY,
  HANDLE_MOUSE,
  AUDIO_ADDED,
  AUDIO_REMOVED,
  LAUNCH,
  FSCOMMAND,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

enum {
  PROP_0,
  PROP_CACHE_SIZE,
  PROP_INITIALIZED,
  PROP_DEFAULT_WIDTH,
  PROP_DEFAULT_HEIGHT,
  PROP_RATE,
  PROP_MOUSE_CURSOR,
  PROP_NEXT_EVENT,
  PROP_BACKGROUND_COLOR,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ALIGNMENT,
  PROP_SCALE,
  PROP_SCRIPTING,
  PROP_SYSTEM,
  PROP_MAX_RUNTIME,
  PROP_SOCKET_TYPE
};

G_DEFINE_TYPE (SwfdecPlayer, swfdec_player, SWFDEC_TYPE_AS_CONTEXT)

void
swfdec_player_remove_movie (SwfdecPlayer *player, SwfdecMovie *movie)
{
  SwfdecPlayerPrivate *priv = player->priv;

  swfdec_movie_remove (movie);
  priv->movies = g_list_remove (priv->movies, movie);
}

static guint
swfdec_player_alignment_to_flags (SwfdecAlignment alignment)
{
  static const guint align_flags[9] = { 
    SWFDEC_ALIGN_FLAG_TOP | SWFDEC_ALIGN_FLAG_LEFT,
    SWFDEC_ALIGN_FLAG_TOP,
    SWFDEC_ALIGN_FLAG_TOP | SWFDEC_ALIGN_FLAG_RIGHT,
    SWFDEC_ALIGN_FLAG_LEFT,
    0,
    SWFDEC_ALIGN_FLAG_RIGHT,
    SWFDEC_ALIGN_FLAG_BOTTOM | SWFDEC_ALIGN_FLAG_LEFT,
    SWFDEC_ALIGN_FLAG_BOTTOM,
    SWFDEC_ALIGN_FLAG_BOTTOM | SWFDEC_ALIGN_FLAG_RIGHT
  };
  return align_flags[alignment];
}

static SwfdecAlignment
swfdec_player_alignment_from_flags (guint flags)
{
  if (flags & SWFDEC_ALIGN_FLAG_TOP) {
    if (flags & SWFDEC_ALIGN_FLAG_LEFT) {
      return SWFDEC_ALIGNMENT_TOP_LEFT;
    } else if (flags & SWFDEC_ALIGN_FLAG_RIGHT) {
      return SWFDEC_ALIGNMENT_TOP_RIGHT;
    } else {
      return SWFDEC_ALIGNMENT_TOP;
    }
  } else if (flags & SWFDEC_ALIGN_FLAG_BOTTOM) {
    if (flags & SWFDEC_ALIGN_FLAG_LEFT) {
      return SWFDEC_ALIGNMENT_BOTTOM_LEFT;
    } else if (flags & SWFDEC_ALIGN_FLAG_RIGHT) {
      return SWFDEC_ALIGNMENT_BOTTOM_RIGHT;
    } else {
      return SWFDEC_ALIGNMENT_BOTTOM;
    }
  } else {
    if (flags & SWFDEC_ALIGN_FLAG_LEFT) {
      return SWFDEC_ALIGNMENT_LEFT;
    } else if (flags & SWFDEC_ALIGN_FLAG_RIGHT) {
      return SWFDEC_ALIGNMENT_RIGHT;
    } else {
      return SWFDEC_ALIGNMENT_CENTER;
    }
  }
}

static void
swfdec_player_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (object);
  SwfdecPlayerPrivate *priv = player->priv;
  
  switch (param_id) {
    case PROP_BACKGROUND_COLOR:
      g_value_set_uint (value, swfdec_player_get_background_color (player));
      break;
    case PROP_CACHE_SIZE:
      g_value_set_uint (value, priv->cache->max_size);
      break;
    case PROP_INITIALIZED:
      g_value_set_boolean (value, swfdec_player_is_initialized (player));
      break;
    case PROP_DEFAULT_WIDTH:
      g_value_set_uint (value, priv->width);
      break;
    case PROP_DEFAULT_HEIGHT:
      g_value_set_uint (value, priv->height);
      break;
    case PROP_RATE:
      g_value_set_double (value, priv->rate / 256.0);
      break;
    case PROP_MOUSE_CURSOR:
      g_value_set_enum (value, priv->mouse_cursor);
      break;
    case PROP_NEXT_EVENT:
      g_value_set_uint (value, swfdec_player_get_next_event (player));
      break;
    case PROP_WIDTH:
      g_value_set_int (value, priv->stage_width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, priv->stage_height);
      break;
    case PROP_ALIGNMENT:
      g_value_set_enum (value, swfdec_player_alignment_from_flags (priv->align_flags));
      break;
    case PROP_SCALE:
      g_value_set_enum (value, priv->scale_mode);
      break;
    case PROP_SCRIPTING:
      g_value_set_object (value, priv->scripting);
      break;
    case PROP_SYSTEM:
      g_value_set_object (value, priv->system);
      break;
    case PROP_MAX_RUNTIME:
      g_value_set_ulong (value, priv->max_runtime);
      break;
    case PROP_SOCKET_TYPE:
      g_value_set_gtype (value, priv->socket_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

void
swfdec_player_update_scale (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv = player->priv;
  int width, height;
  double scale_x, scale_y;

  priv->stage.width = priv->stage_width >= 0 ? priv->stage_width : (int) priv->width;
  priv->stage.height = priv->stage_height >= 0 ? priv->stage_height : (int) priv->height;
  if (priv->stage.height == 0 || priv->stage.width == 0) {
    priv->scale_x = 1.0;
    priv->scale_y = 1.0;
    priv->offset_x = 0;
    priv->offset_y = 0;
    return;
  }
  if (priv->width == 0 || priv->height == 0) {
    scale_x = 1.0;
    scale_y = 1.0;
  } else {
    scale_x = (double) priv->stage.width / priv->width;
    scale_y = (double) priv->stage.height / priv->height;
  }
  switch (priv->scale_mode) {
    case SWFDEC_SCALE_SHOW_ALL:
      priv->scale_x = MIN (scale_x, scale_y);
      priv->scale_y = priv->scale_x;
      break;
    case SWFDEC_SCALE_NO_BORDER:
      priv->scale_x = MAX (scale_x, scale_y);
      priv->scale_y = priv->scale_x;
      break;
    case SWFDEC_SCALE_EXACT_FIT:
      priv->scale_x = scale_x;
      priv->scale_y = scale_y;
      break;
    case SWFDEC_SCALE_NONE:
      priv->scale_x = 1.0;
      priv->scale_y = 1.0;
      break;
    default:
      g_assert_not_reached ();
  }
  width = priv->stage.width - ceil (priv->width * priv->scale_x);
  height = priv->stage.height - ceil (priv->height * priv->scale_y);
  if (priv->align_flags & SWFDEC_ALIGN_FLAG_LEFT) {
    priv->offset_x = 0;
  } else if (priv->align_flags & SWFDEC_ALIGN_FLAG_RIGHT) {
    priv->offset_x = width;
  } else {
    priv->offset_x = width / 2;
  }
  if (priv->align_flags & SWFDEC_ALIGN_FLAG_TOP) {
    priv->offset_y = 0;
  } else if (priv->align_flags & SWFDEC_ALIGN_FLAG_BOTTOM) {
    priv->offset_y = height;
  } else {
    priv->offset_y = height / 2;
  }
  SWFDEC_LOG ("coordinate translation is %g * x + %d - %g * y + %d", 
      priv->scale_x, priv->offset_x, priv->scale_y, priv->offset_y);
#if 0
  /* FIXME: make this emit the signal at the right time */
  priv->invalid.x0 = 0;
  priv->invalid.y0 = 0;
  priv->invalid.x1 = priv->stage_width;
  priv->invalid.y1 = priv->stage_height;
#endif
}

static void
swfdec_player_set_property (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (object);
  SwfdecPlayerPrivate *priv = player->priv;

  switch (param_id) {
    case PROP_BACKGROUND_COLOR:
      swfdec_player_set_background_color (player, g_value_get_uint (value));
      break;
    case PROP_CACHE_SIZE:
      priv->cache->max_size = g_value_get_uint (value);
      break;
    case PROP_WIDTH:
      swfdec_player_set_size (player, g_value_get_int (value), priv->stage_height);
      break;
    case PROP_HEIGHT:
      swfdec_player_set_size (player, priv->stage_width, g_value_get_int (value));
      break;
    case PROP_ALIGNMENT:
      priv->align_flags = swfdec_player_alignment_to_flags (g_value_get_enum (value));
      swfdec_player_update_scale (player);
      break;
    case PROP_SCALE:
      swfdec_player_set_scale_mode (player, g_value_get_enum (value));
      break;
    case PROP_SCRIPTING:
      swfdec_player_set_scripting (player, g_value_get_object (value));
      break;
    case PROP_SYSTEM:
      g_object_unref (priv->system);
      if (g_value_get_object (value)) {
	priv->system = SWFDEC_SYSTEM (g_value_dup_object (value));
      } else {
	priv->system = swfdec_system_new ();
      }
      break;
    case PROP_MAX_RUNTIME:
      swfdec_player_set_maximum_runtime (player, g_value_get_ulong (value));
      break;
    case PROP_SOCKET_TYPE:
      priv->socket_type = g_value_get_gtype (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_player_dispose (GObject *object)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (object);
  SwfdecPlayerPrivate *priv = player->priv;
  guint i;

  swfdec_player_stop_all_sounds (player);
  swfdec_player_resource_request_finish (player);
  g_hash_table_destroy (priv->registered_classes);
  g_hash_table_destroy (priv->scripting_callbacks);

  while (priv->roots)
    swfdec_movie_destroy (priv->roots->data);
  if (priv->resource) {
    swfdec_flash_security_free_pending (
	SWFDEC_FLASH_SECURITY (priv->resource));
    g_object_unref (priv->resource);
    priv->resource = NULL;
  }
  while (priv->rooted_objects)
    swfdec_player_unroot_object (player, priv->rooted_objects->data);

  /* we do this here so references to GC'd objects get freed */
  G_OBJECT_CLASS (swfdec_player_parent_class)->dispose (object);

  swfdec_player_remove_all_external_actions (player, player);
#ifndef G_DISABLE_ASSERT
  {
    SwfdecPlayerExternalAction *action;
    while ((action = swfdec_ring_buffer_pop (priv->external_actions)) != NULL) {
      g_assert (action->object == NULL); /* skip removed actions */
    }
  }
  {
    SwfdecPlayerAction *action;
    for (i = 0; i < SWFDEC_PLAYER_N_ACTION_QUEUES; i++) {
      while ((action = swfdec_ring_buffer_pop (priv->actions[i])) != NULL) {
	g_assert (action->movie == NULL); /* skip removed actions */
      }
    }
  }
#endif
  swfdec_ring_buffer_free (priv->external_actions);
  for (i = 0; i < SWFDEC_PLAYER_N_ACTION_QUEUES; i++) {
    swfdec_ring_buffer_free (priv->actions[i]);
  }
  g_assert (priv->movies == NULL);
  g_assert (priv->audio == NULL);
  if (priv->external_timeout.callback)
    swfdec_player_remove_timeout (player, &priv->external_timeout);
  if (priv->rate) {
    swfdec_player_remove_timeout (player, &priv->iterate_timeout);
  }
  g_assert (priv->timeouts == NULL);
  g_list_free (priv->intervals);
  priv->intervals = NULL;
  swfdec_cache_unref (priv->cache);
  if (priv->system) {
    g_object_unref (priv->system);
    priv->system = NULL;
  }
  g_array_free (priv->invalidations, TRUE);
  priv->invalidations = NULL;
  if (priv->runtime) {
    g_timer_destroy (priv->runtime);
    priv->runtime = NULL;
  }
}

static void
swfdec_player_broadcast (SwfdecPlayer *player, const char *object_name, const char *signal)
{
  SwfdecAsValue val;
  SwfdecAsObject *obj;

  SWFDEC_DEBUG ("broadcasting message %s.%s", object_name, signal);
  obj = SWFDEC_AS_CONTEXT (player)->global;
  swfdec_as_object_get_variable (obj, object_name, &val);
  if (!SWFDEC_AS_VALUE_IS_OBJECT (&val))
    return;
  obj = SWFDEC_AS_VALUE_GET_OBJECT (&val);
  SWFDEC_AS_VALUE_SET_STRING (&val, signal);
  swfdec_as_object_call (obj, SWFDEC_AS_STR_broadcastMessage, 1, &val, NULL);
}

static void
swfdec_player_update_mouse_cursor (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv = player->priv;
  SwfdecMouseCursor new = SWFDEC_MOUSE_CURSOR_NORMAL;

  if (!priv->mouse_visible) {
    new = SWFDEC_MOUSE_CURSOR_NONE;
  } else if (priv->mouse_grab != NULL) {
    SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);

    if (klass->mouse_cursor)
      new = klass->mouse_cursor (priv->mouse_grab);
    else
      new = SWFDEC_MOUSE_CURSOR_CLICK;
  }

  if (new != priv->mouse_cursor) {
    priv->mouse_cursor = new;
    g_object_notify (G_OBJECT (player), "mouse-cursor");
  }
}

static void
swfdec_player_update_drag_movie (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv = player->priv;
  double x, y;
  SwfdecMovie *movie;

  if (priv->mouse_drag == NULL)
    return;

  movie = priv->mouse_drag;
  g_assert (movie->cache_state == SWFDEC_MOVIE_UP_TO_DATE);
  x = priv->mouse_x;
  y = priv->mouse_y;
  swfdec_player_stage_to_global (player, &x, &y);
  if (movie->parent)
    swfdec_movie_global_to_local (movie->parent, &x, &y);
  if (priv->mouse_drag_center) {
    x -= (movie->extents.x1 - movie->extents.x0) / 2;
    y -= (movie->extents.y1 - movie->extents.y0) / 2;
  } else {
    x -= priv->mouse_drag_x;
    y -= priv->mouse_drag_y;
  }
  x = CLAMP (x, priv->mouse_drag_rect.x0, priv->mouse_drag_rect.x1);
  y = CLAMP (y, priv->mouse_drag_rect.y0, priv->mouse_drag_rect.y1);
  SWFDEC_LOG ("mouse is at %g %g, originally (%g %g)", x, y, priv->mouse_x, priv->mouse_y);
  if (x != movie->matrix.x0 || y != movie->matrix.y0) {
    swfdec_movie_queue_update (movie, SWFDEC_MOVIE_INVALID_MATRIX);
    movie->matrix.x0 = x;
    movie->matrix.y0 = y;
  }
}

/**
 * swfdec_player_set_drag_movie:
 * @player: a #SwfdecPlayer
 * @drag: the movie to be dragged by the mouse or NULL to unset
 * @center: TRUE if the center of @drag should be at the mouse pointer, FALSE if (0,0)
 *          of @drag should be at the mouse pointer.
 * @rect: NULL or the rectangle that clips the mouse position. The coordinates 
 *        are in the coordinate system of the parent of @drag.
 *
 * Sets or unsets the movie that is dragged by the mouse.
 **/
void
swfdec_player_set_drag_movie (SwfdecPlayer *player, SwfdecMovie *drag, gboolean center,
    SwfdecRect *rect)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (drag == NULL || SWFDEC_IS_MOVIE (drag));

  /* FIXME: need to do anything with old drag? */
  priv = player->priv;
  priv->mouse_drag = drag;
  priv->mouse_drag_center = center;
  if (drag && !center) {
    priv->mouse_drag_x = priv->mouse_x;
    priv->mouse_drag_y = priv->mouse_y;
    swfdec_player_stage_to_global (player, &priv->mouse_drag_x, &priv->mouse_drag_y);
    if (drag->parent)
      swfdec_movie_global_to_local (drag->parent, &priv->mouse_drag_x, &priv->mouse_drag_y);
    priv->mouse_drag_x -= drag->matrix.x0;
    priv->mouse_drag_y -= drag->matrix.y0;
  }
  if (rect) {
    priv->mouse_drag_rect = *rect;
  } else {
    priv->mouse_drag_rect.x0 = -G_MAXDOUBLE;
    priv->mouse_drag_rect.y0 = -G_MAXDOUBLE;
    priv->mouse_drag_rect.x1 = G_MAXDOUBLE;
    priv->mouse_drag_rect.y1 = G_MAXDOUBLE;
  }
  SWFDEC_DEBUG ("starting drag in %g %g  %g %g", 
      priv->mouse_drag_rect.x0, priv->mouse_drag_rect.y0,
      priv->mouse_drag_rect.x1, priv->mouse_drag_rect.y1);
  /* FIXME: need a way to make sure we get updated */
  if (drag) {
    swfdec_movie_update (drag);
    drag->modified = TRUE;
    swfdec_player_update_drag_movie (player);
  }
}

static void
swfdec_player_grab_mouse_movie (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv = player->priv;
  GList *walk;
  double x, y;
  SwfdecMovie *below_mouse = NULL;

  x = priv->mouse_x;
  y = priv->mouse_y;
  swfdec_player_stage_to_global (player, &x, &y);
  for (walk = g_list_last (priv->roots); walk; walk = walk->prev) {
    below_mouse = swfdec_movie_get_movie_at (walk->data, x, y, TRUE);
    if (below_mouse) {
      if (swfdec_movie_get_mouse_events (below_mouse))
	break;
      below_mouse = NULL;
    }
  }
  if (swfdec_player_is_mouse_pressed (player)) {
    /* a mouse grab is active */
    if (priv->mouse_grab) {
      if (below_mouse == priv->mouse_grab &&
	  priv->mouse_below != priv->mouse_grab) {
	SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);
	if (klass->mouse_in)
	  klass->mouse_in (priv->mouse_grab);
      } else if (below_mouse != priv->mouse_grab &&
	  priv->mouse_below == priv->mouse_grab) {
	SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);
	if (klass->mouse_out)
	  klass->mouse_out (priv->mouse_grab);
      }
    }
  } else {
    /* no mouse grab is active */
    if (below_mouse != priv->mouse_grab) {
      if (priv->mouse_grab) {
	SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);
	if (klass->mouse_out)
	  klass->mouse_out (priv->mouse_grab);
      }
      if (below_mouse) {
	SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (below_mouse);
	if (klass->mouse_in)
	  klass->mouse_in (below_mouse);
      }
    }
    priv->mouse_grab = below_mouse;
  }
  priv->mouse_below = below_mouse;
  SWFDEC_DEBUG ("%s %p has mouse at %g %g", 
      priv->mouse_grab ? G_OBJECT_TYPE_NAME (priv->mouse_grab) : "---", 
      priv->mouse_grab, priv->mouse_x, priv->mouse_y);
}

static gboolean
swfdec_player_do_mouse_move (SwfdecPlayer *player, double x, double y)
{
  SwfdecPlayerPrivate *priv = player->priv;
  GList *walk;
  
  if (priv->mouse_x != x || priv->mouse_y != y) {
    priv->mouse_x = x;
    priv->mouse_y = y;
    for (walk = priv->movies; walk; walk = walk->next) {
      swfdec_movie_queue_script (walk->data, SWFDEC_EVENT_MOUSE_MOVE);
    }
    swfdec_player_broadcast (player, SWFDEC_AS_STR_Mouse, SWFDEC_AS_STR_onMouseMove);
  }
  swfdec_player_grab_mouse_movie (player);
  swfdec_player_update_drag_movie (player);

  /* FIXME: allow events to pass through */
  return TRUE;
}

static gboolean
swfdec_player_do_mouse_press (SwfdecPlayer *player, guint button)
{
  SwfdecPlayerPrivate *priv = player->priv;
  GList *walk;

  priv->mouse_button |= 1 << button;
  if (button == 0) {
    for (walk = priv->movies; walk; walk = walk->next) {
      swfdec_movie_queue_script (walk->data, SWFDEC_EVENT_MOUSE_DOWN);
    }
    swfdec_player_broadcast (player, SWFDEC_AS_STR_Mouse, SWFDEC_AS_STR_onMouseDown);
  }
  if (priv->mouse_grab) {
    SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);
    if (klass->mouse_press)
      klass->mouse_press (priv->mouse_grab, button);
  }

  /* FIXME: allow events to pass through */
  return TRUE;
}

static gboolean
swfdec_player_do_mouse_release (SwfdecPlayer *player, guint button)
{
  SwfdecPlayerPrivate *priv = player->priv;
  GList *walk;

  priv->mouse_button &= ~(1 << button);
  if (button == 0) {
    for (walk = priv->movies; walk; walk = walk->next) {
      swfdec_movie_queue_script (walk->data, SWFDEC_EVENT_MOUSE_UP);
    }
    swfdec_player_broadcast (player, SWFDEC_AS_STR_Mouse, SWFDEC_AS_STR_onMouseUp);
  }
  if (priv->mouse_grab) {
    SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);
    if (klass->mouse_release)
      klass->mouse_release (priv->mouse_grab, button);
    if (button == 0 && priv->mouse_grab != priv->mouse_below) {
      priv->mouse_grab = priv->mouse_below;
      if (priv->mouse_grab) {
	klass = SWFDEC_MOVIE_GET_CLASS (priv->mouse_grab);
	if (klass->mouse_in)
	  klass->mouse_in (priv->mouse_grab);
      }
    }
  }

  /* FIXME: allow events to pass through */
  return TRUE;
}

static void
swfdec_player_emit_signals (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv = player->priv;
  GList *walk;

  /* emit invalidate signal */
  if (!swfdec_rectangle_is_empty (&priv->invalid_extents)) {
    g_signal_emit (player, signals[INVALIDATE], 0, &priv->invalid_extents,
	priv->invalidations->data, priv->invalidations->len);
    swfdec_rectangle_init_empty (&priv->invalid_extents);
    g_array_set_size (priv->invalidations, 0);
  }

  /* emit audio-added for all added audio streams */
  for (walk = priv->audio; walk; walk = walk->next) {
    SwfdecAudio *audio = walk->data;

    if (audio->added)
      continue;
    g_signal_emit (player, signals[AUDIO_ADDED], 0, audio);
    audio->added = TRUE;
  }
}

static gboolean
swfdec_player_do_handle_key (SwfdecPlayer *player, guint keycode, guint character, gboolean down)
{
  SwfdecPlayerPrivate *priv = player->priv;
  g_assert (keycode < 256);

  if (!swfdec_player_lock (player))
    return FALSE;
  /* set the correct variables */
  priv->last_keycode = keycode;
  priv->last_character = character;
  if (down) {
    priv->key_pressed[keycode / 8] |= 1 << keycode % 8;
  } else {
    priv->key_pressed[keycode / 8] &= ~(1 << keycode % 8);
  }
  swfdec_player_broadcast (player, SWFDEC_AS_STR_Key, down ? SWFDEC_AS_STR_onKeyDown : SWFDEC_AS_STR_onKeyUp);
  swfdec_player_perform_actions (player);
  swfdec_player_unlock (player);

  return TRUE;
}

static gboolean
swfdec_player_do_handle_mouse (SwfdecPlayer *player, 
    double x, double y, int button)
{
  gboolean ret;

  if (!swfdec_player_lock (player))
    return FALSE;

  SWFDEC_LOG ("handling mouse for %g %g %d", x, y, button);
  ret = swfdec_player_do_mouse_move (player, x, y);
  if (button > 0) {
    ret |= swfdec_player_do_mouse_press (player, button - 1);
  } else if (button < 0) {
    ret |= swfdec_player_do_mouse_release (player, -button - 1);
  }
  swfdec_player_perform_actions (player);
  swfdec_player_unlock (player);

  return ret;
}

void
swfdec_player_global_to_stage (SwfdecPlayer *player, double *x, double *y)
{
  SwfdecPlayerPrivate *priv = player->priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  *x = *x / SWFDEC_TWIPS_SCALE_FACTOR * priv->scale_x + priv->offset_x;
  *y = *y / SWFDEC_TWIPS_SCALE_FACTOR * priv->scale_y + priv->offset_y;
}

void
swfdec_player_stage_to_global (SwfdecPlayer *player, double *x, double *y)
{
  SwfdecPlayerPrivate *priv = player->priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  *x = (*x - priv->offset_x) / priv->scale_x * SWFDEC_TWIPS_SCALE_FACTOR;
  *y = (*y - priv->offset_y) / priv->scale_y * SWFDEC_TWIPS_SCALE_FACTOR;
}

static void
swfdec_player_execute_on_load_init (SwfdecPlayer *player)
{
  GList *walk;

  /* FIXME: This can be made a LOT faster with correct caching, but I'm lazy */
  do {
    for (walk = player->priv->movies; walk; walk = walk->next) {
      SwfdecMovie *movie = walk->data;
      SwfdecResource *resource = swfdec_movie_get_own_resource (movie);
      if (resource == NULL)
	continue;
      if (swfdec_resource_emit_on_load_init (resource))
	break;
    }
  } while (walk != NULL);
}

static void
swfdec_player_iterate (SwfdecTimeout *timeout)
{
  SwfdecPlayerPrivate *priv = (SwfdecPlayerPrivate *) ((void *) ((guint8 *) timeout - G_STRUCT_OFFSET (SwfdecPlayerPrivate, iterate_timeout)));
  SwfdecPlayer *player = priv->player;
  GList *walk;

  /* add timeout again - do this first because later code can change it */
  /* FIXME: rounding issues? */
  priv->iterate_timeout.timestamp += SWFDEC_TICKS_PER_SECOND * 256 / priv->rate;
  swfdec_player_add_timeout (player, &priv->iterate_timeout);
  swfdec_player_perform_external_actions (player);
  SWFDEC_INFO ("=== START ITERATION ===");
  /* start the iteration. This performs a goto next frame on all 
   * movies that are not stopped. It also queues onEnterFrame.
   */
  for (walk = priv->movies; walk; walk = walk->next) {
    SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (walk->data);
    if (klass->iterate_start)
      klass->iterate_start (walk->data);
  }
  swfdec_player_perform_actions (player);
  SWFDEC_INFO ("=== STOP ITERATION ===");
  /* this loop allows removal of walk->data */
  walk = priv->movies;
  while (walk) {
    SwfdecMovie *cur = walk->data;
    SwfdecMovieClass *klass = SWFDEC_MOVIE_GET_CLASS (cur);
    walk = walk->next;
    g_assert (klass->iterate_end);
    if (!klass->iterate_end (cur))
      swfdec_movie_destroy (cur);
  }
  swfdec_player_execute_on_load_init (player);
  swfdec_player_resource_request_perform (player);
  swfdec_player_perform_actions (player);
}

static void
swfdec_player_advance_audio (SwfdecPlayer *player, guint samples)
{
  SwfdecAudio *audio;
  GList *walk;

  if (samples == 0)
    return;

  /* don't use for loop here, because we need to advance walk before 
   * removing the audio */
  walk = player->priv->audio;
  while (walk) {
    audio = walk->data;
    walk = walk->next;
    if (swfdec_audio_iterate (audio, samples) == 0)
      swfdec_audio_remove (audio);
  }
}

static void
swfdec_player_do_advance (SwfdecPlayer *player, gulong msecs, guint audio_samples)
{
  SwfdecPlayerPrivate *priv = player->priv;
  SwfdecTimeout *timeout;
  SwfdecTick target_time;
  guint frames_now;
  
  if (!swfdec_player_lock (player))
    return;

  target_time = priv->time + SWFDEC_MSECS_TO_TICKS (msecs);
  SWFDEC_DEBUG ("advancing %lu msecs (%u audio frames)", msecs, audio_samples);

  for (timeout = priv->timeouts ? priv->timeouts->data : NULL;
       timeout && timeout->timestamp <= target_time; 
       timeout = priv->timeouts ? priv->timeouts->data : NULL) {
    priv->timeouts = g_list_remove (priv->timeouts, timeout);
    frames_now = SWFDEC_TICKS_TO_SAMPLES (timeout->timestamp) -
      SWFDEC_TICKS_TO_SAMPLES (priv->time);
    priv->time = timeout->timestamp;
    swfdec_player_advance_audio (player, frames_now);
    audio_samples -= frames_now;
    SWFDEC_LOG ("activating timeout %p now (timeout is %"G_GUINT64_FORMAT", target time is %"G_GUINT64_FORMAT,
	timeout, timeout->timestamp, target_time);
    timeout->callback (timeout);
    swfdec_player_perform_actions (player);
  }
  if (target_time > priv->time) {
    frames_now = SWFDEC_TICKS_TO_SAMPLES (target_time) -
      SWFDEC_TICKS_TO_SAMPLES (priv->time);
    priv->time = target_time;
    swfdec_player_advance_audio (player, frames_now);
    audio_samples -= frames_now;
  }
  g_assert (audio_samples == 0);
  
  swfdec_player_unlock (player);
}

void
swfdec_player_perform_actions (SwfdecPlayer *player)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  while (swfdec_player_do_action (player));
}

/* used for breakpoints */
void
swfdec_player_lock_soft (SwfdecPlayer *player)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_assert (swfdec_rectangle_is_empty (&player->priv->invalid_extents));

  g_object_freeze_notify (G_OBJECT (player));
  g_timer_start (player->priv->runtime);
  SWFDEC_DEBUG ("LOCKED");
}

gboolean
swfdec_player_lock (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[0]) == 0);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[1]) == 0);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[2]) == 0);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[3]) == 0);

  if (swfdec_as_context_is_aborted (SWFDEC_AS_CONTEXT (player)))
    return FALSE;

  g_object_ref (player);
  swfdec_player_lock_soft (player);
  return TRUE;
}

/* runs queued invalidations for all movies and resets the movies */
static void
swfdec_player_update_movies (SwfdecPlayer *player)
{
  SwfdecMovie *movie;
  GList *walk;

  /* FIXME: This g_list_last could be slow */
  for (walk = g_list_last (player->priv->movies); walk; walk = walk->prev) {
    movie = walk->data;

    swfdec_movie_update (movie);
    if (movie->invalidate_last) {
      cairo_matrix_t matrix;

      if (movie->parent)
	swfdec_movie_local_to_global_matrix (movie->parent, &matrix);
      else
	cairo_matrix_init_identity (&matrix);
      swfdec_movie_invalidate (movie, &matrix, TRUE);
      movie->invalidate_last = FALSE;
    }
  }
}

/* used for breakpoints */
void
swfdec_player_unlock_soft (SwfdecPlayer *player)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  SWFDEC_DEBUG ("UNLOCK");
  g_timer_stop (player->priv->runtime);
  swfdec_player_update_movies (player);
  swfdec_player_update_mouse_cursor (player);
  g_object_thaw_notify (G_OBJECT (player));
  swfdec_player_emit_signals (player);
}

void
swfdec_player_unlock (SwfdecPlayer *player)
{
  SwfdecAsContext *context;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[0]) == 0);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[1]) == 0);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[2]) == 0);
  g_assert (swfdec_ring_buffer_get_n_elements (player->priv->actions[3]) == 0);
  context = SWFDEC_AS_CONTEXT (player);
  g_return_if_fail (context->state != SWFDEC_AS_CONTEXT_INTERRUPTED);

  if (context->state == SWFDEC_AS_CONTEXT_RUNNING)
    swfdec_as_context_maybe_gc (SWFDEC_AS_CONTEXT (player));
  swfdec_player_unlock_soft (player);
  g_object_unref (player);
}

static gboolean
swfdec_accumulate_or (GSignalInvocationHint *ihint, GValue *return_accu, 
    const GValue *handler_return, gpointer data)
{
  if (g_value_get_boolean (handler_return))
    g_value_set_boolean (return_accu, TRUE);
  return TRUE;
}

static void
swfdec_player_mark_string_object (gpointer key, gpointer value, gpointer data)
{
  swfdec_as_string_mark (key);
  swfdec_as_object_mark (value);
}

static void
swfdec_player_mark_rooted_object (gpointer object, gpointer unused)
{
  if (SWFDEC_IS_RESOURCE (object)) {
    swfdec_resource_mark (object);
  } else if (SWFDEC_IS_AS_OBJECT (object)) {
    swfdec_as_object_mark (object);
  }
}

static void
swfdec_player_mark (SwfdecAsContext *context)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (context);
  SwfdecPlayerPrivate *priv = player->priv;

  g_hash_table_foreach (priv->registered_classes, swfdec_player_mark_string_object, NULL);
  g_hash_table_foreach (priv->scripting_callbacks, swfdec_player_mark_string_object, NULL);
  swfdec_as_object_mark (priv->MovieClip);
  swfdec_as_object_mark (priv->Video);
  g_list_foreach (priv->roots, (GFunc) swfdec_as_object_mark, NULL);
  g_list_foreach (priv->intervals, (GFunc) swfdec_as_object_mark, NULL);
  g_list_foreach (priv->rooted_objects, swfdec_player_mark_rooted_object, NULL);

  SWFDEC_AS_CONTEXT_CLASS (swfdec_player_parent_class)->mark (context);
}

static void
swfdec_player_get_time (SwfdecAsContext *context, GTimeVal *tv)
{
  *tv = context->start_time;

  /* FIXME: what granularity do we want? Currently it's milliseconds */
  g_time_val_add (tv, SWFDEC_TICKS_TO_MSECS (SWFDEC_PLAYER (context)->priv->time) * 1000);
}

static gboolean
swfdec_player_check_continue (SwfdecAsContext *context)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (context);
  SwfdecPlayerPrivate *priv = player->priv;

  if (priv->max_runtime == 0)
    return TRUE;
  return g_timer_elapsed (priv->runtime, NULL) * 1000 <= priv->max_runtime;
}

static void
swfdec_player_class_init (SwfdecPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecAsContextClass *context_class = SWFDEC_AS_CONTEXT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwfdecPlayerPrivate));

  object_class->get_property = swfdec_player_get_property;
  object_class->set_property = swfdec_player_set_property;
  object_class->dispose = swfdec_player_dispose;

  g_object_class_install_property (object_class, PROP_INITIALIZED,
      g_param_spec_boolean ("initialized", "initialized", "TRUE when the player has initialized its basic values",
	  FALSE, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_DEFAULT_WIDTH,
      g_param_spec_uint ("default-width", "default width", "default width of the movie",
	  0, G_MAXUINT, 0, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_DEFAULT_HEIGHT,
      g_param_spec_uint ("default-height", "default height", "default height of the movie",
	  0, G_MAXUINT, 0, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_RATE,
      g_param_spec_double ("rate", "rate", "rate in frames per second",
	  0.0, 256.0, 0.0, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_MOUSE_CURSOR,
      g_param_spec_enum ("mouse-cursor", "mouse cursor", "how the mouse pointer should be presented",
	  SWFDEC_TYPE_MOUSE_CURSOR, SWFDEC_MOUSE_CURSOR_NONE, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_NEXT_EVENT,
      g_param_spec_long ("next-event", "next event", "how many milliseconds until the next event or 0 when no event pending",
	  -1, G_MAXLONG, -1, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_CACHE_SIZE,
      g_param_spec_uint ("cache-size", "cache size", "maximum cache size in bytes",
	  0, G_MAXUINT, 50 * 1024 * 1024, G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_BACKGROUND_COLOR,
      g_param_spec_uint ("background-color", "background color", "ARGB color used to draw the background",
	  0, G_MAXUINT, SWFDEC_COLOR_COMBINE (0xFF, 0xFF, 0xFF, 0xFF), G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_WIDTH,
      g_param_spec_int ("width", "width", "current width of the movie",
	  -1, G_MAXINT, -1, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height", "current height of the movie",
	  -1, G_MAXINT, -1, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_ALIGNMENT,
      g_param_spec_enum ("alignment", "alignment", "point of the screen to align the output to",
	  SWFDEC_TYPE_ALIGNMENT, SWFDEC_ALIGNMENT_CENTER, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SCALE,
      g_param_spec_enum ("scale-mode", "scale mode", "method used to scale the movie",
	  SWFDEC_TYPE_SCALE_MODE, SWFDEC_SCALE_SHOW_ALL, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SCRIPTING,
      g_param_spec_object ("scripting", "scripting", "external scripting implementation",
	  SWFDEC_TYPE_PLAYER_SCRIPTING, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SCALE,
      g_param_spec_object ("system", "system", "object holding system information",
	  SWFDEC_TYPE_SYSTEM, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_MAX_RUNTIME,
      g_param_spec_ulong ("max-runtime", "maximum runtime", "maximum time in msecs scripts may run in the player before aborting",
	  0, G_MAXULONG, 10 * 1000, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SOCKET_TYPE,
      g_param_spec_gtype ("socket type", "socket type", "type to use for creating sockets",
	  SWFDEC_TYPE_SOCKET, G_PARAM_READWRITE));

  /**
   * SwfdecPlayer::invalidate:
   * @player: the #SwfdecPlayer affected
   * @extents: the smallest rectangle enclosing the full region of changes
   * @rectangles: a number of smaller rectangles for fine-grained control over 
   *              changes
   * @n_rectangles: number of rectangles in @rectangles
   *
   * This signal is emitted whenever graphical elements inside the player have 
   * changed. It provides two ways to look at the changes: By looking at the
   * @extents parameter, it provides a simple way to get a single rectangle that
   * encloses all changes. By looking at the @rectangles array, you can get
   * finer control over changes which is very useful if your rendering system 
   * provides a way to handle regions.
   */
  signals[INVALIDATE] = g_signal_new ("invalidate", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, swfdec_marshal_VOID__BOXED_POINTER_UINT,
      G_TYPE_NONE, 3, SWFDEC_TYPE_RECTANGLE, G_TYPE_POINTER, G_TYPE_UINT);
  /**
   * SwfdecPlayer::advance:
   * @player: the #SwfdecPlayer affected
   * @msecs: the amount of milliseconds the player will advance
   * @audio_samples: number of frames the audio is advanced (in 44100Hz steps)
   *
   * Emitted whenever the player advances.
   */
  signals[ADVANCE] = g_signal_new ("advance", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SwfdecPlayerClass, advance), 
      NULL, NULL, swfdec_marshal_VOID__ULONG_UINT,
      G_TYPE_NONE, 2, G_TYPE_ULONG, G_TYPE_UINT);
  /**
   * SwfdecPlayer::handle-key:
   * @player: the #SwfdecPlayer affected
   * @key: #SwfdecKey that was pressed or released
   * @pressed: %TRUE if the @key was pressed or %FALSE if it was released
   *
   * This signal is emitted whenever @player should respond to a key event. If
   * any of the handlers returns TRUE, swfdec_player_key_press() or 
   * swfdec_player_key_release() will return TRUE. Note that unlike many event 
   * handlers in gtk, returning TRUE will not stop further event handlers from 
   * being invoked. Use g_signal_stop_emission() in that case.
   *
   * Returns: TRUE if this handler handles the event. 
   **/
  signals[HANDLE_KEY] = g_signal_new ("handle-key", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SwfdecPlayerClass, handle_key), 
      swfdec_accumulate_or, NULL, swfdec_marshal_BOOLEAN__UINT_UINT_BOOLEAN,
      G_TYPE_BOOLEAN, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOOLEAN);
  /**
   * SwfdecPlayer::handle-mouse:
   * @player: the #SwfdecPlayer affected
   * @x: new x coordinate of the mouse
   * @y: new y coordinate of the mouse
   * @button: 0 for a mouse move, a positive number if a button was pressed,
   *          a negative number if a button was released
   *
   * This signal is emitted whenever @player should respond to a mouse event. If
   * any of the handlers returns TRUE, swfdec_player_handle_mouse() will return 
   * TRUE. Note that unlike many event handlers in gtk, returning TRUE will not 
   * stop further event handlers from being invoked. Use g_signal_stop_emission()
   * in that case.
   *
   * Returns: TRUE if this handler handles the event. 
   **/
  signals[HANDLE_MOUSE] = g_signal_new ("handle-mouse", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SwfdecPlayerClass, handle_mouse), 
      swfdec_accumulate_or, NULL, swfdec_marshal_BOOLEAN__DOUBLE_DOUBLE_INT,
      G_TYPE_BOOLEAN, 3, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_INT);
  /**
   * SwfdecPlayer::audio-added:
   * @player: the #SwfdecPlayer affected
   * @audio: the audio stream that was added
   *
   * Emitted whenever a new audio stream was added to @player.
   */
  signals[AUDIO_ADDED] = g_signal_new ("audio-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, SWFDEC_TYPE_AUDIO);
  /**
   * SwfdecPlayer::audio-removed:
   * @player: the #SwfdecPlayer affected
   * @audio: the audio stream that was removed
   *
   * Emitted whenever an audio stream was removed from @player. The stream will 
   * have been added with the SwfdecPlayer::audio-added signal previously. 
   */
  signals[AUDIO_REMOVED] = g_signal_new ("audio-removed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, SWFDEC_TYPE_AUDIO);
  /**
   * SwfdecPlayer::fscommand:
   * @player: the #SwfdecPlayer affected
   * @command: the command to execute. This is a lower case string.
   * @parameter: parameter to pass to the command. The parameter depends on the 
   *             function.
   *
   * This signal is emited whenever a Flash script command (also known as 
   * fscommand) is encountered. This method is ued by the Flash file to
   * communicate with the hosting environment. In web browsers it is used to 
   * call Javascript functions. Standalone Flash players understand a limited 
   * set of functions. They vary from player to player, but the most common are 
   * listed here: <itemizedlist>
   * <listitem><para>"quit": quits the player.</para></listitem>
   * <listitem><para>"fullscreen": A boolean setting (parameter is "true" or 
   * "false") that sets the player into fullscreen mode.</para></listitem>
   * <listitem><para>"allowscale": A boolean setting that tells the player to
   * not scale the Flash application.</para></listitem>
   * <listitem><para>"showmenu": A boolean setting that tells the Flash player
   * to not show its own entries in the right-click menu.</para></listitem>
   * <listitem><para>"exec": Run an external executable. The parameter 
   * specifies the path.</para></listitem>
   * <listitem><para>"trapallkeys": A boolean setting that tells the Flash 
   * player to pass all key events to the Flash application instead of using it
   * for keyboard shortcuts or similar.</para></listitem>
   * </itemizedlist>
   */
  signals[FSCOMMAND] = g_signal_new ("fscommand", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, swfdec_marshal_VOID__STRING_STRING,
      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
  /**
   * SwfdecPlayer::launch:
   * @player: the #SwfdecPlayer affected
   * @request: the type of request
   * @url: URL to open
   * @target: target to load the URL into
   * @data: optional data to pass on with the request. Will be of mime type
   *        application/x-www-form-urlencoded. Can be %NULL indicating no data
   *        should be passed.
   *
   * Emitted whenever the @player encounters an URL that should be loaded into 
   * a target the Flash player does not recognize. In most cases this happens 
   * when the user clicks a link in an embedded Flash movie that should open a
   * new web page.
   * The effect of calling any swfdec functions on the emitting @player is undefined.
   */
  signals[LAUNCH] = g_signal_new ("launch", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, swfdec_marshal_VOID__ENUM_STRING_STRING_BOXED,
      G_TYPE_NONE, 4, SWFDEC_TYPE_LOADER_REQUEST, G_TYPE_STRING, G_TYPE_STRING, 
      SWFDEC_TYPE_BUFFER);

  context_class->mark = swfdec_player_mark;
  context_class->get_time = swfdec_player_get_time;
  context_class->check_continue = swfdec_player_check_continue;

  klass->advance = swfdec_player_do_advance;
  klass->handle_key = swfdec_player_do_handle_key;
  klass->handle_mouse = swfdec_player_do_handle_mouse;
}

static void
swfdec_player_init (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv;
  guint i;

  player->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (player, SWFDEC_TYPE_PLAYER, SwfdecPlayerPrivate);
  priv->player = player;

  priv->system = swfdec_system_new ();
  priv->registered_classes = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->scripting_callbacks = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (i = 0; i < SWFDEC_PLAYER_N_ACTION_QUEUES; i++) {
    priv->actions[i] = swfdec_ring_buffer_new_for_type (SwfdecPlayerAction, 16);
  }
  priv->external_actions = swfdec_ring_buffer_new_for_type (SwfdecPlayerExternalAction, 8);
  priv->cache = swfdec_cache_new (50 * 1024 * 1024); /* 100 MB */
  priv->bgcolor = SWFDEC_COLOR_COMBINE (0xFF, 0xFF, 0xFF, 0xFF);
  priv->socket_type = SWFDEC_TYPE_SOCKET;

  priv->runtime = g_timer_new ();
  g_timer_stop (priv->runtime);
  priv->max_runtime = 10 * 1000;
  priv->invalidations = g_array_new (FALSE, FALSE, sizeof (SwfdecRectangle));
  priv->mouse_visible = TRUE;
  priv->mouse_cursor = SWFDEC_MOUSE_CURSOR_NORMAL;
  priv->iterate_timeout.callback = swfdec_player_iterate;
  priv->stage_width = -1;
  priv->stage_height = -1;

  swfdec_player_resource_request_init (player);
}

void
swfdec_player_stop_all_sounds (SwfdecPlayer *player)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  priv = player->priv;
  while (priv->audio) {
    swfdec_audio_remove (priv->audio->data);
  }
}

void
swfdec_player_stop_sounds (SwfdecPlayer *player, SwfdecAudioRemoveFunc func, gpointer data)
{
  GList *walk;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (func);

  walk = player->priv->audio;
  while (walk) {
    SwfdecAudio *audio = walk->data;
    walk = walk->next;
    if (func (audio, data))
      swfdec_audio_remove (audio);
  }
}

/* rect is in global coordinates */
void
swfdec_player_invalidate (SwfdecPlayer *player, const SwfdecRect *rect)
{
  SwfdecPlayerPrivate *priv = player->priv;
  SwfdecRectangle r;
  SwfdecRect tmp;
  guint i;

  if (swfdec_rect_is_empty (rect))
    return;

  tmp = *rect;
  swfdec_player_global_to_stage (player, &tmp.x0, &tmp.y0);
  swfdec_player_global_to_stage (player, &tmp.x1, &tmp.y1);
  swfdec_rectangle_init_rect (&r, &tmp);
  /* FIXME: currently we clamp the rectangle to the visible area, it might
   * be useful to allow out-of-bounds drawing. In that case this needs to be
   * changed */
  swfdec_rectangle_intersect (&r, &r, &priv->stage);
  if (swfdec_rectangle_is_empty (&r))
    return;

  SWFDEC_LOG ("  invalidating %d %d  %d %d", r.x, r.y, r.width, r.height);
  /* FIXME: get region code into swfdec? */
  for (i = 0; i < priv->invalidations->len; i++) {
    SwfdecRectangle *cur = &g_array_index (priv->invalidations, SwfdecRectangle, i);
    if (swfdec_rectangle_contains (cur, &r))
      break;
    if (swfdec_rectangle_contains (&r, cur)) {
      *cur = r;
      swfdec_rectangle_union (&priv->invalid_extents, &priv->invalid_extents, &r);
    }
  }
  if (i == priv->invalidations->len) {
    g_array_append_val (priv->invalidations, r);
    swfdec_rectangle_union (&priv->invalid_extents, &priv->invalid_extents, &r);
  }
  SWFDEC_DEBUG ("toplevel invalidation of %g %g  %g %g - invalid region now %d %d  %d %d (%u subregions)",
      rect->x0, rect->y0, rect->x1, rect->y1,
      priv->invalid_extents.x, priv->invalid_extents.y, 
      priv->invalid_extents.x + priv->invalid_extents.width,
      priv->invalid_extents.y + priv->invalid_extents.height,
      priv->invalidations->len);
}

/**
 * swfdec_player_get_level:
 * @player: a #SwfdecPlayer
 * @name: a name that is supposed to refer to a level
 *
 * Checks if the given @name refers to a level, and if so, returns the level.
 * An example for such a name is "_level5". These strings are used to refer to
 * root movies inside the Flash player.
 *
 * Returns: the level referred to by @name or -1 if none
 **/
int
swfdec_player_get_level (SwfdecPlayer *player, const char *name)
{
  char *end;
  gulong l;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), -1);
  g_return_val_if_fail (name != NULL, -1);

  /* check name starts with "_level" */
  if (swfdec_strncmp (SWFDEC_AS_CONTEXT (player)->version, name, "_level", 6) != 0)
    return -1;
  name += 6;
  /* extract depth from rest string (or fail if it's not a depth) */
  errno = 0;
  l = strtoul (name, &end, 10);
  if (errno != 0 || *end != 0 || l > G_MAXINT)
    return -1;
  return l;
}

SwfdecSpriteMovie *
swfdec_player_create_movie_at_level (SwfdecPlayer *player, SwfdecResource *resource,
    int level)
{
  SwfdecMovie *movie;
  const char *s;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);
  g_return_val_if_fail (level >= 0, NULL);
  g_return_val_if_fail (swfdec_player_get_movie_at_level (player, level) == NULL, NULL);

  /* create new root movie */
  s = swfdec_as_context_give_string (SWFDEC_AS_CONTEXT (player), g_strdup_printf ("_level%d", level));
  movie = swfdec_movie_new (player, level - 16384, NULL, resource, NULL, s);
  if (movie == NULL)
    return NULL;
  movie->name = SWFDEC_AS_STR_EMPTY;
  return SWFDEC_SPRITE_MOVIE (movie);
}

/**
 * swfdec_player_get_movie_at_level:
 * @player: a #SwfdecPlayer
 * @level: number of the level
 *
 * This function is used to look up root movies in the given @player. 
 *
 * Returns: the #SwfdecMovie located at the given level or %NULL if there is no
 *          movie at that level.
 **/
SwfdecSpriteMovie *
swfdec_player_get_movie_at_level (SwfdecPlayer *player, int level)
{
  GList *walk;
  int depth;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);
  g_return_val_if_fail (level >= 0, NULL);

  depth = level - 16384;
  /* find movie */
  for (walk = player->priv->roots; walk; walk = walk->next) {
    SwfdecMovie *cur = walk->data;
    if (cur->depth < depth)
      continue;
    if (cur->depth == depth)
      return SWFDEC_SPRITE_MOVIE (cur);
    break;
  }
  return NULL;
}

void
swfdec_player_launch (SwfdecPlayer *player, SwfdecLoaderRequest request, const char *url, 
    const char *target, SwfdecBuffer *data)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (url != NULL);
  g_return_if_fail (target != NULL);

  if (!g_ascii_strncasecmp (url, "FSCommand:", strlen ("FSCommand:"))) {
    const char *command = url + strlen ("FSCommand:");
    g_signal_emit (player, signals[FSCOMMAND], 0, command, target);
    return;
  }
  g_signal_emit (player, signals[LAUNCH], 0, (int) request, url, target, data);
}

/**
 * swfdec_player_initialize:
 * @player: a #SwfdecPlayer
 * @version: Flash version to use
 * @rate: framerate in 256th or 0 for undefined
 * @width: width of movie
 * @height: height of movie
 *
 * Initializes the player to the given @version, @width, @height and @rate. If 
 * the player is already initialized, this function does nothing.
 **/
void
swfdec_player_initialize (SwfdecPlayer *player, guint version, 
    guint rate, guint width, guint height)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (rate > 0);

  priv = player->priv;
  if (!priv->initialized) {
    SwfdecAsContext *context = SWFDEC_AS_CONTEXT (player);
    swfdec_as_context_startup (context, version);
    /* reset state for initialization */
    /* FIXME: have a better way to do this */
    if (context->state == SWFDEC_AS_CONTEXT_RUNNING) {
      context->state = SWFDEC_AS_CONTEXT_NEW;
      swfdec_sprite_movie_init_context (player, version);
      swfdec_video_movie_init_context (player, version);
      swfdec_net_stream_init_context (player, version);

      swfdec_as_context_run_init_script (context, swfdec_initialize, 
	  sizeof (swfdec_initialize), context->version);

      if (context->state == SWFDEC_AS_CONTEXT_NEW) {
	context->state = SWFDEC_AS_CONTEXT_RUNNING;
	swfdec_as_object_set_constructor (priv->roots->data, priv->MovieClip);
      }
    }
    priv->initialized = TRUE;
    g_object_notify (G_OBJECT (player), "initialized");
  } else {
    /* FIXME: need to kick all other movies out here */
    swfdec_player_remove_timeout (player, &priv->iterate_timeout);
  }

  SWFDEC_INFO ("initializing player to size %ux%u and rate %u/256", width, height, rate);
  if (rate != priv->rate) {
    priv->rate = rate;
    g_object_notify (G_OBJECT (player), "rate");
  }
  if (priv->width != width) {
    priv->width = width;
    g_object_notify (G_OBJECT (player), "default-width");
  }
  if (priv->height != height) {
    priv->height = height;
    g_object_notify (G_OBJECT (player), "default-height");
  }
  priv->broadcasted_width = priv->internal_width = priv->stage_width >= 0 ? (guint) priv->stage_width : priv->width;
  priv->broadcasted_height = priv->internal_height = priv->stage_height >= 0 ? (guint) priv->stage_height : priv->height;
  swfdec_player_update_scale (player);

  priv->iterate_timeout.timestamp = priv->time + SWFDEC_TICKS_PER_SECOND * 256 / priv->rate / 10;
  swfdec_player_add_timeout (player, &priv->iterate_timeout);
  SWFDEC_LOG ("initialized iterate timeout %p to %"G_GUINT64_FORMAT" (now %"G_GUINT64_FORMAT")",
      &priv->iterate_timeout, priv->iterate_timeout.timestamp, priv->time);
}

/**
 * swfdec_player_get_export_class:
 * @player: a #SwfdecPlayer
 * @name: garbage-collected string naming the export
 *
 * Looks up the constructor for characters that are exported using @name.
 *
 * Returns: a #SwfdecAsObject naming the constructor or %NULL if none
 **/
SwfdecAsObject *
swfdec_player_get_export_class (SwfdecPlayer *player, const char *name)
{
  SwfdecPlayerPrivate *priv = player->priv;
  SwfdecAsObject *ret;
  
  ret = g_hash_table_lookup (priv->registered_classes, name);
  if (ret) {
    SWFDEC_LOG ("found registered class %p for %s", ret, name);
    return ret;
  }
  return priv->MovieClip;
}

/**
 * swfdec_player_set_export_class:
 * @player: a #SwfdecPlayer
 * @name: garbage-collected string naming the export
 * @object: object to use as constructor or %NULL for none
 *
 * Sets the constructor to be used for instances created using the object 
 * exported with @name.
 **/
void
swfdec_player_set_export_class (SwfdecPlayer *player, const char *name, SwfdecAsObject *object)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (name != NULL);
  g_return_if_fail (object == NULL || SWFDEC_IS_AS_OBJECT (object));

  priv = player->priv;
  if (object) {
    SWFDEC_LOG ("setting class %p for %s", object, name);
    g_hash_table_insert (priv->registered_classes, (gpointer) name, object);
  } else {
    g_hash_table_remove (priv->registered_classes, name);
  }
}

/* FIXME:
 * I don't like the idea of rooting arbitrary objects very much. And so far, 
 * this API is only necessary for the objects used for loading data. So it seems
 * like a good idea to revisit the refcounting and GCing of resources.
 */
void
swfdec_player_root_object (SwfdecPlayer *player, GObject *object)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (G_IS_OBJECT (object));

  priv = player->priv;
  g_object_ref (object);
  priv->rooted_objects = g_list_prepend (priv->rooted_objects, object);
}

void
swfdec_player_unroot_object (SwfdecPlayer *player, GObject *object)
{
  SwfdecPlayerPrivate *priv = player->priv;
  GList *entry;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (G_IS_OBJECT (object));
  entry = g_list_find (priv->rooted_objects, object);
  g_return_if_fail (entry != NULL);
  g_object_unref (object);
  priv->rooted_objects = g_list_delete_link (priv->rooted_objects, entry);
}

/**
 * swfdec_player_create_socket:
 * @player: a #SwfdecPlayer
 * @hostname: the host name to connect to.
 * @port: the port to connect to
 *
 * Creates a new socket connecting to the given hostname and port.
 *
 * Returns: a new socket or %NULL if no socket implementation exists
 **/
/* FIXME: always return a socket? */
SwfdecSocket *
swfdec_player_create_socket (SwfdecPlayer *player, const char *hostname, guint port)
{
  SwfdecSocket *sock;
  SwfdecSocketClass *klass;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);
  g_return_val_if_fail (hostname != NULL, NULL);
  g_return_val_if_fail (port > 0, NULL);

  if (player->priv->socket_type == 0)
    return NULL;
  klass = g_type_class_ref (player->priv->socket_type);
  sock = klass->create (hostname, port);
  g_type_class_unref (klass);

  return sock;
}

/** PUBLIC API ***/

/**
 * swfdec_player_new:
 * @debugger: %NULL or a #SwfdecAsDebugger to use for debugging this player.
 *
 * Creates a new player. This function is supposed to be used for testing.
 * Because of this, the created player will behave as predictable as possible.
 * For example, it will generate the same random number sequence every time.
 * The function calls swfdec_init () for you if it wasn't called before.
 *
 * Returns: The new player
 **/
SwfdecPlayer *
swfdec_player_new (SwfdecAsDebugger *debugger)
{
  static const GTimeVal the_beginning = { 1035840244, 0 };
  SwfdecPlayer *player;

  g_return_val_if_fail (debugger == NULL || SWFDEC_IS_AS_DEBUGGER (debugger), NULL);

  swfdec_init ();
  player = g_object_new (SWFDEC_TYPE_PLAYER, "random-seed", 0,
      "max-runtime", 0, 
      "debugger", debugger, NULL);
  /* FIXME: make this a property or something and don't set it here */
  SWFDEC_AS_CONTEXT (player)->start_time = the_beginning;

  return player;
}

/**
 * swfdec_player_set_loader:
 * @player: a #SwfdecPlayer
 * @loader: the loader to use for this player. Takes ownership of the given loader.
 *
 * Sets the loader for the main data. This function only works if no loader has 
 * been set on @player yet.
 * For details, see swfdec_player_set_loader_with_variables().
 **/
void
swfdec_player_set_loader (SwfdecPlayer *player, SwfdecLoader *loader)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (player->priv->roots == NULL);
  g_return_if_fail (SWFDEC_IS_LOADER (loader));

  swfdec_player_set_loader_with_variables (player, loader, NULL);
}

/**
 * swfdec_player_set_loader_with_variables:
 * @player: a #SwfdecPlayer
 * @loader: the loader to use for this player. Takes ownership of the given loader.
 * @variables: a string that is checked to be in 'application/x-www-form-urlencoded'
 *             syntax describing the arguments to set on the new player or NULL for
 *             none.
 *
 * Sets the loader for the main data. This function only works if no loader has 
 * been set on @player yet.
 * If the @variables are set and validate, they will be set as properties on the 
 * root movie. 
 **/
void
swfdec_player_set_loader_with_variables (SwfdecPlayer *player, SwfdecLoader *loader,
    const char *variables)
{
  SwfdecPlayerPrivate *priv;
  SwfdecMovie *movie;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (player->priv->resource == NULL);
  g_return_if_fail (SWFDEC_IS_LOADER (loader));

  priv = player->priv;
  priv->resource = swfdec_resource_new (player, loader, variables);
  movie = swfdec_movie_new (player, -16384, NULL, priv->resource, NULL, SWFDEC_AS_STR__level0);
  movie->name = SWFDEC_AS_STR_EMPTY;
  g_object_unref (loader);
}

/**
 * swfdec_player_new_from_file:
 * @filename: name of the file to play
 *
 * Creates a player to play back the given file. If the file does not
 * exist or another error occurs, the player will be in an error state and not
 * be initialized.
 * This function calls swfdec_init () for you if it wasn't called before.
 *
 * Returns: a new player
 **/
SwfdecPlayer *
swfdec_player_new_from_file (const char *filename)
{
  SwfdecLoader *loader;
  SwfdecPlayer *player;

  g_return_val_if_fail (filename != NULL, NULL);

  loader = swfdec_file_loader_new (filename);
  player = swfdec_player_new (NULL);
  swfdec_player_set_loader (player, loader);

  return player;
}

/**
 * swfdec_init:
 *
 * Initializes the Swfdec library.
 **/
void
swfdec_init (void)
{
  static gboolean _inited = FALSE;
  const char *s;

  if (_inited)
    return;

  _inited = TRUE;

  if (!g_thread_supported ())
    g_thread_init (NULL);
  g_type_init ();
  oil_init ();

  s = g_getenv ("SWFDEC_DEBUG");
  if (s && s[0]) {
    char *end;
    int level;

    level = strtoul (s, &end, 0);
    if (end[0] == 0) {
      swfdec_debug_set_level (level);
    }
  }
}

/**
 * swfdec_player_mouse_move:
 * @player: a #SwfdecPlayer
 * @x: x coordinate of mouse
 * @y: y coordinate of mouse
 *
 * Updates the current mouse position. If the mouse has left the area of @player,
 * you should pass values outside the movie size for @x and @y. You will 
 * probably want to call swfdec_player_advance() before to update the player to
 * the correct time when calling this function.
 *
 * Returns: %TRUE if the mouse event was handled. %FALSE if the event should be
 *	    propagated further. A mouse event may not be handled if the user 
 *	    clicked on a translucent area.
 **/
gboolean
swfdec_player_mouse_move (SwfdecPlayer *player, double x, double y)
{
  gboolean ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);

  g_signal_emit (player, signals[HANDLE_MOUSE], 0, x, y, 0, &ret);

  return ret;
}

/**
 * swfdec_player_mouse_press:
 * @player: a #SwfdecPlayer
 * @x: x coordinate of mouse
 * @y: y coordinate of mouse
 * @button: number of the button that was pressed. Swfdec supports up to 32
 *          buttons.
 *
 * Tells the @player that the mouse button @button was pressed at the given
 * coordinate.
 *
 * Returns: %TRUE if the mouse event was handled. %FALSE if the event should be
 *	    propagated further. A mouse event may not be handled if the user 
 *	    clicked on a translucent area.
 **/
gboolean
swfdec_player_mouse_press (SwfdecPlayer *player, double x, double y, 
    guint button)
{
  gboolean ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);
  g_return_val_if_fail (button > 0 && button <= 32, FALSE);

  g_signal_emit (player, signals[HANDLE_MOUSE], 0, x, y, button, &ret);

  return ret;
}

/**
 * swfdec_player_mouse_release:
 * @player: a #SwfdecPlayer
 * @x: x coordinate of mouse
 * @y: y coordinate of mouse
 * @button: number of the button that was released. Swfdec supports up to 32
 *          buttons.
 *
 * Tells the @player that the mouse button @button was released at the given
 * coordinate.
 *
 * Returns: %TRUE if the mouse event was handled. %FALSE if the event should be
 *	    propagated further. A mouse event may not be handled if the user 
 *	    clicked on a translucent area.
 **/
gboolean
swfdec_player_mouse_release (SwfdecPlayer *player, double x, double y, 
    guint button)
{
  gboolean ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);
  g_return_val_if_fail (button > 0 && button <= 32, FALSE);

  g_signal_emit (player, signals[HANDLE_MOUSE], 0, x, y, -button, &ret);

  return ret;
}

/**
 * swfdec_player_key_press:
 * @player: a #SwfdecPlayer
 * @keycode: the key that was pressed
 * @character: UCS4 of the character that was inserted or 0 if none
 *
 * Call this function to make the @player react to a key press. Be sure to
 * check that keycode transformations are done correctly. For a list of 
 * keycodes see FIXME.
 *
 * Returns: %TRUE if the key press was handled by the @player, %FALSE if it
 *          should be propagated further
 **/
gboolean
swfdec_player_key_press (SwfdecPlayer *player, guint keycode, guint character)
{
  gboolean ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);
  g_return_val_if_fail (keycode < 256, FALSE);

  g_signal_emit (player, signals[HANDLE_KEY], 0, keycode, character, TRUE, &ret);

  return ret;
}

/**
 * swfdec_player_key_release:
 * @player: a #SwfdecPlayer
 * @keycode: the key that was released
 * @character: UCS4 of the character that was inserted or 0 if none
 *
 * Call this function to make the @player react to a key being released. See
 * swfdec_player_key_press() for details.
 *
 * Returns: %TRUE if the key press was handled by the @player, %FALSE if it
 *          should be propagated further
 **/
gboolean
swfdec_player_key_release (SwfdecPlayer *player, guint keycode, guint character)
{
  gboolean ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);
  g_return_val_if_fail (keycode < 256, FALSE);

  g_signal_emit (player, signals[HANDLE_KEY], 0, keycode, character, FALSE, &ret);

  return ret;
}

/**
 * swfdec_player_render:
 * @player: a #SwfdecPlayer
 * @cr: #cairo_t to render to
 * @x: x coordinate of top left position to render
 * @y: y coordinate of top left position to render
 * @width: width of area to render or 0 for full width
 * @height: height of area to render or 0 for full height
 *
 * Renders the given area of the current frame to @cr.
 **/
void
swfdec_player_render (SwfdecPlayer *player, cairo_t *cr, 
    double x, double y, double width, double height)
{
  static const SwfdecColorTransform trans = { FALSE, 256, 0, 256, 0, 256, 0, 256, 0 };
  SwfdecPlayerPrivate *priv;
  GList *walk;
  SwfdecRect real;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0.0);
  g_return_if_fail (height >= 0.0);

  /* FIXME: fail when !initialized? */
  if (!swfdec_player_is_initialized (player))
    return;

  priv = player->priv;
  if (width == 0.0)
    width = priv->stage_width;
  if (height == 0.0)
    height = priv->stage_height;
  /* clip the area */
  cairo_save (cr);
  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);
  /* compute the rectangle */
  x -= priv->offset_x;
  y -= priv->offset_y;
  real.x0 = floor (x * SWFDEC_TWIPS_SCALE_FACTOR) / priv->scale_x;
  real.y0 = floor (y * SWFDEC_TWIPS_SCALE_FACTOR) / priv->scale_y;
  real.x1 = ceil ((x + width) * SWFDEC_TWIPS_SCALE_FACTOR) / priv->scale_x;
  real.y1 = ceil ((y + height) * SWFDEC_TWIPS_SCALE_FACTOR) / priv->scale_y;
  SWFDEC_INFO ("=== %p: START RENDER, area %g %g  %g %g ===", player, 
      real.x0, real.y0, real.x1, real.y1);
  /* convert the cairo matrix */
  cairo_translate (cr, priv->offset_x, priv->offset_y);
  cairo_scale (cr, priv->scale_x / SWFDEC_TWIPS_SCALE_FACTOR, priv->scale_y / SWFDEC_TWIPS_SCALE_FACTOR);
  swfdec_color_set_source (cr, priv->bgcolor);
  cairo_paint (cr);

  for (walk = priv->roots; walk; walk = walk->next) {
    swfdec_movie_render (walk->data, cr, &trans, &real);
  }
  SWFDEC_INFO ("=== %p: END RENDER ===", player);
  cairo_restore (cr);
}

/**
 * swfdec_player_advance:
 * @player: the #SwfdecPlayer to advance
 * @msecs: number of milliseconds to advance
 *
 * Advances @player by @msecs. You should make sure to call this function as
 * often as the SwfdecPlayer::next-event property indicates.
 **/
void
swfdec_player_advance (SwfdecPlayer *player, gulong msecs)
{
  SwfdecPlayerPrivate *priv;
  guint frames;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  priv = player->priv;
  frames = SWFDEC_TICKS_TO_SAMPLES (priv->time + SWFDEC_MSECS_TO_TICKS (msecs))
    - SWFDEC_TICKS_TO_SAMPLES (priv->time);
  g_signal_emit (player, signals[ADVANCE], 0, msecs, frames);
}

/**
 * swfdec_player_is_initialized:
 * @player: a #SwfdecPlayer
 *
 * Determines if the @player is initalized yet. An initialized player is able
 * to provide basic values like width, height or rate. A player may not be 
 * initialized if the loader it was started with does not reference a Flash
 * resources or it did not provide enough data yet. If a player is initialized,
 * it will never be uninitialized again.
 *
 * Returns: TRUE if the basic values are known.
 **/
gboolean
swfdec_player_is_initialized (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), FALSE);

  return player->priv->initialized;
}

/**
 * swfdec_player_get_next_event:
 * @player: ia #SwfdecPlayer
 *
 * Queries how long to the next event. This is the next time when you should 
 * call swfdec_player_advance() to forward to.
 *
 * Returns: number of milliseconds until next event or -1 if no outstanding event
 **/
glong
swfdec_player_get_next_event (SwfdecPlayer *player)
{
  SwfdecTick tick;
  guint ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), 0);

  if (swfdec_as_context_is_aborted (SWFDEC_AS_CONTEXT (player)))
    return -1;

  tick = swfdec_player_get_next_event_time (player);
  if (tick == G_MAXUINT64)
    return -1;
  /* round up to full msecs */
  ret = SWFDEC_TICKS_TO_MSECS (tick + SWFDEC_TICKS_PER_SECOND / 1000 - 1); 

  return ret;
}

/**
 * swfdec_player_get_rate:
 * @player: a #SwfdecPlayer
 *
 * Queries the framerate of this movie. This number specifies the number
 * of frames that are supposed to pass per second. It is a 
 * multiple of 1/256. It is possible that the movie has no framerate if it does
 * not display a Flash movie but an FLV video for example. This does not mean
 * it will not change however.
 *
 * Returns: The framerate of this movie or 0 if it isn't known yet or the
 *          movie doesn't have a framerate.
 **/
double
swfdec_player_get_rate (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), 0.0);

  return player->priv->rate / 256.0;
}

/**
 * swfdec_player_get_default_size:
 * @player: a #SwfdecPlayer
 * @width: integer to store the width in or %NULL
 * @height: integer to store the height in or %NULL
 *
 * If the default size of the movie is initialized, fills in @width and @height 
 * with the size. Otherwise @width and @height are set to 0.
 **/
void
swfdec_player_get_default_size (SwfdecPlayer *player, guint *width, guint *height)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  if (width)
    *width = player->priv->width;
  if (height)
    *height = player->priv->height;
}

/**
 * swfdec_player_get_size:
 * @player: a #SwfdecPlayer
 * @width: integer to store the width in or %NULL
 * @height: integer to store the height in or %NULL
 *
 * Gets the currently set image size. If the default width or height should be 
 * used, the width or height respectively is set to -1.
 **/
void
swfdec_player_get_size (SwfdecPlayer *player, int *width, int *height)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  if (width)
    *width = player->priv->stage_width;
  if (height)
    *height = player->priv->stage_height;
}

static void
swfdec_player_update_size (gpointer playerp, gpointer unused)
{
  SwfdecPlayer *player = playerp;
  SwfdecPlayerPrivate *priv = player->priv;

  /* FIXME: only update if not fullscreen */
  priv->internal_width = priv->stage_width >=0 ? (guint) priv->stage_width : priv->width;
  priv->internal_height = priv->stage_height >=0 ? (guint) priv->stage_height : priv->height;

  if (priv->scale_mode != SWFDEC_SCALE_NONE)
    return;

  /* only broadcast once */
  if (priv->internal_width == priv->broadcasted_width &&
      priv->internal_height == priv->broadcasted_height)
    return;

  priv->broadcasted_width = priv->internal_width;
  priv->broadcasted_height = priv->internal_height;
  swfdec_player_broadcast (player, SWFDEC_AS_STR_Stage, SWFDEC_AS_STR_onResize);
}

/**
 * swfdec_player_set_size:
 * @player: a #SwfdecPlayer
 * @width: desired width of the movie or -1 for default
 * @height: desired height of the movie or -1 for default
 *
 * Sets the image size to the given values. The image size is what the area that
 * the @player will render and advocate with scripts.
 **/
void
swfdec_player_set_size (SwfdecPlayer *player, int width, int height)
{
  SwfdecPlayerPrivate *priv;
  gboolean changed = FALSE;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  priv = player->priv;
  if (priv->stage_width != width) {
    priv->stage_width = width;
    g_object_notify (G_OBJECT (player), "width");
    changed = TRUE;
  }
  if (priv->stage_height != height) {
    priv->stage_height = height;
    g_object_notify (G_OBJECT (player), "height");
    changed = TRUE;
  }
  swfdec_player_update_scale (player);
  if (changed)
    swfdec_player_add_external_action (player, player, swfdec_player_update_size, NULL);
}

/**
 * swfdec_player_get_audio:
 * @player: a #SwfdecPlayer
 *
 * Returns a list of all currently active audio streams in @player.
 *
 * Returns: A #GList of #SwfdecAudio. You must not modify or free this list.
 **/
/* FIXME: I don't like this function */
const GList *
swfdec_player_get_audio (SwfdecPlayer *	player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);

  return player->priv->audio;
}

/**
 * swfdec_player_get_background_color:
 * @player: a #SwfdecPlayer
 *
 * Gets the current background color. The color will be an ARGB-quad, with the 
 * MSB being the alpha value.
 *
 * Returns: the background color as an ARGB value
 **/
guint
swfdec_player_get_background_color (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), SWFDEC_COLOR_COMBINE (0xFF, 0xFF, 0xFF, 0xFF));

  return player->priv->bgcolor;
}

/**
 * swfdec_player_set_background_color:
 * @player: a #SwfdecPlayer
 * @color: new color to use as background color
 *
 * Sets a new background color as an ARGB value. To get transparency, set the 
 * value to 0. To get a black beackground, use 0xFF000000.
 **/
void
swfdec_player_set_background_color (SwfdecPlayer *player, guint color)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  priv = player->priv;
  priv->bgcolor_set = TRUE;
  if (priv->bgcolor == color)
    return;
  priv->bgcolor = color;
  g_object_notify (G_OBJECT (player), "background-color");
  if (swfdec_player_is_initialized (player)) {
    g_signal_emit (player, signals[INVALIDATE], 0, 0.0, 0.0, 
	(double) priv->width, (double) priv->height);
  }
}

/**
 * swfdec_player_get_scale_mode:
 * @player: a #SwfdecPlayer
 *
 * Gets the currrent mode used for scaling the movie. See #SwfdecScaleMode for 
 * the different modes.
 *
 * Returns: the current scale mode
 **/
SwfdecScaleMode
swfdec_player_get_scale_mode (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), SWFDEC_SCALE_SHOW_ALL);

  return player->priv->scale_mode;
}

/**
 * swfdec_player_set_scale_mode:
 * @player: a #SwfdecPlayer
 * @mode: a #SwfdecScaleMode
 *
 * Sets the currrent mode used for scaling the movie. See #SwfdecScaleMode for 
 * the different modes.
 **/
void
swfdec_player_set_scale_mode (SwfdecPlayer *player, SwfdecScaleMode mode)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  priv = player->priv;
  if (priv->scale_mode != mode) {
    priv->scale_mode = mode;
    swfdec_player_update_scale (player);
    g_object_notify (G_OBJECT (player), "scale-mode");
    swfdec_player_add_external_action (player, player, swfdec_player_update_size, NULL);
  }
}

/**
 * swfdec_player_get_alignment:
 * @player: a #SwfdecPlayer
 *
 * Gets the alignment of the player. The alignment describes what point is used
 * as the anchor for drawing the contents. See #SwfdecAlignment for possible 
 * values.
 *
 * Returns: the current alignment
 **/
SwfdecAlignment
swfdec_player_get_alignment (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), SWFDEC_ALIGNMENT_CENTER);

  return swfdec_player_alignment_from_flags (player->priv->align_flags);
}

/**
 * swfdec_player_set_alignment:
 * @player: a #SwfdecPlayer
 * @align: #SwfdecAlignment to set
 *
 * Sets the alignment to @align. For details about alignment, see 
 * swfdec_player_get_alignment() and #SwfdecAlignment.
 **/
void
swfdec_player_set_alignment (SwfdecPlayer *player, SwfdecAlignment align)
{
  guint flags;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  flags = swfdec_player_alignment_to_flags (align);
  swfdec_player_set_align_flags (player, flags);
}

void
swfdec_player_set_align_flags (SwfdecPlayer *player, guint flags)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  priv = player->priv;
  if (flags != priv->align_flags) {
    priv->align_flags = flags;
    swfdec_player_update_scale (player);
    g_object_notify (G_OBJECT (player), "alignment");
  }
}

/**
 * swfdec_player_get_maximum_runtime:
 * @player: a #SwfdecPlayer
 *
 * Queries the given @player for how long scripts may run. see 
 * swfdec_player_set_maximum_runtime() for a longer discussion of this value.
 *
 * Returns: the maximum time in milliseconds that scripts are allowed to run or
 *          0 for infinite.
 **/
gulong
swfdec_player_get_maximum_runtime (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), 0);

  return player->priv->max_runtime;
}

/**
 * swfdec_player_set_maximum_runtime:
 * @player: a #SwfdecPlayer
 * @msecs: time in milliseconds that scripts are allowed to run or 0 for 
 *         infinite
 *
 * Sets the time that the player may use to let internal scripts run. If the 
 * Flash file that is currently played back does not manage to complete its 
 * scripts in the given time, it is aborted. You cannot continue the scripts at
 * a later point in time. However, your application may become unresponsive and
 * your users annoyed if they cannot interact with it for too long. To give a 
 * reference point, the Adobe Flash player usually sets this value to 10 
 * seconds. Note that this time determines the maximum time calling 
 * swfdec_player_advance() may take, even if it is called with a large value.
 * Also note that this setting is ignored when running inside a debugger.
 **/
void
swfdec_player_set_maximum_runtime (SwfdecPlayer *player, gulong msecs)
{
  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  player->priv->max_runtime = msecs;
  g_object_notify (G_OBJECT (player), "max-runtime");
}

/**
 * swfdec_player_get_scripting:
 * @player: a #SwfdecPlayer
 *
 * Gets the current scripting implementation in use. If no implementation is in 
 * use (the default), %NULL is returned.
 *
 * Returns: the current scripting implementation used or %NULL if none
 **/
SwfdecPlayerScripting *
swfdec_player_get_scripting (SwfdecPlayer *player)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);

  return player->priv->scripting;
}

/**
 * swfdec_player_set_scripting:
 * @player: a #SwfdecPlayer
 * @scripting: the scripting implementation to use or %NULL to disable scripting
 *
 * Sets the implementation to use for external scripting in the given @player.
 * Note that this is different from the internal script engine. See the 
 * #SwfdecPlayerScripting paragraph for details about external scripting.
 **/
void
swfdec_player_set_scripting (SwfdecPlayer *player, SwfdecPlayerScripting *scripting)
{
  SwfdecPlayerPrivate *priv;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));
  g_return_if_fail (scripting == NULL || SWFDEC_IS_PLAYER_SCRIPTING (scripting));

  priv = player->priv;
  if (priv->scripting == scripting)
    return;

  if (priv->scripting)
    g_object_unref (priv->scripting);
  priv->scripting = g_object_ref (scripting);
  g_object_notify (G_OBJECT (player), "scripting");
}
