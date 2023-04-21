/*
MIT License

Copyright (c) 2011-2017 Ihar Yermalayeu

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstring>
#include <cmath>
#include <cinttypes>
#include <algorithm>
#include "features.hpp"

#define LINEAR_SHIFT 4
#define BILINEAR_SHIFT (LINEAR_SHIFT * 2)
#define BILINEAR_ROUND_TERM (1 << (BILINEAR_SHIFT - 1))
#define FRACTION_RANGE (1 << LINEAR_SHIFT)
#define TRK_PI    3.1415926535897932384626433832795

inline size_t
AlignHi (size_t size, size_t align)
{
  return (size + align - 1) & ~(align - 1);
}

inline void *
AlignHi (const void *ptr, size_t align)
{
  return (void *) ((((size_t) ptr) + align - 1) & ~(align - 1));
}


inline void *
rsz_allocate (size_t size)
{
  size_t align = 64;

  size += 128;

  void *ptr = NULL;
#if defined(_MSC_VER)
  ptr = _aligned_malloc (size, align);
#elif defined(__GNUC__)
  align = AlignHi (align, sizeof (void *));
  size = AlignHi (size, align);
  if (posix_memalign (&ptr, align, size)) {
    ptr = NULL;
  }
#else
  ptr = malloc (size);
#endif

  if (ptr)
    ptr = (char *) ptr + 64;

  return ptr;
}

inline void
rsz_free (void *ptr)
{
  if (ptr)
    ptr = (char *) ptr - 64;

#if defined(_MSC_VER)
  _aligned_free (ptr);
#elif defined(__MINGW32__) || defined(__MINGW64__)
  return __mingw_aligned_free (ptr);
#else
  free (ptr);
#endif
}

void
estimate_alpha_index (size_t srcSize, size_t dstSize, int *indexes, int *alphas,
    size_t channelCount)
{
  float scale = (float) srcSize / dstSize;

  for (size_t i = 0; i < dstSize; ++i) {
    float alpha = (float) ((i + 0.5) * scale - 0.5);

    int64_t index = (int64_t) std::floor (alpha);

    alpha -= index;

    if (index < 0) {
      index = 0;
      alpha = 0;
    }

    if (index > (int64_t) srcSize - 2) {
      index = srcSize - 2;
      alpha = 1;
    }

    for (size_t c = 0; c < channelCount; c++) {
      size_t offset = i * channelCount + c;
      indexes[offset] = (int) (channelCount * index + c);
      alphas[offset] = (int) (alpha * FRACTION_RANGE + 0.5);
    }
  }
}

struct rsz_buffer_c
{
  rsz_buffer_c (size_t width, size_t height)
  {
    _p = rsz_allocate (2 * sizeof (int) * (2 * width + height));
      ix = (int *) _p;
      ax = ix + width;
      iy = ax + width;
      ay = iy + height;
      pbx[0] = (int *) (ay + height);
      pbx[1] = pbx[0] + width;
  }
   ~rsz_buffer_c ()
  {
    rsz_free (_p);
  }
  int *ix;
  int *ax;
  int *iy;
  int *ay;
  int *pbx[2];
private:
  void *_p;
};

void
resize_bilinear_c (const uint8_t * src, size_t srcWidth, size_t srcHeight,
    size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight,
    size_t dstStride, size_t channelCount)
{
  size_t dstRowSize = channelCount * dstWidth;

  rsz_buffer_c buffer (dstRowSize, dstHeight);

  estimate_alpha_index (srcHeight, dstHeight, buffer.iy, buffer.ay, 1);

  estimate_alpha_index (srcWidth, dstWidth, buffer.ix, buffer.ax, channelCount);

  int64_t previous = -2;

  for (size_t yDst = 0; yDst < dstHeight; yDst++, dst += dstStride) {
    int fy = buffer.ay[yDst];
    int64_t sy = buffer.iy[yDst];
    int k = 0, *t;

    if (sy == previous)
      k = 2;
    else if (sy == previous + 1) {
      t = buffer.pbx[0];
      buffer.pbx[0] = buffer.pbx[1];
      buffer.pbx[1] = t;

      k = 1;
    }

    previous = sy;

    for (; k < 2; k++) {
      int *pb = buffer.pbx[k];
      const uint8_t *ps = src + (sy + k) * srcStride;
      for (size_t x = 0; x < dstRowSize; x++) {
        size_t sx = buffer.ix[x];
        int fx = buffer.ax[x];
        int t = ps[sx];
        pb[x] = (t << LINEAR_SHIFT) + (ps[sx + channelCount] - t) * fx;
      }
    }

    if (fy == 0)
      for (size_t xDst = 0; xDst < dstRowSize; xDst++)
        dst[xDst] =
            ((buffer.pbx[0][xDst] << LINEAR_SHIFT) +
            BILINEAR_ROUND_TERM) >> BILINEAR_SHIFT;
    else if (fy == FRACTION_RANGE)
      for (size_t xDst = 0; xDst < dstRowSize; xDst++)
        dst[xDst] =
            ((buffer.pbx[1][xDst] << LINEAR_SHIFT) +
            BILINEAR_ROUND_TERM) >> BILINEAR_SHIFT;
    else {
      for (size_t xDst = 0; xDst < dstRowSize; xDst++) {
        int t = buffer.pbx[0][xDst];
        dst[xDst] =
            ((t << LINEAR_SHIFT) + (buffer.pbx[1][xDst] - t) * fy +
            BILINEAR_ROUND_TERM) >> BILINEAR_SHIFT;
      }
    }
  }
}

class HOGFeatureExtractor_c
{
  static const size_t C = 8;
  static const size_t Q = 9;
  static const size_t Q2 = 18;

  size_t _sx, _sy, _hs;

  float _cos[5];
  float _sin[5];
  float _k[C];

  int *_index;
  float *_value;
  float *_histogram;
  float *_norm;

  void Init (size_t w, size_t h)
  {
    _sx = w / C;
    _hs = _sx + 2;
    _sy = h / C;
    for (int i = 0; i < 5; ++i)
    {
      _cos[i] = (float)::cos (i * TRK_PI / Q);
      _sin[i] = (float)::sin (i * TRK_PI / Q);
    }
    for (int i = 0; i < (int) C; ++i)
      _k[i] = float ((1 + i * 2) / 16.0f);

    _index = (int *) malloc (sizeof (int) * w);
    _value = (float *) malloc (sizeof (float) * w);
    _histogram = (float *) malloc (sizeof (float) * (_sx + 2) * (_sy + 2) * Q2);
    _norm = (float *) malloc (sizeof (float) * (_sx + 2) * (_sy + 2));
  }

  void AddRowToHistogram (size_t row, size_t width, size_t height)
  {
    size_t iyp = (row - 4) / C;
    float vy0 = _k[(row + 4) & 7];
    float vy1 = 1.0f - vy0;
    float *h0 = _histogram + ((iyp + 1) * _hs + 0) * Q2;
    float *h1 = _histogram + ((iyp + 2) * _hs + 0) * Q2;        //Min<size_t
    for (size_t col = 1, n = C, i = 5; col < width - 1;
        i = 0, n = std::min (C, width - col - 1)) {
      for (; i < n; ++i, ++col) {
        float value = _value[col];
        int index = _index[col];
        float vx0 = _k[i];
        float vx1 = 1.0f - vx0;
        h0[index] += vx1 * vy1 * value;
        h1[index] += vx1 * vy0 * value;
        h0[Q2 + index] += vx0 * vy1 * value;
        h1[Q2 + index] += vx0 * vy0 * value;
      }
      h0 += Q2;
      h1 += Q2;
    }
  }

  void EstimateHistogram (const uint8_t * src, size_t stride, size_t width,
      size_t height)
  {
    memset (_histogram, 0, sizeof (float) * (_sx + 2) * (_sy + 2) * Q2);
    for (size_t row = 1; row < height - 1; ++row) {
      const uint8_t *src1 = src + stride * row;
      const uint8_t *src0 = src1 - stride;
      const uint8_t *src2 = src1 + stride;

      for (size_t col = 1; col < width - 1; ++col) {
        float dy = (float) (src2[col] - src0[col]);
        float dx = (float) (src1[col + 1] - src1[col - 1]);
        float value = (float)::sqrt (dx * dx + dy * dy);
        float ady = fabs (dy);
        float adx = fabs (dx);

        float bestDot = 0;
        int index = 0;
        for (int direction = 0; direction < 5; direction++) {
          float dot = _cos[direction] * adx + _sin[direction] * ady;
          if (dot > bestDot) {
            bestDot = dot;
            index = direction;
          }
        }
        if (dx < 0)
          index = Q - index;
        if (dy < 0 && index != 0)
          index = Q2 - index - (dx == 0);

        _value[col] = value;
        _index[col] = index;
      }

      AddRowToHistogram (row, width, height);
    }
  }

  void EstimateNorm ()
  {
    memset (_norm, 0, sizeof (float) * (_sx + 2) * (_sy + 2));
    for (size_t y = 0; y < _sy; ++y) {
      const float *ph = _histogram + ((y + 1) * _hs + 1) * Q2;
      float *pn = _norm + (y + 1) * _hs + 1;
      for (size_t x = 0; x < _sx; ++x) {
        const float *h = ph + x * Q2;
        for (int o = 0; o < (int) Q; ++o)
          pn[x] += pow ((h[o] + h[o + Q]), 2);
      }
    }
  }

  void ExtractFeatures (float *features)
  {
    float eps = 0.0001f;
    for (size_t y = 0; y < _sy; y++) {
      for (size_t x = 0; x < _sx; x++) {
        float *dst = features + (y * _sx + x) * 31;

        float *psrc, n1, n2, n3, n4;

        float *p0 = _norm + y * _hs + x;
        float *p1 = p0 + _hs;
        float *p2 = p1 + _hs;

        n1 = 1.0f / sqrt (p1[1] + p1[2] + p2[1] + p2[2] + eps);
        n2 = 1.0f / sqrt (p0[1] + p0[2] + p1[1] + p1[2] + eps);
        n3 = 1.0f / sqrt (p1[0] + p1[1] + p2[0] + p2[1] + eps);
        n4 = 1.0f / sqrt (p0[0] + p0[1] + p1[0] + p1[1] + eps);

        float t1 = 0;
        float t2 = 0;
        float t3 = 0;
        float t4 = 0;

        psrc = _histogram + ((y + 1) * _hs + x + 1) * Q2;
        for (int o = 0; o < (int) Q2; o++) {
          float h1 = std::min (*psrc * n1, 0.2f);
          float h2 = std::min (*psrc * n2, 0.2f);
          float h3 = std::min (*psrc * n3, 0.2f);
          float h4 = std::min (*psrc * n4, 0.2f);
          *dst = 0.5f * (h1 + h2 + h3 + h4);
          t1 += h1;
          t2 += h2;
          t3 += h3;
          t4 += h4;
          dst++;
          psrc++;
        }

        psrc = _histogram + ((y + 1) * _hs + x + 1) * Q2;
        for (int o = 0; o < (int) Q; o++) {
          float sum = *psrc + *(psrc + Q);
          float h1 = std::min (sum * n1, 0.2f);
          float h2 = std::min (sum * n2, 0.2f);
          float h3 = std::min (sum * n3, 0.2f);
          float h4 = std::min (sum * n4, 0.2f);
          *dst = 0.5f * (h1 + h2 + h3 + h4);
          dst++;
          psrc++;
        }

        *dst = 0.2357f * t1;
        dst++;
        *dst = 0.2357f * t2;
        dst++;
        *dst = 0.2357f * t3;
        dst++;
        *dst = 0.2357f * t4;
      }
    }
  }

  void ExtractFeatures22 (float *features)
  {
    float eps = 0.0001f;
    for (size_t y = 0; y < _sy; y++) {
      for (size_t x = 0; x < _sx; x++) {
        float *dst = features + (y * _sx + x) * 22;

        float *psrc, n1, n2, n3, n4;

        float *p0 = _norm + y * _hs + x;
        float *p1 = p0 + _hs;
        float *p2 = p1 + _hs;

        n1 = 1.0f / sqrt (p1[1] + p1[2] + p2[1] + p2[2] + eps);
        n2 = 1.0f / sqrt (p0[1] + p0[2] + p1[1] + p1[2] + eps);
        n3 = 1.0f / sqrt (p1[0] + p1[1] + p2[0] + p2[1] + eps);
        n4 = 1.0f / sqrt (p0[0] + p0[1] + p1[0] + p1[1] + eps);

        float t1 = 0;
        float t2 = 0;
        float t3 = 0;
        float t4 = 0;

        psrc = _histogram + ((y + 1) * _hs + x + 1) * Q2;
        for (int o = 0; o < (int) Q2; o++) {
          float h1 = std::min (*psrc * n1, 0.2f);
          float h2 = std::min (*psrc * n2, 0.2f);
          float h3 = std::min (*psrc * n3, 0.2f);
          float h4 = std::min (*psrc * n4, 0.2f);
          *dst = 0.5f * (h1 + h2 + h3 + h4);
          t1 += h1;
          t2 += h2;
          t3 += h3;
          t4 += h4;
          dst++;
          psrc++;
        }

        *dst = 0.2357f * t1;
        dst++;
        *dst = 0.2357f * t2;
        dst++;
        *dst = 0.2357f * t3;
        dst++;
        *dst = 0.2357f * t4;
      }
    }
  }

  void Deinit ()
  {
    free (_index);
    free (_value);
    free (_histogram);
    free (_norm);
  }

public:
  void Run (const uint8_t * src, size_t stride, size_t width, size_t height,
      float *features, int fet_len)
  {
    Init (width, height);

    EstimateHistogram (src, stride, width, height);

    EstimateNorm ();

    if (fet_len == 31)
      ExtractFeatures (features);
    else
      ExtractFeatures22 (features);

    Deinit ();
  }
};

class HOGFeatureExtractor4_c
{
  static const size_t C = 4;    //8; //adhi
  static const size_t Q = 9;
  static const size_t Q2 = 18;

  size_t _sx, _sy, _hs;

  float _cos[5];
  float _sin[5];
  float _k[C];

  int *_index;
  float *_value;
  float *_histogram;
  float *_norm;

  void Init (size_t w, size_t h)
  {
    _sx = w / C;
    _hs = _sx + 2;
    _sy = h / C;
    for (int i = 0; i < 5; ++i)
    {
      _cos[i] = (float)::cos (i * TRK_PI / Q);
      _sin[i] = (float)::sin (i * TRK_PI / Q);
    }
    for (int i = 0; i < (int) C; ++i)
      _k[i] = float ((1 + i * 2) / 8.0f);       //adhi 16 t0 8
    _index = (int *) malloc (sizeof (int) * w);
    _value = (float *) malloc (sizeof (float) * w);
    _histogram = (float *) malloc (sizeof (float) * (_sx + 2) * (_sy + 2) * Q2);
    _norm = (float *) malloc (sizeof (float) * (_sx + 2) * (_sy + 2));
  }

  void AddRowToHistogram (size_t row, size_t width, size_t height)
  {
    size_t iyp = (row - 2) / C; //adhi 4 to 2
    float vy0 = _k[(row + 2) & 3];      //adhi 4 to 2 and 7 to 3
    float vy1 = 1.0f - vy0;
    float *h0 = _histogram + ((iyp + 1) * _hs + 0) * Q2;
    float *h1 = _histogram + ((iyp + 2) * _hs + 0) * Q2;
    for (size_t col = 1, n = C, i = 3; col < width - 1; i = 0, n = std::min (C, width - col - 1))       //adhi i = 5 to 3
    {
      for (; i < n; ++i, ++col) {
        float value = _value[col];
        int index = _index[col];
        float vx0 = _k[i];
        float vx1 = 1.0f - vx0;
        h0[index] += vx1 * vy1 * value;
        h1[index] += vx1 * vy0 * value;
        h0[Q2 + index] += vx0 * vy1 * value;
        h1[Q2 + index] += vx0 * vy0 * value;
      }
      h0 += Q2;
      h1 += Q2;
    }
  }

  void EstimateHistogram (const uint8_t * src, size_t stride, size_t width,
      size_t height)
  {
    memset (_histogram, 0, sizeof (float) * (_sx + 2) * (_sy + 2) * Q2);
    for (size_t row = 1; row < height - 1; ++row) {
      const uint8_t *src1 = src + stride * row;
      const uint8_t *src0 = src1 - stride;
      const uint8_t *src2 = src1 + stride;

      for (size_t col = 1; col < width - 1; ++col) {
        float dy = (float) (src2[col] - src0[col]);
        float dx = (float) (src1[col + 1] - src1[col - 1]);
        float value = (float)::sqrt (dx * dx + dy * dy);
        float ady = fabs (dy);
        float adx = fabs (dx);

        float bestDot = 0;
        int index = 0;
        for (int direction = 0; direction < 5; direction++) {
          float dot = _cos[direction] * adx + _sin[direction] * ady;
          if (dot > bestDot) {
            bestDot = dot;
            index = direction;
          }
        }
        if (dx < 0)
          index = Q - index;
        if (dy < 0 && index != 0)
          index = Q2 - index - (dx == 0);

        _value[col] = value;
        _index[col] = index;
      }

      AddRowToHistogram (row, width, height);
    }
  }

  void EstimateNorm ()
  {
    memset (_norm, 0, sizeof (float) * (_sx + 2) * (_sy + 2));
    for (size_t y = 0; y < _sy; ++y) {
      const float *ph = _histogram + ((y + 1) * _hs + 1) * Q2;
      float *pn = _norm + (y + 1) * _hs + 1;
      for (size_t x = 0; x < _sx; ++x) {
        const float *h = ph + x * Q2;
        for (int o = 0; o < (int) Q; ++o)
          pn[x] += pow ((h[o] + h[o + Q]), 2);
      }
    }
  }

  void ExtractFeatures (float *features)
  {
    float eps = 0.0001f;
    for (size_t y = 0; y < _sy; y++) {
      for (size_t x = 0; x < _sx; x++) {
        float *dst = features + (y * _sx + x) * 31;

        float *psrc, n1, n2, n3, n4;

        float *p0 = _norm + y * _hs + x;
        float *p1 = p0 + _hs;
        float *p2 = p1 + _hs;

        n1 = 1.0f / sqrt (p1[1] + p1[2] + p2[1] + p2[2] + eps);
        n2 = 1.0f / sqrt (p0[1] + p0[2] + p1[1] + p1[2] + eps);
        n3 = 1.0f / sqrt (p1[0] + p1[1] + p2[0] + p2[1] + eps);
        n4 = 1.0f / sqrt (p0[0] + p0[1] + p1[0] + p1[1] + eps);

        float t1 = 0;
        float t2 = 0;
        float t3 = 0;
        float t4 = 0;

        psrc = _histogram + ((y + 1) * _hs + x + 1) * Q2;
        for (int o = 0; o < (int) Q2; o++) {
          float h1 = std::min (*psrc * n1, 0.2f);
          float h2 = std::min (*psrc * n2, 0.2f);
          float h3 = std::min (*psrc * n3, 0.2f);
          float h4 = std::min (*psrc * n4, 0.2f);
          *dst = 0.5f * (h1 + h2 + h3 + h4);
          t1 += h1;
          t2 += h2;
          t3 += h3;
          t4 += h4;
          dst++;
          psrc++;
        }

        psrc = _histogram + ((y + 1) * _hs + x + 1) * Q2;
        for (int o = 0; o < (int) Q; o++) {
          float sum = *psrc + *(psrc + Q);
          float h1 = std::min (sum * n1, 0.2f);
          float h2 = std::min (sum * n2, 0.2f);
          float h3 = std::min (sum * n3, 0.2f);
          float h4 = std::min (sum * n4, 0.2f);
          *dst = 0.5f * (h1 + h2 + h3 + h4);
          dst++;
          psrc++;
        }

        *dst = 0.2357f * t1;
        dst++;
        *dst = 0.2357f * t2;
        dst++;
        *dst = 0.2357f * t3;
        dst++;
        *dst = 0.2357f * t4;
      }
    }
  }

  void ExtractFeatures22 (float *features)
  {
    float eps = 0.0001f;
    for (size_t y = 0; y < _sy; y++) {
      for (size_t x = 0; x < _sx; x++) {
        float *dst = features + (y * _sx + x) * 22;

        float *psrc, n1, n2, n3, n4;

        float *p0 = _norm + y * _hs + x;
        float *p1 = p0 + _hs;
        float *p2 = p1 + _hs;

        n1 = 1.0f / sqrt (p1[1] + p1[2] + p2[1] + p2[2] + eps);
        n2 = 1.0f / sqrt (p0[1] + p0[2] + p1[1] + p1[2] + eps);
        n3 = 1.0f / sqrt (p1[0] + p1[1] + p2[0] + p2[1] + eps);
        n4 = 1.0f / sqrt (p0[0] + p0[1] + p1[0] + p1[1] + eps);

        float t1 = 0;
        float t2 = 0;
        float t3 = 0;
        float t4 = 0;

        psrc = _histogram + ((y + 1) * _hs + x + 1) * Q2;
        for (int o = 0; o < (int) Q2; o++) {
          float h1 = std::min (*psrc * n1, 0.2f);
          float h2 = std::min (*psrc * n2, 0.2f);
          float h3 = std::min (*psrc * n3, 0.2f);
          float h4 = std::min (*psrc * n4, 0.2f);
          *dst = 0.5f * (h1 + h2 + h3 + h4);
          t1 += h1;
          t2 += h2;
          t3 += h3;
          t4 += h4;
          dst++;
          psrc++;
        }

        *dst = 0.2357f * t1;
        dst++;
        *dst = 0.2357f * t2;
        dst++;
        *dst = 0.2357f * t3;
        dst++;
        *dst = 0.2357f * t4;
      }
    }
  }

  void Deinit ()
  {
    free (_index);
    free (_value);
    free (_histogram);
    free (_norm);
  }

public:
  void Run (const uint8_t * src, size_t stride, size_t width, size_t height,
      float *features, int fet_len)
  {
    Init (width, height);

    EstimateHistogram (src, stride, width, height);

    EstimateNorm ();

    if (fet_len == 31)
      ExtractFeatures (features);
    else
      ExtractFeatures22 (features);
    Deinit ();
  }
};

void
HOG_extract_features_c (const uint8_t * src, size_t stride, size_t width,
    size_t height, float *features, int cell_size, int fet_len)
{
  if (cell_size == 8) {
    HOGFeatureExtractor_c extractor;
    extractor.Run (src, stride, width, height, features, fet_len);
  } else {
    HOGFeatureExtractor4_c extractor;
    extractor.Run (src, stride, width, height, features, fet_len);
  }
}
