/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstbasetransform.c: 
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


#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "../gst-i18n-lib.h"
#include "gstbasetransform.h"
#include <gst/gstmarshal.h>

GST_DEBUG_CATEGORY_STATIC (gst_base_transform_debug);
#define GST_CAT_DEFAULT gst_base_transform_debug

/* BaseTransform signals and args */
enum
{
  SIGNAL_HANDOFF,
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

static GstElementClass *parent_class = NULL;

static void gst_base_transform_base_init (gpointer g_class);
static void gst_base_transform_class_init (GstBaseTransformClass * klass);
static void gst_base_transform_init (GstBaseTransform * trans,
    gpointer g_class);

GType
gst_base_transform_get_type (void)
{
  static GType base_transform_type = 0;

  if (!base_transform_type) {
    static const GTypeInfo base_transform_info = {
      sizeof (GstBaseTransformClass),
      (GBaseInitFunc) gst_base_transform_base_init,
      NULL,
      (GClassInitFunc) gst_base_transform_class_init,
      NULL,
      NULL,
      sizeof (GstBaseTransform),
      0,
      (GInstanceInitFunc) gst_base_transform_init,
    };

    base_transform_type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstBaseTransform", &base_transform_info, G_TYPE_FLAG_ABSTRACT);
  }
  return base_transform_type;
}

static void gst_base_transform_finalize (GObject * object);
static void gst_base_transform_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_base_transform_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_base_transform_src_activate (GstPad * pad,
    GstActivateMode mode);
static gboolean gst_base_transform_sink_activate (GstPad * pad,
    GstActivateMode mode);
static GstElementStateReturn gst_base_transform_change_state (GstElement *
    element);

static gboolean gst_base_transform_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_base_transform_getrange (GstPad * pad, guint64 offset,
    guint length, GstBuffer ** buffer);
static GstFlowReturn gst_base_transform_chain (GstPad * pad,
    GstBuffer * buffer);
static GstFlowReturn gst_base_transform_handle_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf);
static GstCaps *gst_base_transform_proxy_getcaps (GstPad * pad);
static gboolean gst_base_transform_setcaps (GstPad * pad, GstCaps * caps);

/* static guint gst_base_transform_signals[LAST_SIGNAL] = { 0 }; */

static void
gst_base_transform_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_base_transform_debug, "basetransform", 0,
      "basetransform element");
}

static void
gst_base_transform_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_base_transform_class_init (GstBaseTransformClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_base_transform_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_base_transform_get_property);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_base_transform_finalize);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_base_transform_change_state);
}

static void
gst_base_transform_init (GstBaseTransform * trans, gpointer g_class)
{
  GstPadTemplate *pad_template;

  GST_DEBUG ("gst_base_transform_init");

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (g_class), "sink");
  g_return_if_fail (pad_template != NULL);
  trans->sinkpad = gst_pad_new_from_template (pad_template, "sink");
  gst_pad_set_getcaps_function (trans->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_proxy_getcaps));
  gst_pad_set_setcaps_function (trans->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_setcaps));
  gst_pad_set_event_function (trans->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_event));
  gst_pad_set_chain_function (trans->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_chain));
  gst_pad_set_activate_function (trans->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_sink_activate));
  gst_element_add_pad (GST_ELEMENT (trans), trans->sinkpad);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (g_class), "src");
  g_return_if_fail (pad_template != NULL);
  trans->srcpad = gst_pad_new_from_template (pad_template, "src");
  gst_pad_set_getcaps_function (trans->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_proxy_getcaps));
  gst_pad_set_getrange_function (trans->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_getrange));
  gst_pad_set_activate_function (trans->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_transform_src_activate));
  gst_element_add_pad (GST_ELEMENT (trans), trans->srcpad);
}

static GstCaps *
gst_base_transform_proxy_getcaps (GstPad * pad)
{
  GstPad *otherpad;
  GstBaseTransform *trans = GST_BASE_TRANSFORM (GST_OBJECT_PARENT (pad));

  otherpad = pad == trans->srcpad ? trans->sinkpad : trans->srcpad;

  return gst_pad_peer_get_caps (otherpad);
}

static gboolean
gst_base_transform_setcaps (GstPad * pad, GstCaps * caps)
{
  GstBaseTransform *trans;
  GstBaseTransformClass *bclass;
  gboolean result = TRUE;

  trans = GST_BASE_TRANSFORM (GST_PAD_PARENT (pad));
  bclass = GST_BASE_TRANSFORM_GET_CLASS (trans);

  if (bclass->set_caps)
    result = bclass->set_caps (trans, caps);

  return result;
}

