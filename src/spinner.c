/*
 * Copyright (C) 2025 The Phosh Developers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "phoc-spinner"

#include "spinner.h"
#include "timed-animation.h"

enum {
  PROP_0,
  PROP_CX,
  PROP_CY,
  PROP_ANIMATABLE,
  PROP_ROTATION,
  PROP_LAST_PROP
};

static GParamSpec *props[PROP_LAST_PROP];

#define PHOC_ANIM_DURATION_SPIN_MS 750 /* ms */

/**
 * PhocSpinner:
 *
 * An animated spinner, used to represent indeterminate progress.
 */
struct _PhocSpinner {
  GObject parent;

  gint                cx;
  gint                cy;
  PhocAnimatable     *animatable;
  PhocTimedAnimation *animation;
  PhocPropertyEaser  *easer;
  gfloat              rotation;
};

G_DEFINE_TYPE (PhocSpinner, phoc_spinner, G_TYPE_OBJECT)


static void
on_animation_done (PhocSpinner *self)
{
  /* animation cycles indefinitely */
  phoc_timed_animation_play (self->animation);
}


static void
phoc_spinner_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  PhocSpinner *self = PHOC_SPINNER (object);

  switch (property_id) {
  case PROP_CX:
    self->cx = g_value_get_int (value);
    break;
  case PROP_CY:
    self->cy = g_value_get_int (value);
    break;
  case PROP_ANIMATABLE:
    self->animatable = g_value_get_object (value);
    break;
  case PROP_ROTATION:
    self->rotation = g_value_get_float (value);
    /* easing method might overshoot the 0-360 degree range we want. */
    while (self->rotation < 0.0f || self->rotation >= 360.0f) {
      if (self->rotation < 0.0)
        self->rotation = 360.0f - self->rotation;
      else
        self->rotation = self->rotation - 359.0f;
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
phoc_spinner_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  PhocSpinner *self = PHOC_SPINNER (object);

  switch (property_id) {
  case PROP_CX:
    g_value_set_int (value, self->cx);
    break;
  case PROP_CY:
    g_value_set_int (value, self->cy);
    break;
  case PROP_ANIMATABLE:
    g_value_set_object (value, self->animatable);
    break;
  case PROP_ROTATION:
    g_value_set_float (value, self->rotation);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
phoc_spinner_finalize (GObject *object)
{
  PhocSpinner *self = PHOC_SPINNER (object);

  g_clear_object (&self->easer);
  phoc_timed_animation_reset (self->animation);
  g_clear_object (&self->animation);

  G_OBJECT_CLASS (phoc_spinner_parent_class)->finalize (object);
}


static void
phoc_spinner_constructed (GObject *obj)
{
  PhocSpinner *self = PHOC_SPINNER (obj);

  self->animation = g_object_new (PHOC_TYPE_TIMED_ANIMATION,
                                  "animatable", self->animatable,
                                  "duration", PHOC_ANIM_DURATION_SPIN_MS,
                                  "property-easer", self->easer,
                                  NULL);
  g_signal_connect_swapped (self->animation, "done",
                            G_CALLBACK (on_animation_done),
                            self);
}


static void
phoc_spinner_class_init (PhocSpinnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = phoc_spinner_constructed;
  object_class->get_property = phoc_spinner_get_property;
  object_class->set_property = phoc_spinner_set_property;
  object_class->finalize = phoc_spinner_finalize;

  /**
   * PhocSpinner:animatable:
   *
   * A [type@Animatable] implementation that can drive this spinner's animation
   */
  props[PROP_ANIMATABLE] =
    g_param_spec_object ("animatable", "", "",
                         PHOC_TYPE_ANIMATABLE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PhocSpinner:cx:
   *
   * The center x coord to render spinner at
  */
  props[PROP_CX] =
    g_param_spec_int ("cx", "", "",
                      0, INT32_MAX, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PhocSpinner:cy:
   *
   * The center y coord to render spinner at
  */
  props[PROP_CY] =
    g_param_spec_int ("cy", "", "",
                      0, INT32_MAX, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PhocSpinner:rotation:
   *
   * The current rotation of the spinner.
   */
  props[PROP_ROTATION] =
    g_param_spec_float ("rotation", "", "",
                        -999.0f,
                        999.0f,
                        0.0f,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
phoc_spinner_init (PhocSpinner *self)
{
  self->easer = g_object_new (PHOC_TYPE_PROPERTY_EASER,
                              "target", self,
                              "easing", PHOC_EASING_EASE_IN_OUT_BACK,
                              NULL);
  phoc_property_easer_set_props (self->easer,
                                 "rotation", 0.0, 359.0,
                                 NULL);
}


PhocSpinner*
phoc_spinner_new (PhocAnimatable *animatable, gint cx, gint cy)
{
  return g_object_new (PHOC_TYPE_SPINNER,
                       "animatable", animatable,
                       "cx", cx,
                       "cy", cy,
                       NULL);
}
