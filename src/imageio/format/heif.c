/*
    This file is part of darktable,
    copyright (c) 2019 darix

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <libheif/heif.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "bauhaus/bauhaus.h"
#include "common/colorspaces.h"
#include "common/darktable.h"
#include "common/imageio.h"
#include "common/imageio_module.h"
#include "control/conf.h"
#include "imageio/format/imageio_format_api.h"

DT_MODULE(3)

typedef enum
{
  heif_lossy = 0,
  heif_lossless = 1
} comp_type_t;

typedef struct dt_imageio_heif_t
{
  dt_imageio_module_data_t global;
  int bpp;
  int comp_type;
  int quality;
} dt_imageio_heif_t;

typedef struct dt_imageio_heif_gui_t
{
  GtkWidget *bit_depth;
  GtkWidget *compression;
  GtkWidget *quality;
} dt_imageio_heif_gui_t;

void init(dt_imageio_module_format_t *self) {
#ifdef USE_LUA
  luaA_enum(darktable.lua_state.state, comp_type_t);
  luaA_enum_value(darktable.lua_state.state, comp_type_t, heif_lossy);
  luaA_enum_value(darktable.lua_state.state, comp_type_t, heif_lossless);
  dt_lua_register_module_member(darktable.lua_state.state, self, dt_imageio_heif_t, comp_type, comp_type_t);
  dt_lua_register_module_member(darktable.lua_state.state, self, dt_imageio_heif_t, quality, int);
#endif
}

void cleanup(dt_imageio_module_format_t *self)
{
}

int write_image(struct dt_imageio_module_data_t *data, const char *filename, const void *in,
                dt_colorspaces_color_profile_type_t over_type, const char *over_filename,
                void *exif, int exif_len, int imgid, int num, int total, struct dt_dev_pixelpipe_t *pipe) {

  dt_imageio_heif_t *p = (dt_imageio_heif_t *)data;

  struct heif_image        *image   = NULL;
  struct heif_context      *context = heif_context_alloc ();
  struct heif_encoder      *encoder = NULL;
  struct heif_image_handle *handle  = NULL;
  struct heif_writer        writer;
  struct heif_error         err;

  gint                      stride = 0;
  guint8                   *plane_data;
  gboolean                  has_alpha = 0;

  const int width = p->global.width, height = p->global.height;
  const uint8_t *in_data = (const uint8_t *)in;

  err = heif_image_create (width, height,
                           heif_colorspace_RGB,
                           has_alpha ?
                           heif_chroma_interleaved_RGBA :
                           heif_chroma_interleaved_RGB,
                           &image);

  heif_image_add_plane (image, heif_channel_interleaved,
                        width, height, has_alpha ? 32 : 24);
  plane_data = heif_image_get_plane (image, heif_channel_interleaved, &stride);

  size_t channel_size = sizeof(uint8_t);
  if (p->bpp > 8) {
    channel_size = sizeof(uint16_t);
  }

  // TODO: foreach block of 4*channel_size bytes in the input array copy the first 3*channel_size bytes into the output array
  size_t plane_size = width*height*3*channel_size;
  size_t in_size    = width*height*4*channel_size;
  size_t i, j;

  for (i = 0, j = 0; i < plane_size && j < in_size; i += channel_size * 3, j += channel_size * 4) {
    memcpy(plane_data+i, in_data+j, channel_size * 3);
  }

  // get the default encoder
  err = heif_context_get_encoder_for_format(context, heif_compression_HEVC, &encoder);

  // set the encoder parameters
  heif_encoder_set_lossy_quality(encoder, p->quality);
  heif_encoder_set_lossless     (encoder, p->comp_type);

  // encode the image
  err = heif_context_encode_image(context, image, encoder, NULL, &handle);

  if (err.code != 0)
    {
      /* TODO: g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Encoding HEIF image failed: %s"),
                   err.message); */
      return FALSE;
    }
  heif_context_add_exif_metadata(context, handle, exif, exif_len);

  heif_context_write_to_file(context, filename);

  heif_image_handle_release (handle);
  heif_encoder_release(encoder);
	return 0;
}


size_t params_size(dt_imageio_module_format_t *self)
{
  return sizeof(dt_imageio_heif_t);
}

void *get_params(dt_imageio_module_format_t *self)
{
  dt_imageio_heif_t *d = (dt_imageio_heif_t *)calloc(1, sizeof(dt_imageio_heif_t));
  d->bpp = dt_conf_get_int("plugins/imageio/format/heif/bpp");
  d->comp_type = dt_conf_get_int("plugins/imageio/format/heif/comp_type");
  if(d->comp_type == heif_lossy)
    d->quality = dt_conf_get_int("plugins/imageio/format/heif/quality");
  else
    d->quality = 100;
  return d;
}

int set_params(dt_imageio_module_format_t *self, const void *params, const int size)
{
  if(size != self->params_size(self)) return 1;
  const dt_imageio_heif_t *d = (dt_imageio_heif_t *)params;
  dt_imageio_heif_gui_t *g = (dt_imageio_heif_gui_t *)self->gui_data;
  dt_bauhaus_combobox_set(g->compression, d->comp_type);
  dt_bauhaus_combobox_set(g->bit_depth, d->bpp);
  dt_bauhaus_slider_set(g->quality, d->quality);
  return 0;
}

