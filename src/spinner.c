/*
 * Copyright (C) 2025 The Phosh Developers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "phoc-spinner"

#include "bling.h"
#include "server.h"
#include "spinner.h"
#include "timed-animation.h"

#include "render-private.h"

#include <cairo.h>
#include <drm_fourcc.h>

G_DEFINE_AUTOPTR_CLEANUP_FUNC (cairo_t, cairo_destroy)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (cairo_surface_t, cairo_surface_destroy)

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

extern char _binary_src_spinner_png_start;
extern char _binary_src_spinner_png_end;

struct read_spinner_progress {
  uint read;
};

static cairo_status_t
read_spinner (void          *closure,
              unsigned char *data,
              unsigned int   length)
{
  struct read_spinner_progress *progress = closure;
  char *src = &_binary_src_spinner_png_start + progress->read;
  progress->read += length;

  while (length > 0 && src != &_binary_src_spinner_png_end) {
    *data++ = *src++;
    length--;
  }
  return CAIRO_STATUS_SUCCESS;
}

/**
 * PhocSpinner:
 *
 * An animated spinner, used to represent indeterminate progress. It should be rendered as a
 * [type@Bling]. It will draw a static sprite that is animated through all 360 degrees of rotation.
 * On being mapped, it will compute a texture atlas containing all 360 frames.
 */
struct _PhocSpinner {
  GObject parent;

  gint                width;
  gint                height;
  gint                cx;
  gint                cy;
  PhocAnimatable     *animatable;
  PhocTimedAnimation *animation;
  PhocPropertyEaser  *easer;
  gfloat              rotation;
  struct wlr_texture *texture;
};

static void bling_interface_init (PhocBlingInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PhocSpinner, phoc_spinner, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PHOC_TYPE_BLING, bling_interface_init))


static void
on_animation_done (PhocSpinner *self)
{
  /* animation cycles indefinitely */
  phoc_timed_animation_play (self->animation);
}


static PhocBox
bling_get_box (PhocBling *bling)
{
  PhocSpinner *self = PHOC_SPINNER (bling);

  return (PhocBox) {
    .x = self->cx - self->width * 0.5,
    .y = self->cy - self->height * 0.5,
    .width = self->width,
    .height = self->height,
  };
}


static void
phoc_spinner_damage_box (PhocSpinner *self)
{
  PhocDesktop *desktop = phoc_server_get_desktop (phoc_server_get_default ());
  PhocOutput *output;

  if (!self->texture)
    return;

  wl_list_for_each (output, &desktop->outputs, link) {
    struct wlr_box damage_box = bling_get_box (PHOC_BLING (self));
    bool intersects = wlr_output_layout_intersects (desktop->layout, output->wlr_output, &damage_box);
    if (!intersects)
      continue;

    damage_box.x -= output->lx;
    damage_box.y -= output->ly;
    phoc_utils_scale_box (&damage_box, output->wlr_output->scale);

    if (wlr_damage_ring_add_box (&output->damage_ring, &damage_box))
      wlr_output_schedule_frame (output->wlr_output);
  }
}


static void
bling_render (PhocBling *bling, PhocRenderContext *ctx)
{
  PhocSpinner *self = PHOC_SPINNER (bling);
  struct wlr_render_texture_options options;

  if (self->texture == NULL)
    return;

  options = (struct wlr_render_texture_options) {
    .texture = self->texture,
    .src_box = {
      .x = self->width * ((int)floor(self->rotation) % 19),
      .y = self->height * floor(self->rotation / 19),
      .width = self->width,
      .height = self->height,
    },
    .dst_box = bling_get_box (bling),
  };

  wlr_render_pass_add_texture (ctx->render_pass, &options);
}


static void
phoc_spinner_create_texture (PhocSpinner *self)
{
  int atlas_width, atlas_height, stride;
  unsigned char *data;
  g_autoptr (cairo_surface_t) spinner_surf = NULL;
  g_autoptr (cairo_surface_t) texture_surf = NULL;
  g_autoptr (cairo_t) cr = NULL;
  PhocServer *server = phoc_server_get_default ();
  PhocRenderer *renderer = phoc_server_get_renderer (server);

  spinner_surf = cairo_image_surface_create_from_png_stream (read_spinner,
                                                             &(struct read_spinner_progress) {});

  if (!spinner_surf)
    return;

  self->width = cairo_image_surface_get_width (spinner_surf);
  self->height = cairo_image_surface_get_height (spinner_surf);

  if (!self->width || !self->height)
    return;

  /* the 360 frames are arranged in 19 columns and 19 rows */
  atlas_width = self->width * 19;
  atlas_height = self->height * 19;

  texture_surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, atlas_width, atlas_height);
  if (!texture_surf)
    return;

  cr = cairo_create (texture_surf);

  for (int i = 0; i < 360; i++) {
    cairo_save (cr);
    cairo_translate(cr, self->width * (i % 19), (i / 19) * self->height);
    cairo_translate(cr, (self->width * 0.5), (self->height * 0.5));
    cairo_rotate (cr, (double)i * (M_PI / 180.0f));
    cairo_translate(cr, -(self->width * 0.5), -(self->height * 0.5));
    cairo_set_source_surface (cr, spinner_surf, 0, 0);
    cairo_paint (cr);
    cairo_restore (cr);
  }

  cairo_surface_flush (texture_surf);
  data = cairo_image_surface_get_data (texture_surf);
  stride = cairo_image_surface_get_stride (texture_surf);
  self->texture = wlr_texture_from_pixels (phoc_renderer_get_wlr_renderer (renderer),
                                          DRM_FORMAT_ARGB8888, stride, atlas_width, atlas_height, data);
}


static void
bling_map (PhocBling *bling)
{
  PhocSpinner *self = PHOC_SPINNER (bling);

  phoc_spinner_create_texture (self);
  phoc_spinner_damage_box (self);
  phoc_timed_animation_play (self->animation);
}


static void
bling_unmap (PhocBling *bling)
{
  PhocSpinner *self = PHOC_SPINNER (bling);

  phoc_spinner_damage_box (self);
  g_clear_pointer (&self->texture, wlr_texture_destroy);
  phoc_timed_animation_reset (self->animation);
}


static gboolean
bling_is_mapped (PhocBling *bling)
{
  PhocSpinner *self = PHOC_SPINNER (bling);

  return self->texture != NULL;
}


static void
bling_interface_init (PhocBlingInterface *iface)
{
  iface->get_box = bling_get_box;
  iface->render = bling_render;
  iface->map = bling_map;
  iface->unmap = bling_unmap;
  iface->is_mapped = bling_is_mapped;
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
    phoc_spinner_damage_box (self);
    self->cx = g_value_get_int (value);
    phoc_spinner_damage_box (self);
    break;
  case PROP_CY:
    phoc_spinner_damage_box (self);
    self->cy = g_value_get_int (value);
    phoc_spinner_damage_box (self);
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
    phoc_spinner_damage_box (self);
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

  phoc_bling_unmap (PHOC_BLING (self));
  g_clear_object (&self->easer);
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
