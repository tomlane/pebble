/* Copyright 2015-2016 Samsung Electronics Co., Ltd.
 * Copyright 2016 University of Szeged.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "lit-strings.h"

#include "jrt-libc-includes.h"

/**
 * Validate utf-8 string
 *
 * NOTE:
 *   Isolated surrogates are allowed.
 *   Correct pair of surrogates is not allowed, it should be represented as 4-byte utf-8 character.
 *
 * @return true if utf-8 string is well-formed
 *         false otherwise
 */
bool
lit_is_utf8_string_valid (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                          lit_utf8_size_t buf_size) /**< string size */
{
  lit_utf8_size_t idx = 0;

  bool is_prev_code_point_high_surrogate = false;
  while (idx < buf_size)
  {
    lit_utf8_byte_t c = utf8_buf_p[idx++];
    if ((c & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
    {
      is_prev_code_point_high_surrogate = false;
      continue;
    }

    lit_code_point_t code_point = 0;
    lit_code_point_t min_code_point = 0;
    lit_utf8_size_t extra_bytes_count;
    if ((c & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
    {
      extra_bytes_count = 1;
      min_code_point = LIT_UTF8_2_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_5_BITS_MASK));
    }
    else if ((c & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER)
    {
      extra_bytes_count = 2;
      min_code_point = LIT_UTF8_3_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_4_BITS_MASK));
    }
    else if ((c & LIT_UTF8_4_BYTE_MASK) == LIT_UTF8_4_BYTE_MARKER)
    {
      extra_bytes_count = 3;
      min_code_point = LIT_UTF8_4_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_3_BITS_MASK));
    }
    else
    {
      /* utf-8 string could not contain 5- and 6-byte sequences. */
      return false;
    }

    if (idx + extra_bytes_count > buf_size)
    {
      /* utf-8 string breaks in the middle */
      return false;
    }

    for (lit_utf8_size_t offset = 0; offset < extra_bytes_count; ++offset)
    {
      c = utf8_buf_p[idx + offset];
      if ((c & LIT_UTF8_EXTRA_BYTE_MASK) != LIT_UTF8_EXTRA_BYTE_MARKER)
      {
        /* invalid continuation byte */
        return false;
      }
      code_point <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
      code_point |= (c & LIT_UTF8_LAST_6_BITS_MASK);
    }

    if (code_point < min_code_point
        || code_point > LIT_UNICODE_CODE_POINT_MAX)
    {
      /* utf-8 string doesn't encode valid unicode code point */
      return false;
    }

    if (code_point >= LIT_UTF16_HIGH_SURROGATE_MIN
        && code_point <= LIT_UTF16_HIGH_SURROGATE_MAX)
    {
      is_prev_code_point_high_surrogate = true;
    }
    else if (code_point >= LIT_UTF16_LOW_SURROGATE_MIN
             && code_point <= LIT_UTF16_LOW_SURROGATE_MAX
             && is_prev_code_point_high_surrogate)
    {
      /* sequence of high and low surrogate is not allowed */
      return false;
    }
    else
    {
      is_prev_code_point_high_surrogate = false;
    }

    idx += extra_bytes_count;
  }

  return true;
} /* lit_is_utf8_string_valid */

/**
 * Validate cesu-8 string
 *
 * @return true if cesu-8 string is well-formed
 *         false otherwise
 */