void free_params(dt_imageio_module_format_t *self, dt_imageio_module_data_t *params)
{
  free(params);
}


int bpp(struct dt_imageio_module_data_t *data)
{
  return ((dt_imageio_heif_t *)data)->bpp;
}

int levels(struct dt_imageio_module_data_t *data)
{
  return IMAGEIO_RGB | IMAGEIO_INT8;
}

const char *mime(dt_imageio_module_data_t *data)
{
  return "image/heic";
}

const char *extension(dt_imageio_module_data_t *data)
{
  return "heif";
}

const char *name()
{
  return _("HEIF (8/10/12/16-bit)");
}

int flags(struct dt_imageio_module_data_t *data) {
  return FORMAT_FLAGS_SUPPORT_XMP;
}

static void compression_changed(GtkWidget *widget, gpointer user_data)
{
  const int comp_type = dt_bauhaus_combobox_get(widget);
  dt_conf_set_int("plugins/imageio/format/heif/comp_type", comp_type);
}

static void quality_changed(GtkWidget *slider, gpointer user_data)
{
  const int quality = (int)dt_bauhaus_slider_get(slider);
  dt_conf_set_int("plugins/imageio/format/heif/quality", quality);
}

static void bit_depth_changed(GtkWidget *slider, gpointer user_data)
{
  const int bpp_index = dt_bauhaus_combobox_get(slider);
  int bpp = 8;
  switch(bpp_index) {
    case 1:
        bpp = 10;
        break;
    case 2:
        bpp = 12;
        break;
    case 3:
        bpp = 16;
        break;
  }
  dt_conf_set_int("plugins/imageio/format/heif/bpp", bpp);
}

void gui_init(dt_imageio_module_format_t *self)
{
  dt_imageio_heif_gui_t *gui = (dt_imageio_heif_gui_t *)malloc(sizeof(dt_imageio_heif_gui_t));
  self->gui_data = (void *)gui;
  const int comp_type = dt_conf_get_int("plugins/imageio/format/heif/comp_type");
  const int quality = dt_conf_get_int("plugins/imageio/format/heif/quality");
  const int bpp = dt_conf_get_int("plugins/imageio/format/heif/bpp");

  self->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Bit depth combo box
  gui->bit_depth = dt_bauhaus_combobox_new(NULL);
  dt_bauhaus_widget_set_label(gui->bit_depth, NULL, _("bit depth"));
  dt_bauhaus_combobox_add(gui->bit_depth, _("8 bit"));
  dt_bauhaus_combobox_add(gui->bit_depth, _("10 bit"));
  dt_bauhaus_combobox_add(gui->bit_depth, _("12 bit"));
  dt_bauhaus_combobox_add(gui->bit_depth, _("16 bit"));
  switch(bpp) {
    case 10:
        dt_bauhaus_combobox_set(gui->bit_depth, 1);
        break;
    case 12:
        dt_bauhaus_combobox_set(gui->bit_depth, 2);
        break;
    case 16:
        dt_bauhaus_combobox_set(gui->bit_depth, 3);
        break;
    default:
      dt_bauhaus_combobox_set(gui->bit_depth, 0);
  }
  gtk_box_pack_start(GTK_BOX(self->widget), gui->bit_depth, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(gui->bit_depth), "value-changed", G_CALLBACK(bit_depth_changed), NULL);

  gui->compression = dt_bauhaus_combobox_new(NULL);
  dt_bauhaus_widget_set_label(gui->compression, NULL, _("compression type"));
  dt_bauhaus_combobox_add(gui->compression, _("lossy"));
  dt_bauhaus_combobox_add(gui->compression, _("lossless"));
  dt_bauhaus_combobox_set(gui->compression, comp_type);
  g_signal_connect(G_OBJECT(gui->compression), "value-changed", G_CALLBACK(compression_changed), NULL);
  gtk_box_pack_start(GTK_BOX(self->widget), gui->compression, TRUE, TRUE, 0);

  gui->quality = dt_bauhaus_slider_new_with_range(NULL, 5, 100, 1, 95, 0);
  dt_bauhaus_widget_set_label(gui->quality, NULL, _("quality"));
  dt_bauhaus_slider_set_default(gui->quality, 95);
  dt_bauhaus_slider_set_format(gui->quality, "%.2f%%");
  gtk_widget_set_tooltip_text(gui->quality, _("applies only to lossy setting"));
  if(quality > 0 && quality <= 100) dt_bauhaus_slider_set(gui->quality, quality);
  gtk_box_pack_start(GTK_BOX(self->widget), gui->quality, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(gui->quality), "value-changed", G_CALLBACK(quality_changed), (gpointer)0);
}

void gui_cleanup(dt_imageio_module_format_t *self)
{
  free(self->gui_data);
}

void gui_reset(dt_imageio_module_format_t *self)
{
}
