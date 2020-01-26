/*
    This file is part of darktable,
    copyright (c) 2020 Martin Burri.

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
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <cmocka.h>

#include "iop/filmicrgb.c"

// epsilon for floating point comparison (TODO: take more sophisticated value):
#define E 1e-6f


/*
 * MOCKED FUNCTIONS
 */

void __wrap_dt_iop_color_picker_reset(dt_iop_module_t *module, gboolean update)
{
  check_expected_ptr(module);
  check_expected(update);
}

dt_iop_order_iccprofile_info_t *__wrap_dt_ioppr_get_pipe_work_profile_info(struct dt_dev_pixelpipe_t *pipe)
{
  return mock_ptr_type(dt_iop_order_iccprofile_info_t *);
}


/*
 * TEST FUNCTIONS
 */

static void test_sample(void **state)
{
  expect_value(__wrap_dt_iop_color_picker_reset, module, NULL);
  expect_value(__wrap_dt_iop_color_picker_reset, update, TRUE);
  gui_focus(NULL, 0);
}

/*
 * Verify that the pixel norm is computed corretly, given different pixel values
 *
 * This is an example of a small unit test that really only verifies one function.
 */
static void test_get_pixel_norm(void **state)
{
// Number of test vectors:
#define N 4
  const dt_iop_order_iccprofile_info_t *const work_profile = NULL; // unused
  const dt_iop_filmicrgb_methods_type_t variants[2] = {
    DT_FILMIC_METHOD_MAX_RGB,
    DT_FILMIC_METHOD_LUMINANCE
  };
  const float pixels[N][4] = {
    // N pixels to be tested...
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.5f, 0.0f, 0.0f },
    { 1.0f, 2.0f, 3.0f, 4.0f }, // note: value at index [3] is ignored
    { 1.1f, 0.0f, 0.0f, 0.0f }
    // TODO: This is just an example, we need more and more sophisticated test
    // vectors.
  };
  const float exp_norms[2][N] = {
    // expected norms...
    { 0.0f, 0.5f, 3.0f, 1.1f },               // max_rgb
    { 0.0f, 0.358439f, 1.838112f, 0.244755f } // luminance
    // TODO: luminance values are taken experimentally, just for demonstration.
    // They should be computed by some means for testing of course!
  };

  float norm = 0.0f;

  for(int v = 0; v < 2; v++)
  {
    printf("variant=%i\n", v);
    for(int i = 0; i < N; i++)
    {
      const float *const pixel = pixels[i];
      printf("case %i: pixel={%f, %f, %f, %f}", i, pixel[0], pixel[1], pixel[2], pixel[3]);
      norm = get_pixel_norm(pixel, variants[v], work_profile);
      printf(" -> norm=%f\n", norm);
      // TODO: use assert_float_equal() - not available in Ubuntu 18.04 version
      assert_true(norm < (exp_norms[v][i] + E));
      assert_true(norm > (exp_norms[v][i] - E));
    }
  }
#undef N
}

/*
 * Verify the process() method, given a simple environment and some test data
 *
 * This is a more advanced test that covers way more than just a method.  Actually, this is not
 * really a unit test anymore. But I think it has a very nice granularity since this way we can
 * test each module separately and feed it with simple and very small "test images". As an
 * assertion we could have simple boundary checks or more dedicated expectations values. I also
 * plan to add kind of a "basic set" of test inputs that each module must pass (e.g.  bounded
 * output values etc.).
 */
static void test_process(void **state)
{
  // prepare environment (TODO: is there a better way?):
  struct dt_iop_module_t module;
  init(&module);
  module.commit_params = commit_params;
  printf("module ok\n");
  struct dt_dev_pixelpipe_t pipe;
  struct dt_dev_pixelpipe_iop_t piece;
  init_pipe(&module, &pipe, &piece);
  piece.pipe = &pipe;
  printf("pipe ok\n");

  // prepare input and output buffers:
  // TODO: of course this is just a start...
  const float in[1][4] = { { 0.4f, 0.99f, 1.2f, 0.0f } };
  float out[1][4] = { { 0.0f, 0.0f, 0.0f, 0.0f } };
  dt_iop_roi_t roi_in;
  dt_iop_roi_t roi_out;
  roi_out.width = 1;
  roi_out.height = 1;

  // load mock return values:
  will_return(__wrap_dt_ioppr_get_pipe_work_profile_info, NULL);

  // set parameters:
  dt_iop_filmicrgb_data_t *data = (dt_iop_filmicrgb_data_t *)piece.data;
  data->preserve_color = DT_FILMIC_METHOD_MAX_RGB;

  // now, call process():
  printf("calling process()...\n");
  process(&module, &piece, in, out, &roi_in, &roi_out);

  // assert return value:
  // TODO: we just print here - of course, some assertion is needed :-)
  for(size_t k = 0; k < roi_out.height * roi_out.width; k += 1)
  {
    float *const pix_out = out[k];
    printf("[%f, %f, %f, %f]\n", pix_out[0], pix_out[1], pix_out[2], pix_out[3]);
  }
}


/*
 * MAIN FUNCTION
 */
int main()
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_sample),
    cmocka_unit_test(test_get_pixel_norm),
    cmocka_unit_test(test_process)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