static gboolean
gst_base_transform_event (GstPad * pad, GstEvent * event)
{
  GstBaseTransform *trans;
  GstBaseTransformClass *bclass;
  gboolean ret = FALSE;

  trans = GST_BASE_TRANSFORM (GST_PAD_PARENT (pad));
  bclass = GST_BASE_TRANSFORM_GET_CLASS (trans);

  if (bclass->event)
    bclass->event (trans, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH:
      if (GST_EVENT_FLUSH_DONE (event)) {
      }
      GST_STREAM_LOCK (pad);
      break;
    case GST_EVENT_EOS:
      GST_STREAM_LOCK (pad);
      break;
    default:
      break;
  }
  ret = gst_pad_event_default (pad, event);
  GST_STREAM_UNLOCK (pad);

  return ret;
}

static GstFlowReturn
gst_base_transform_getrange (GstPad * pad, guint64 offset,
    guint length, GstBuffer ** buffer)
{
  GstBaseTransform *trans;
  GstFlowReturn ret;
  GstBuffer *inbuf;

  trans = GST_BASE_TRANSFORM (GST_PAD_PARENT (pad));

  GST_STREAM_LOCK (pad);

  ret = gst_pad_pull_range (trans->sinkpad, offset, length, &inbuf);
  if (ret == GST_FLOW_OK) {
    ret = gst_base_transform_handle_buffer (trans, inbuf, buffer);
  }

  GST_STREAM_UNLOCK (pad);

  return ret;
}

static GstFlowReturn
gst_base_transform_chain (GstPad * pad, GstBuffer * buffer)
{
  GstBaseTransform *trans;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *outbuf;

  trans = GST_BASE_TRANSFORM (GST_PAD_PARENT (pad));

  GST_STREAM_LOCK (pad);

  ret = gst_base_transform_handle_buffer (trans, buffer, &outbuf);
  if (ret == GST_FLOW_OK) {
    ret = gst_pad_push (trans->srcpad, outbuf);
  }

  GST_STREAM_UNLOCK (pad);

  return ret;
}

static GstFlowReturn
gst_base_transform_handle_buffer (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer ** outbuf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBaseTransformClass *bclass;

  bclass = GST_BASE_TRANSFORM_GET_CLASS (trans);
  if (bclass->transform)
    ret = bclass->transform (trans, inbuf, outbuf);

  gst_buffer_unref (inbuf);

  return ret;
}

static void
gst_base_transform_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseTransform *trans;

  trans = GST_BASE_TRANSFORM (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_base_transform_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseTransform *trans;

  trans = GST_BASE_TRANSFORM (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_base_transform_sink_activate (GstPad * pad, GstActivateMode mode)
{
  gboolean result = TRUE;
  GstBaseTransform *trans;
  GstBaseTransformClass *bclass;

  trans = GST_BASE_TRANSFORM (GST_OBJECT_PARENT (pad));
  bclass = GST_BASE_TRANSFORM_GET_CLASS (trans);

  switch (mode) {
    case GST_ACTIVATE_PUSH:
    case GST_ACTIVATE_PULL:
      if (bclass->start)
        result = bclass->start (trans);
      break;
    case GST_ACTIVATE_NONE:
      break;
  }

  return result;
}

static gboolean
gst_base_transform_src_activate (GstPad * pad, GstActivateMode mode)
{
  gboolean result = FALSE;
  GstBaseTransform *trans;

  trans = GST_BASE_TRANSFORM (GST_OBJECT_PARENT (pad));

  switch (mode) {
    case GST_ACTIVATE_PUSH:
      result = TRUE;
      break;
    case GST_ACTIVATE_PULL:
      result = gst_pad_set_active (trans->sinkpad, mode);
      result = gst_pad_peer_set_active (trans->sinkpad, mode);
      break;
    case GST_ACTIVATE_NONE:
      result = TRUE;
      break;
  }
  return result;
}

static GstElementStateReturn
gst_base_transform_change_state (GstElement * element)
{
  GstBaseTransform *trans;
  GstBaseTransformClass *bclass;
  GstElementState transition;
  GstElementStateReturn result;

  trans = GST_BASE_TRANSFORM (element);
  bclass = GST_BASE_TRANSFORM_GET_CLASS (trans);

  transition = GST_STATE_TRANSITION (element);

  switch (transition) {
    case GST_STATE_NULL_TO_READY:
      break;
    case GST_STATE_READY_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element);

  switch (transition) {
    case GST_STATE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_READY:
      if (bclass->stop)
        result = bclass->stop (trans);
      break;
    case GST_STATE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return result;
}