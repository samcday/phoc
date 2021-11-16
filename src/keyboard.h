#pragma once

#include "input.h"
#include "input-device.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <xkbcommon/xkbcommon.h>

G_BEGIN_DECLS

#define PHOC_KEYBOARD_PRESSED_KEYSYMS_CAP 32

#define PHOC_TYPE_KEYBOARD (phoc_keyboard_get_type())

G_DECLARE_FINAL_TYPE (PhocKeyboard, phoc_keyboard, PHOC, KEYBOARD, PhocInputDevice)

PhocKeyboard *phoc_keyboard_new (struct wlr_input_device *device,
                                 PhocSeat *seat);
void          phoc_keyboard_next_layout (PhocKeyboard *self);
uint32_t      phoc_keyboard_get_meta_key (PhocKeyboard *self);

G_END_DECLS