bool
lit_is_cesu8_string_valid (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                           lit_utf8_size_t buf_size) /**< string size */
{
  lit_utf8_size_t idx = 0;

  while (idx < buf_size)
  {
    lit_utf8_byte_t c = utf8_buf_p[idx++];
    if ((c & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
    {
      continue;
    }

    lit_code_point_t code_point = 0;
    lit_code_point_t min_code_point = 0;
    lit_utf8_size_t extra_bytes_count;
    if ((c & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
    {
      extra_bytes_count = 1;
      min_code_point = LIT_UTF8_2_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_5_BITS_MASK));
    }
    else if ((c & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER)
    {
      extra_bytes_count = 2;
      min_code_point = LIT_UTF8_3_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_4_BITS_MASK));
    }
    else
    {
      return false;
    }

    if (idx + extra_bytes_count > buf_size)
    {
      /* cesu-8 string breaks in the middle */
      return false;
    }

    for (lit_utf8_size_t offset = 0; offset < extra_bytes_count; ++offset)
    {
      c = utf8_buf_p[idx + offset];
      if ((c & LIT_UTF8_EXTRA_BYTE_MASK) != LIT_UTF8_EXTRA_BYTE_MARKER)
      {
        /* invalid continuation byte */
        return false;
      }
      code_point <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
      code_point |= (c & LIT_UTF8_LAST_6_BITS_MASK);
    }

    if (code_point < min_code_point)
    {
      /* cesu-8 string doesn't encode valid unicode code point */
      return false;
    }

    idx += extra_bytes_count;
  }

  return true;
} /* lit_is_cesu8_string_valid */

/**
 * Check if the code point is UTF-16 low surrogate
 *
 * @return true / false
 */
bool
lit_is_code_point_utf16_low_surrogate (lit_code_point_t code_point) /**< code point */
{
  return LIT_UTF16_LOW_SURROGATE_MIN <= code_point && code_point <= LIT_UTF16_LOW_SURROGATE_MAX;
} /* lit_is_code_point_utf16_low_surrogate */

/**
 * Check if the code point is UTF-16 high surrogate
 *
 * @return true / false
 */
bool
lit_is_code_point_utf16_high_surrogate (lit_code_point_t code_point) /**< code point */
{
  return LIT_UTF16_HIGH_SURROGATE_MIN <= code_point && code_point <= LIT_UTF16_HIGH_SURROGATE_MAX;
} /* lit_is_code_point_utf16_high_surrogate */

/**
 * Represents code point (>0xFFFF) as surrogate pair and returns its lower part
 *
 * @return lower code_unit of the surrogate pair
 */
static ecma_char_t
convert_code_point_to_low_surrogate (lit_code_point_t code_point) /**< code point, should be > 0xFFFF */
{
  JERRY_ASSERT (code_point > LIT_UTF16_CODE_UNIT_MAX);

  ecma_char_t code_unit_bits;
  code_unit_bits = (ecma_char_t) (code_point & LIT_UTF16_LAST_10_BITS_MASK);

  return (ecma_char_t) (LIT_UTF16_LOW_SURROGATE_MARKER | code_unit_bits);
} /* convert_code_point_to_low_surrogate */

/**
 * Represents code point (>0xFFFF) as surrogate pair and returns its higher part
 *
 * @return higher code_unit of the surrogate pair
 */
static ecma_char_t
convert_code_point_to_high_surrogate (lit_code_point_t code_point) /**< code point, should be > 0xFFFF */
{
  JERRY_ASSERT (code_point > LIT_UTF16_CODE_UNIT_MAX);
  JERRY_ASSERT (code_point <= LIT_UNICODE_CODE_POINT_MAX);

  ecma_char_t code_unit_bits;
  code_unit_bits = (ecma_char_t) ((code_point - LIT_UTF16_FIRST_SURROGATE_CODE_POINT) >> LIT_UTF16_BITS_IN_SURROGATE);

  return (LIT_UTF16_HIGH_SURROGATE_MARKER | code_unit_bits);
} /* convert_code_point_to_high_surrogate */

/**
 *
 * @return byte count required to represent the code point
 */

lit_utf8_size_t
lit_get_codepoint_utf8_size(lit_code_point_t code_point) {
  if (code_point <= LIT_UTF8_1_BYTE_CODE_POINT_MAX)
  {
    return 1;
  }
  else if (code_point <= LIT_UTF8_2_BYTE_CODE_POINT_MAX)
  {
    return 2;
  }
  else if (code_point <= LIT_UTF8_3_BYTE_CODE_POINT_MAX)
  {
    return 3;
  }
  else
  {
    JERRY_ASSERT (code_point <= LIT_UTF8_4_BYTE_CODE_POINT_MAX);
    return 4;
  }
}

/**
 * Handle surrogate code point in CESU-8 string and increase the pointer by one code unit.
 * When a complete surrogate code point is found, the callback will be called.
 *
 * NOTE:
 *   Half surrogate pairs will be ignored and "dropped" silently.
 *
 * @return required size for the UTF-8 buffer
 */
static lit_utf8_size_t
lit_cesu8_inc_and_handle_surrogate(const lit_utf8_byte_t **cesu8_str_p,
                                   ecma_char_t *high_surrogate_ch,
                                   void(*surrogate_pair_cb)(const lit_code_point_t cp, void *ctx),
                                   void(*non_surrogate_ch_cb)(const lit_code_point_t cp, void *ctx),
                                   void *ctx) {
  ecma_char_t ch = 0;
  lit_utf8_size_t surrogate_size = 0;
  const lit_utf8_size_t codepoint_sz = lit_read_code_unit_from_utf8(*cesu8_str_p, &ch);
  *cesu8_str_p += codepoint_sz;
  if (lit_is_code_point_utf16_high_surrogate(ch)) {
    *high_surrogate_ch = ch;
    return codepoint_sz;
  }
  else if (lit_is_code_point_utf16_low_surrogate(ch)) {
    surrogate_size = codepoint_sz;
    if (0 != *high_surrogate_ch) {
      const lit_code_point_t cp = lit_convert_surrogate_pair_to_code_point(*high_surrogate_ch, ch);
      surrogate_pair_cb(cp, ctx);
    }
  } else if (non_surrogate_ch_cb) {
    non_surrogate_ch_cb((lit_code_point_t)ch, ctx);
  }
  *high_surrogate_ch = 0;
  return surrogate_size;
}

/**
 * Helper/callback for lit_utf8_string_size_from_cesu8_string() implementation.
 */
static void lit_cesu8_handle_surrogates_utf8_size_callback(const lit_code_point_t cp,
                                                           void *ctx) {
  lit_utf8_size_t *utf8_buf_size = (lit_utf8_size_t *)ctx;
  *utf8_buf_size += lit_get_codepoint_utf8_size(cp);
}

/**
 * Calculate the required size for a buffer to contain the UTF-8 encoded data, given a CESU-8
 * encoded string.
 *
 * NOTE:
 *   Half surrogate pairs will be ignored and "dropped" silently.
 *
 * @return required size for the UTF-8 buffer
 */
lit_utf8_size_t
lit_utf8_string_size_from_cesu8_string (const lit_utf8_byte_t *cesu8_str_p,  /**< cesu-8 string */
                                        lit_utf8_size_t cesu8_buf_size) /**< cesu-8 string size */
{
  lit_utf8_size_t utf8_buf_size = cesu8_buf_size;
  ecma_char_t high_surrogate_ch = LIT_UNICODE_CODE_POINT_NULL;
  const lit_utf8_byte_t *const end = cesu8_str_p + cesu8_buf_size;
  while (cesu8_str_p < end) {
    if ((*cesu8_str_p & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER) {
      ++cesu8_str_p;
      high_surrogate_ch = 0;
      continue;
    }
    utf8_buf_size -=
        lit_cesu8_inc_and_handle_surrogate(&cesu8_str_p, &high_surrogate_ch,
                                           lit_cesu8_handle_surrogates_utf8_size_callback, NULL,
                                           &utf8_buf_size);
  }
  return utf8_buf_size;
} /** lit_utf8_string_size_from_cesu8_string */

typedef struct {
  lit_utf8_byte_t **utf8_str_ptr_ptr;
  const lit_utf8_byte_t *utf8_end;
  bool is_buffer_too_small;
} Cesu8ToUtf8CbData;

/**
 * Helper/callback for lit_utf8_string_convert_from_cesu8() implementation.
 */
static void lit_cesu8_handle_surrogates_utf8_convert_callback(const lit_code_point_t cp,
                                                              void *ctx) {
  Cesu8ToUtf8CbData *cb_data = (Cesu8ToUtf8CbData *)ctx;
  lit_utf8_byte_t **utf8_str_p = cb_data->utf8_str_ptr_ptr;
  if (*utf8_str_p + lit_get_codepoint_utf8_size(cp) > cb_data->utf8_end) {
    cb_data->is_buffer_too_small = true;
    return;
  }
  *utf8_str_p += lit_code_point_to_utf8(cp, *utf8_str_p);
}

/**
 * Copy & convert a CESU-8 encoded string into a UTF-8 encoded string.
 *
 * NOTE:
 *   Half surrogate pairs will be ignored and "dropped" silently.
 *
 * @return number of bytes of UTF-8 data written
 */
lit_utf8_size_t
lit_utf8_string_convert_from_cesu8 (const lit_utf8_byte_t *cesu8_str_p,  /**< cesu-8 string */
                                    lit_utf8_size_t cesu8_buf_size,  /**< cesu-8 buffer size */
                                    lit_utf8_byte_t *utf8_str_p, /**< [out] utf-8 buffer */
                                    lit_utf8_size_t utf8_buf_size) /**< utf-8 buffer size */
{
  ecma_char_t high_surrogate_ch = LIT_UNICODE_CODE_POINT_NULL;
  const lit_utf8_byte_t *const cesu8_end = cesu8_str_p + cesu8_buf_size;
  lit_utf8_byte_t *const begin = utf8_str_p;
  const lit_utf8_byte_t *const utf8_end = utf8_str_p + utf8_buf_size;
  Cesu8ToUtf8CbData cb_data = {
    .utf8_str_ptr_ptr = &utf8_str_p,
    .utf8_end = utf8_end,
    .is_buffer_too_small = false,
  };
  while (cesu8_str_p < cesu8_end && *cesu8_str_p) {
    const lit_utf8_byte_t byte = *cesu8_str_p;
    if ((byte & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER) {
      if (utf8_str_p >= utf8_end) {
        return 0;
      }
      *(utf8_str_p++) = byte;
      ++cesu8_str_p;
      high_surrogate_ch = 0;
      continue;
    }

    lit_cesu8_inc_and_handle_surrogate(&cesu8_str_p, &high_surrogate_ch,
                                       lit_cesu8_handle_surrogates_utf8_convert_callback,
                                       lit_cesu8_handle_surrogates_utf8_convert_callback,
                                       &cb_data);
    if (cb_data.is_buffer_too_small) {
      return 0;
    }
  }
  return (lit_utf8_size_t) (utf8_str_p - begin);
} /** lit_utf8_string_size_from_cesu8_string */

/**
 * Calculate size of a zero-terminated utf-8 string
 *
 * NOTE:
 *   string should not contain zero characters in the middel
 *
 * @return size of a string
 */
lit_utf8_size_t
lit_zt_utf8_string_size (const lit_utf8_byte_t *utf8_str_p) /**< zero-terminated utf-8 string */
{
  return (lit_utf8_size_t) strlen ((const char *) utf8_str_p);
} /* lit_zt_utf8_string_size */

/**
 * Calculate length of a cesu-8 encoded string
 *
 * @return UTF-16 code units count
 */
ecma_length_t
lit_utf8_string_length (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                        lit_utf8_size_t utf8_buf_size) /**< string size */
{
  ecma_length_t length = 0;
  lit_utf8_size_t size = 0;

  while (size < utf8_buf_size)
  {
    size += lit_get_unicode_char_size_by_utf8_first_byte (*(utf8_buf_p + size));
    length++;
  }

  JERRY_ASSERT (size == utf8_buf_size);

  return length;
} /* lit_utf8_string_length */

/**
 * Decodes a unicode code point from non-empty utf-8-encoded buffer
 *
 * @return number of bytes occupied by code point in the string
 */
lit_utf8_size_t
lit_read_code_point_from_utf8 (const lit_utf8_byte_t *buf_p, /**< buffer with characters */
                               lit_utf8_size_t buf_size, /**< size of the buffer in bytes */
                               lit_code_point_t *code_point) /**< [out] code point */
{
  JERRY_ASSERT (buf_p && buf_size);

  lit_utf8_byte_t c = buf_p[0];
  if ((c & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
  {
    *code_point = (lit_code_point_t) (c & LIT_UTF8_LAST_7_BITS_MASK);
    return 1;
  }

  lit_code_point_t ret = LIT_UNICODE_CODE_POINT_NULL;
  ecma_length_t bytes_count = 0;
  if ((c & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
  {
    bytes_count = 2;
    ret = ((lit_code_point_t) (c & LIT_UTF8_LAST_5_BITS_MASK));
  }
  else if ((c & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER)
  {
    bytes_count = 3;
    ret = ((lit_code_point_t) (c & LIT_UTF8_LAST_4_BITS_MASK));
  }
  else if ((c & LIT_UTF8_4_BYTE_MASK) == LIT_UTF8_4_BYTE_MARKER)
  {
    bytes_count = 4;
    ret = ((lit_code_point_t) (c & LIT_UTF8_LAST_3_BITS_MASK));
  }
  else
  {
    JERRY_ASSERT (false);
  }

  JERRY_ASSERT (buf_size >= bytes_count);

  for (uint32_t i = 1; i < bytes_count; ++i)
  {
    ret <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
    ret |= (buf_p[i] & LIT_UTF8_LAST_6_BITS_MASK);
  }

  *code_point = ret;
  return bytes_count;
} /* lit_read_code_point_from_utf8 */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return number of bytes occupied by code point in the string
 */
lit_utf8_size_t
lit_read_code_unit_from_utf8 (const lit_utf8_byte_t *buf_p, /**< buffer with characters */
                              ecma_char_t *code_point) /**< [out] code point */
{
  JERRY_ASSERT (buf_p);

  lit_utf8_byte_t c = buf_p[0];
  if ((c & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
  {
    *code_point = (ecma_char_t) (c & LIT_UTF8_LAST_7_BITS_MASK);
    return 1;
  }

  lit_code_point_t ret = LIT_UNICODE_CODE_POINT_NULL;
  ecma_length_t bytes_count;
  if ((c & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
  {
    bytes_count = 2;
    ret = ((lit_code_point_t) (c & LIT_UTF8_LAST_5_BITS_MASK));
  }
  else
  {
    JERRY_ASSERT ((c & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER);
    bytes_count = 3;
    ret = ((lit_code_point_t) (c & LIT_UTF8_LAST_4_BITS_MASK));
  }

  for (uint32_t i = 1; i < bytes_count; ++i)
  {
    ret <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
    ret |= (buf_p[i] & LIT_UTF8_LAST_6_BITS_MASK);
  }

  JERRY_ASSERT (ret <= LIT_UTF16_CODE_UNIT_MAX);
  *code_point = (ecma_char_t) ret;
  return bytes_count;
} /* lit_read_code_unit_from_utf8 */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return number of bytes occupied by code point in the string
 */
lit_utf8_size_t
lit_read_prev_code_unit_from_utf8 (const lit_utf8_byte_t *buf_p, /**< buffer with characters */
                                   ecma_char_t *code_point) /**< [out] code point */
{
  JERRY_ASSERT (buf_p);

  lit_utf8_decr (&buf_p);
  return lit_read_code_unit_from_utf8 (buf_p, code_point);
} /* lit_read_prev_code_unit_from_utf8 */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return next code unit
 */
ecma_char_t
lit_utf8_read_next (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (*buf_p);
  ecma_char_t ch;

  *buf_p += lit_read_code_unit_from_utf8 (*buf_p, &ch);

  return ch;
} /* lit_utf8_read_next */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return previous code unit
 */
ecma_char_t
lit_utf8_read_prev (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (*buf_p);
  ecma_char_t ch;

  lit_utf8_decr (buf_p);
  lit_read_code_unit_from_utf8 (*buf_p, &ch);

  return ch;
} /* lit_utf8_read_prev */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return next code unit
 */
ecma_char_t
lit_utf8_peek_next (const lit_utf8_byte_t *buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (buf_p);
  ecma_char_t ch;

  lit_read_code_unit_from_utf8 (buf_p, &ch);

  return ch;
} /* lit_utf8_peek_next */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return previous code unit
 */
ecma_char_t
lit_utf8_peek_prev (const lit_utf8_byte_t *buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (buf_p);
  ecma_char_t ch;

  lit_read_prev_code_unit_from_utf8 (buf_p, &ch);

  return ch;
} /* lit_utf8_peek_prev */

/**
 * Increase cesu-8 encoded string pointer by one code unit.
 */
void
lit_utf8_incr (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (*buf_p);

  *buf_p += lit_get_unicode_char_size_by_utf8_first_byte (**buf_p);
} /* lit_utf8_incr */

/**
 * Decrease cesu-8 encoded string pointer by one code unit.
 */
void
lit_utf8_decr (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (*buf_p);
  const lit_utf8_byte_t *current_p = *buf_p;

  do
  {
    current_p--;
  }
  while ((*(current_p) & LIT_UTF8_EXTRA_BYTE_MASK) == LIT_UTF8_EXTRA_BYTE_MARKER);

  *buf_p = current_p;
} /* lit_utf8_decr */

/**
 * Calc hash using the specified hash_basis.
 *
 * NOTE:
 *   This is implementation of FNV-1a hash function, which is released into public domain.
 *   Constants used, are carefully picked primes by the authors.
 *   More info: http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * @return ecma-string's hash
 */
inline lit_string_hash_t __attr_always_inline___
lit_utf8_string_hash_combine (lit_string_hash_t hash_basis, /**< hash to be combined with */
                              const lit_utf8_byte_t *utf8_buf_p, /**< characters buffer */
                              lit_utf8_size_t utf8_buf_size) /**< number of characters in the buffer */
{
  JERRY_ASSERT (utf8_buf_p != NULL || utf8_buf_size == 0);

  uint32_t hash = hash_basis;

  for (uint32_t i = 0; i < utf8_buf_size; i++)
  {
    // 16777619 is 32 bit FNV_prime = 2^24 + 2^8 + 0x93 = 16777619
    hash = (hash ^ utf8_buf_p[i]) * 16777619;
  }

  return (lit_string_hash_t) hash;
} /* lit_utf8_string_hash_combine */

/**
 * Calculate hash from the buffer.
 *
 * @return ecma-string's hash
 */
inline lit_string_hash_t __attr_always_inline___
lit_utf8_string_calc_hash (const lit_utf8_byte_t *utf8_buf_p, /**< characters buffer */
                           lit_utf8_size_t utf8_buf_size) /**< number of characters in the buffer */
{
  JERRY_ASSERT (utf8_buf_p != NULL || utf8_buf_size == 0);

  // 32 bit offset_basis for FNV = 2166136261
  return lit_utf8_string_hash_combine ((lit_string_hash_t) 2166136261, utf8_buf_p, utf8_buf_size);
} /* lit_utf8_string_calc_hash */

/**
 * Return code unit at the specified position in string
 *
 * NOTE:
 *   code_unit_offset should be less then string's length
 *
 * @return code unit value
 */
ecma_char_t
lit_utf8_string_code_unit_at (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                              lit_utf8_size_t utf8_buf_size, /**< string size in bytes */
                              ecma_length_t code_unit_offset) /**< ofset of a code_unit */
{
  lit_utf8_byte_t *current_p = (lit_utf8_byte_t *) utf8_buf_p;
  ecma_char_t code_unit;

  do
  {
    JERRY_ASSERT (current_p < utf8_buf_p + utf8_buf_size);
    current_p += lit_read_code_unit_from_utf8 (current_p, &code_unit);
  }
  while (code_unit_offset--);

  return code_unit;
} /* lit_utf8_string_code_unit_at */

/**
 * Get CESU-8 encoded size of character
 *
 * @return number of bytes occupied in CESU-8
 */
inline lit_utf8_size_t __attr_always_inline___
lit_get_unicode_char_size_by_utf8_first_byte (const lit_utf8_byte_t first_byte) /**< buffer with characters */
{
  if ((first_byte & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
  {
    return 1;
  }
  else if ((first_byte & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
  {
    return 2;
  }
  else
  {
    JERRY_ASSERT ((first_byte & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER);
    return 3;
  }
} /* lit_get_unicode_char_size_by_utf8_first_byte */

/**
 * Convert code unit to cesu-8 representation
 *
 * @return byte count required to represent the code unit
 */
lit_utf8_size_t
lit_code_unit_to_utf8 (ecma_char_t code_unit, /**< code unit */
                       lit_utf8_byte_t *buf_p) /**< buffer where to store the result,
                                                *   its size should be at least MAX_BYTES_IN_CODE_UNIT */
{
  if (code_unit <= LIT_UTF8_1_BYTE_CODE_POINT_MAX)
  {
    buf_p[0] = (lit_utf8_byte_t) code_unit;
    return 1;
  }
  else if (code_unit <= LIT_UTF8_2_BYTE_CODE_POINT_MAX)
  {
    uint32_t code_unit_bits = code_unit;
    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_unit_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_5_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_unit_bits);

    buf_p[0] = LIT_UTF8_2_BYTE_MARKER | first_byte_bits;
    buf_p[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    return 2;
  }
  else
  {
    uint32_t code_unit_bits = code_unit;
    lit_utf8_byte_t third_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_unit_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_unit_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_4_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_unit_bits);

    buf_p[0] = LIT_UTF8_3_BYTE_MARKER | first_byte_bits;
    buf_p[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    buf_p[2] = LIT_UTF8_EXTRA_BYTE_MARKER | third_byte_bits;
    return 3;
  }
} /* lit_code_unit_to_utf8 */

/**
 * Convert code point to cesu-8 representation
 *
 * @return byte count required to represent the code point
 */
lit_utf8_size_t
lit_code_point_to_cesu8 (lit_code_point_t code_point, /**< code point */
                         lit_utf8_byte_t *buf) /**< buffer where to store the result,
                                                *   its size should be at least 6 bytes */
{
  if (code_point <= LIT_UTF16_CODE_UNIT_MAX)
  {
    return lit_code_unit_to_utf8 ((ecma_char_t) code_point, buf);
  }
  else
  {
    lit_utf8_size_t offset = lit_code_unit_to_utf8 (convert_code_point_to_high_surrogate (code_point), buf);
    offset += lit_code_unit_to_utf8 (convert_code_point_to_low_surrogate (code_point), buf + offset);
    return offset;
  }
} /* lit_code_point_to_cesu8 */

/**
 * Convert code point to utf-8 representation
 *
 * @return byte count required to represent the code point
 */
lit_utf8_size_t
lit_code_point_to_utf8 (lit_code_point_t code_point, /**< code point */
                        lit_utf8_byte_t *buf) /**< buffer where to store the result,
                                              *   its size should be at least 4 bytes */
{
  if (code_point <= LIT_UTF8_1_BYTE_CODE_POINT_MAX)
  {
    buf[0] = (lit_utf8_byte_t) code_point;
    return 1;
  }
  else if (code_point <= LIT_UTF8_2_BYTE_CODE_POINT_MAX)
  {
    uint32_t code_point_bits = code_point;
    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_5_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_point_bits);

    buf[0] = LIT_UTF8_2_BYTE_MARKER | first_byte_bits;
    buf[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    return 2;
  }
  else if (code_point <= LIT_UTF8_3_BYTE_CODE_POINT_MAX)
  {
    uint32_t code_point_bits = code_point;
    lit_utf8_byte_t third_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_4_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_point_bits);

    buf[0] = LIT_UTF8_3_BYTE_MARKER | first_byte_bits;
    buf[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    buf[2] = LIT_UTF8_EXTRA_BYTE_MARKER | third_byte_bits;
    return 3;
  }
  else
  {
    JERRY_ASSERT (code_point <= LIT_UTF8_4_BYTE_CODE_POINT_MAX);

    uint32_t code_point_bits = code_point;
    lit_utf8_byte_t fourth_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t third_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_3_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_point_bits);

    buf[0] = LIT_UTF8_4_BYTE_MARKER | first_byte_bits;
    buf[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    buf[2] = LIT_UTF8_EXTRA_BYTE_MARKER | third_byte_bits;
    buf[3] = LIT_UTF8_EXTRA_BYTE_MARKER | fourth_byte_bits;
    return 4;
  }
} /* lit_code_point_to_utf8 */

/**
 * Convert surrogate pair to code point
 *
 * @return code point
 */
lit_code_point_t
lit_convert_surrogate_pair_to_code_point (ecma_char_t high_surrogate, /**< high surrogate code point */
                                          ecma_char_t low_surrogate) /**< low surrogate code point */
{
  JERRY_ASSERT (lit_is_code_point_utf16_high_surrogate (high_surrogate));
  JERRY_ASSERT (lit_is_code_point_utf16_low_surrogate (low_surrogate));

  lit_code_point_t code_point;
  code_point = (uint16_t) (high_surrogate - LIT_UTF16_HIGH_SURROGATE_MIN);
  code_point <<= LIT_UTF16_BITS_IN_SURROGATE;

  code_point += LIT_UTF16_FIRST_SURROGATE_CODE_POINT;

  code_point |= (uint16_t) (low_surrogate - LIT_UTF16_LOW_SURROGATE_MIN);
  return code_point;
} /* lit_convert_surrogate_pair_to_code_point */

/**
 * Compare cesu-8 string to cesu-8 string
 *
 * @return  true - if strings are equal;
 *          false - otherwise.
 */
bool
lit_compare_utf8_strings (const lit_utf8_byte_t *string1_p, /**< utf-8 string */
                          lit_utf8_size_t string1_size, /**< string size */
                          const lit_utf8_byte_t *string2_p, /**< utf-8 string */
                          lit_utf8_size_t string2_size) /**< string size */
{
  if (string1_size != string2_size)
  {
    return false;
  }

  return memcmp (string1_p, string2_p, string1_size) == 0;
} /* lit_compare_utf8_strings */

/**
 * Relational compare of cesu-8 strings
 *
 * First string is less than second string if:
 *  - strings are not equal;
 *  - first string is prefix of second or is lexicographically less than second.
 *
 * @return true - if first string is less than second string,
 *         false - otherwise.
 */
bool lit_compare_utf8_strings_relational (const lit_utf8_byte_t *string1_p, /**< utf-8 string */
                                          lit_utf8_size_t string1_size, /**< string size */
                                          const lit_utf8_byte_t *string2_p, /**< utf-8 string */
                                          lit_utf8_size_t string2_size) /**< string size */
{
  lit_utf8_byte_t *string1_pos = (lit_utf8_byte_t *) string1_p;
  lit_utf8_byte_t *string2_pos = (lit_utf8_byte_t *) string2_p;
  const lit_utf8_byte_t *string1_end_p = string1_p + string1_size;
  const lit_utf8_byte_t *string2_end_p = string2_p + string2_size;

  while (string1_pos < string1_end_p && string2_pos < string2_end_p)
  {
    ecma_char_t ch1, ch2;
    string1_pos += lit_read_code_unit_from_utf8 (string1_pos, &ch1);
    string2_pos += lit_read_code_unit_from_utf8 (string2_pos, &ch2);

    if (ch1 < ch2)
    {
      return true;
    }
    else if (ch1 > ch2)
    {
      return false;
    }
  }

  return (string1_pos >= string1_end_p && string2_pos < string2_end_p);
} /* lit_compare_utf8_strings_relational */
