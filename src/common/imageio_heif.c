/*
 * This file is part of darktable,
 *
 *  Copyright (c) 2020      Andreas Schneider
 *
 *  darktable is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  darktable is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with darktable.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/image.h"
#include <inttypes.h>
#include <memory.h>
#include <stdio.h>
#include <strings.h>

#include "control/control.h"
#include "common/colorspaces.h"
#include "common/darktable.h"
#include "common/exif.h"
#include "control/conf.h"
#include "develop/develop.h"
#include "imageio.h"
#include "imageio_heif.h"

#include <libheif/heif.h>

struct heif_raw_data {
    const uint8_t *data;
    size_t size;
};

static dt_imageio_retval_t read_image(const char *filename, struct heif_raw_data *raw)
{
  size_t nread;
  size_t heif_file_size;
  FILE *f = NULL;
  dt_imageio_retval_t ret;
  int rc;
  const char *ext = strrchr(filename, '.');
  int cmp;

  cmp = strncmp(ext, ".heic", 5);
  if (cmp != 0) {
    cmp = strncmp(ext, ".heif", 5);
    if (cmp != 0) {
      return DT_IMAGEIO_FILE_CORRUPTED;
    }
  }

  f = g_fopen(filename, "rb");
  if (f == NULL) {
    return DT_IMAGEIO_FILE_NOT_FOUND;
  }

  rc = fseek(f, 0, SEEK_END);
  if (rc != 0) {
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }
  heif_file_size = ftell(f);
  if (heif_file_size < 10) {
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }
  rc = fseek(f, 0, SEEK_SET);
  if (rc != 0) {
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  raw->data = malloc(heif_file_size);
  if (raw->data == NULL) {
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }
  raw->size = heif_file_size;

  nread = fread((void *)raw->data, 1, raw->size, f); /* discard const */
  if (nread != heif_file_size) {
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  ret = DT_IMAGEIO_OK;
out:
  fclose(f);

  return ret;
}

dt_imageio_retval_t dt_imageio_open_heif(dt_image_t *img,
                                         const char *filename,
                                         dt_mipmap_buffer_t *mbuf)
{
  dt_imageio_retval_t ret;
  struct heif_raw_data raw = {
      .size = 0,
  };
  struct heif_context *ctx = NULL;
  struct heif_image_handle *handle = NULL;
  struct heif_image *heif = NULL;
  struct heif_error err = {
      .code = 0,
  };
  enum heif_filetype_result filetype_check = heif_filetype_no;
  int image_count = 0;

  ret = read_image(filename, &raw);
  if (ret != DT_IMAGEIO_OK) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to read image [%s]\n",
             filename);
    return ret;
  }

  filetype_check = heif_check_filetype(raw.data, 12);
  if (filetype_check == heif_filetype_no) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Invalid heif image [%s]\n",
             filename);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  ctx = heif_context_alloc();
  if (ctx == NULL) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to create HEIF context for image [%s]\n",
             filename);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  err = heif_context_read_from_memory_without_copy(ctx,
                                                   raw.data,
                                                   raw.size,
                                                   NULL);
  if (err.code != 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to parse HEIF image [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  image_count = heif_context_get_number_of_top_level_images(ctx);
  if (image_count == 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "No HEIF image in container [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
  }

  if (image_count > 1) {
    dt_control_log(_("image '%s' has more than one frame!"), filename);
  }

  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to decode first frame of HEIF image [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  err = heif_decode_image(handle,
                          &heif,
                          heif_colorspace_RGB,
                          heif_chroma_interleaved_RGBA,
                          NULL);
  if (err.code != 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to decode first frame of HEIF image [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  const size_t width = heif_image_handle_get_width(handle);
  const size_t height = heif_image_handle_get_height(handle);
  /* If `> 8', all plane ptrs are 'uint16_t *' */
  const size_t bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);

  /* Initialize cached image buffer */
  img->width = width;
  img->height = height;

  img->buf_dsc.channels = 4;
  img->buf_dsc.datatype = TYPE_FLOAT;
  img->buf_dsc.cst = iop_cs_rgb;

  float *mipbuf = (float *)dt_mipmap_cache_alloc(mbuf, img);
  if (mipbuf == NULL) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to allocate mipmap buffer for HEIF image [%s]\n",
             filename);
    ret = DT_IMAGEIO_CACHE_FULL;
    goto out;
  }

  /* This can be LDR or HDR, it depends on the ICC profile. */
  img->flags &= ~DT_IMAGE_RAW;
  img->flags |= DT_IMAGE_HDR;

  const float max_channel_f = (float)((1 << bit_depth) - 1);

  const size_t R_rowbytes;
  const size_t G_rowbytes;
  const size_t B_rowbytes;

  const uint8_t *const restrict R_plane8 = (const uint8_t *)heif_image_get_plane_readonly(heif, heif_channel_R, (int *)&R_rowbytes);
  const uint8_t *const restrict G_plane8 = (const uint8_t *)heif_image_get_plane_readonly(heif, heif_channel_G, (int *)&G_rowbytes);
  const uint8_t *const restrict B_plane8 = (const uint8_t *)heif_image_get_plane_readonly(heif, heif_channel_B, (int *)&B_rowbytes);

  switch (bit_depth) {
  case 16:
  case 12:
  case 10: {
#ifdef _OPENMP
#pragma omp parallel for simd default(none) \
  dt_omp_firstprivate(mipbuf, width, height, R_plane8, G_plane8, B_plane8, \
                      R_rowbytes, G_rowbytes, B_rowbytes, max_channel_f) \
  schedule(simd:static) \
  collapse(2)
#endif
    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
          float *pixel = &mipbuf[(size_t)4 * ((y * width) + x)];

          /* max_channel_f is 255.0f for 8bit */
          pixel[0] = ((float)*((uint16_t *)&R_plane8[(x * 2) + (y * R_rowbytes)])) * (1.0f / max_channel_f);
          pixel[1] = ((float)*((uint16_t *)&G_plane8[(x * 2) + (y * G_rowbytes)])) * (1.0f / max_channel_f);
          pixel[2] = ((float)*((uint16_t *)&B_plane8[(x * 2) + (y * B_rowbytes)])) * (1.0f / max_channel_f);
          pixel[3] = 0.0f; /* alpha */
      }
    }
    break;
  }
  case 8: {
#ifdef _OPENMP
#pragma omp parallel for simd default(none) \
  dt_omp_firstprivate(mipbuf, width, height, R_plane8, G_plane8, B_plane8, \
                      R_rowbytes, G_rowbytes, B_rowbytes, max_channel_f) \
  schedule(simd:static) \
  collapse(2)
#endif
    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
          float *pixel = &mipbuf[(size_t)4 * ((y * width) + x)];

          /* max_channel_f is 255.0f for 8bit */
          pixel[0] = ((float)R_plane8[x + (y * R_rowbytes)]) * (1.0f / max_channel_f);
          pixel[1] = ((float)G_plane8[x + (y * G_rowbytes)]) * (1.0f / max_channel_f);
          pixel[2] = ((float)B_plane8[x + (y * B_rowbytes)]) * (1.0f / max_channel_f);
          pixel[3] = 0.0f; /* alpha */
      }
    }
    break;
  }
  default:
    dt_print(DT_DEBUG_IMAGEIO,
             "Invalid bit depth for HEIF image [%s]\n",
             filename);
    ret = DT_IMAGEIO_CACHE_FULL;
    goto out;
  }

  ret = DT_IMAGEIO_OK;
out:
  heif_image_release(heif);
  heif_context_free(ctx);
  free((void *)raw.data);

  return ret;
}

dt_imageio_retval_t dt_imageio_heif_read_color_profile(const char *filename, struct heif_color_profile *cp)
{
  dt_imageio_retval_t ret;
  struct heif_raw_data raw = {
      .size = 0,
  };
  struct heif_context *ctx = NULL;
  struct heif_image_handle *handle = NULL;
  struct heif_image *heif = NULL;
  struct heif_error err = {
      .code = 0,
  };
  enum heif_filetype_result filetype_check = heif_filetype_no;
  int image_count = 0;
  enum heif_color_profile_type profile_type = heif_color_profile_type_not_present;

  ret = read_image(filename, &raw);
  if (ret != DT_IMAGEIO_OK) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to read image [%s]\n",
             filename);
    return ret;
  }

  filetype_check = heif_check_filetype(raw.data, 12);
  if (filetype_check == heif_filetype_no) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Invalid heif image [%s]\n",
             filename);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  ctx = heif_context_alloc();
  if (ctx == NULL) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to create HEIF context for image [%s]\n",
             filename);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  err = heif_context_read_from_memory_without_copy(ctx,
                                                   raw.data,
                                                   raw.size,
                                                   NULL);
  if (err.code != 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to parse HEIF image [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  image_count = heif_context_get_number_of_top_level_images(ctx);
  if (image_count == 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "No HEIF image in container [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
  }

  if (image_count > 1) {
    dt_control_log(_("image '%s' has more than one frame!"), filename);
  }

  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to decode first frame of HEIF image [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  err = heif_decode_image(handle,
                          &heif,
                          heif_colorspace_RGB,
                          heif_chroma_interleaved_RGBA,
                          NULL);
  if (err.code != 0) {
    dt_print(DT_DEBUG_IMAGEIO,
             "Failed to decode first frame of HEIF image [%s]: %s\n",
             filename, err.message);
    ret = DT_IMAGEIO_FILE_CORRUPTED;
    goto out;
  }

  profile_type = heif_image_handle_get_color_profile_type(handle);
  switch(profile_type) {
  case heif_color_profile_type_nclx: {
    struct heif_color_profile_nclx *nclx = NULL;

    err = heif_image_handle_get_nclx_color_profile(handle, &nclx);
    if (err.code != 0) {
        dt_print(DT_DEBUG_IMAGEIO,
                 "Failed to get NCLX box of image [%s]: %s\n",
                 filename, err.message);
        ret = DT_IMAGEIO_FILE_CORRUPTED;
        goto out;
    }

    switch(nclx->color_primaries) {
    /*
     * BT709
     */
    case heif_color_primaries_ITU_R_BT_709_5:

      switch (nclx->transfer_characteristics) {
      /*
       * SRGB
       */
      case heif_transfer_characteristic_ITU_R_BT_709_5:

        switch (nclx->matrix_coefficients) {
        case heif_matrix_coefficients_ITU_R_BT_709_5:
          cp->type = DT_COLORSPACE_SRGB;
          break;
        default:
          break;
        }

        break; /* SRGB */

#if 0
      /*
       * GAMMA22 BT709
       */
      case HEIF_NCLX_TRANSFER_CHARACTERISTICS_GAMMA22:

        switch (nclx.matrixCoefficients) {
        case HEIF_NCLX_MATRIX_COEFFICIENTS_BT709:
        case HEIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL:
          cp->type = DT_COLORSPACE_REC709;
          break;
        default:
          break;
        }

        break; /* GAMMA22 BT709 */
#endif

      /*
       * LINEAR BT709
       */
      case heif_transfer_characteristic_linear:

        switch (nclx->matrix_coefficients) {
        case heif_matrix_coefficients_RGB_GBR:
          cp->type = DT_COLORSPACE_LIN_REC709;
          break;
        default:
          break;
        }

        break; /* LINEAR BT709 */

      default:
        break;
      }

      break; /* BT709 */

#if 0
    /*
     * BT2020
     */
    case HEIF_NCLX_COLOUR_PRIMARIES_BT2020:

      switch (nclx.transferCharacteristics) {
      /*
       * LINEAR BT2020
       */
      case HEIF_NCLX_TRANSFER_CHARACTERISTICS_LINEAR:

        switch (nclx.matrixCoefficients) {
        case HEIF_NCLX_MATRIX_COEFFICIENTS_BT2020_NCL:
        case HEIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL:
          cp->type = DT_COLORSPACE_LIN_REC2020;
          break;
        default:
          break;
        }

        break; /* LINEAR BT2020 */

      /*
       * PQ BT2020
       */
      case HEIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_PQ:

        switch (nclx.matrixCoefficients) {
        case HEIF_NCLX_MATRIX_COEFFICIENTS_BT2020_NCL:
        case HEIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL:
          cp->type = DT_COLORSPACE_PQ_REC2020;
          break;
        default:
          break;
        }

        break; /* PQ BT2020 */

      /*
       * HLG BT2020
       */
      case HEIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_HLG:

        switch (nclx.matrixCoefficients) {
        case HEIF_NCLX_MATRIX_COEFFICIENTS_BT2020_NCL:
        case HEIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL:
          cp->type = DT_COLORSPACE_HLG_REC2020;
          break;
        default:
          break;
        }

        break; /* HLG BT2020 */

      default:
        break;
      }

      break; /* BT2020 */

    /*
     * P3
     */
    case HEIF_NCLX_COLOUR_PRIMARIES_P3:

      switch (nclx.transferCharacteristics) {
      /*
       * PQ P3
       */
      case HEIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_PQ:

        switch (nclx.matrixCoefficients) {
        case HEIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL:
          cp->type = DT_COLORSPACE_PQ_P3;
          break;
        default:
          break;
        }

        break; /* PQ P3 */

      /*
       * HLG P3
       */
      case HEIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_HLG:

        switch (nclx.matrixCoefficients) {
        case HEIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL:
          cp->type = DT_COLORSPACE_PQ_P3;
          break;
        default:
          break;
        }

        break; /* HLG P3 */

      default:
        break;
      }

      break; /* P3 */
#endif

    default:
      dt_print(DT_DEBUG_IMAGEIO,
               "Unsupported color profile for %s\n",
               filename);
      break;
    }

    break; /* HEIF_PROFILE_FORMAT_NCLX */
  }
  case heif_color_profile_type_rICC: {
    size_t icc_size = heif_image_get_raw_color_profile_size(heif);

    if (icc_size == 0) {
      ret = DT_IMAGEIO_FILE_CORRUPTED;
      goto out;
    }

    uint8_t *data = (uint8_t *)g_malloc0(icc_size * sizeof(uint8_t));
    if (data == NULL) {
      dt_print(DT_DEBUG_IMAGEIO,
               "Failed to allocate ICC buffer for HEIF image [%s]\n",
               filename);
      ret = DT_IMAGEIO_FILE_CORRUPTED;
      goto out;
    }
    err = heif_image_get_raw_color_profile(heif, &data);

    cp->icc_profile_size = icc_size;
    cp->icc_profile = data;
    break;
  }
  case heif_color_profile_type_prof:
  case heif_color_profile_type_not_present:
    break;
  }

  ret = DT_IMAGEIO_OK;
out:
  heif_image_release(heif);
  heif_context_free(ctx);
  free((void *)raw.data);

  return ret;
}
