/*
 * This file is part of the rc_genicam_api package.
 *
 * Copyright (c) 2017 Roboception GmbH
 * All rights reserved
 *
 * Author: Heiko Hirschmueller
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RC_GENICAM_API_IMAGE
#define RC_GENICAM_API_IMAGE

#include "buffer.h"

#include <memory>

namespace rcg
{

/**
  The image class encapsulates image information. It can be created from a
  buffer or a buffer part in case of a mult part buffer and provides a part of
  its information. It can be used to temporarily store the image so that the
  buffer can be freed.

  NOTE: A GenTLException is thrown in case of a severe error.
*/

class Image
{
  public:

    /**
      Copies the image information of the buffer.

      @param buffer Buffer object to copy the data from.
      @param part   Part number from which the image should be created.
    */

    Image(const Buffer *buffer, uint32_t part);

    /**
      Pointer to pixel information of the image.

      @return Pointer to pixels.
    */

    const uint8_t *getPixels() const { return pixel.get(); }

    uint64_t getTimestampNS() const { return timestamp; }

    size_t getWidth() const { return width; }
    size_t getHeight() const { return height; }
    size_t getXOffset() const { return xoffset; }
    size_t getYOffset() const { return yoffset; }
    size_t getXPadding() const { return xpadding; }
    size_t getYPadding() const { return ypadding; }
    uint64_t getFrameID() const { return frameid; }
    uint64_t getPixelFormat() const { return pixelformat; }
    bool isBigEndian() const { return bigendian; }

  private:

    Image(class Image &); // forbidden
    Image &operator=(const Image &); // forbidden

    std::unique_ptr<uint8_t []> pixel;

    uint64_t timestamp;
    size_t width;
    size_t height;
    size_t xoffset;
    size_t yoffset;
    size_t xpadding;
    size_t ypadding;
    uint64_t frameid;
    uint64_t pixelformat;
    bool bigendian;
};

/**
  Conversion of one pixel from YCbCr411 format (6 bytes for four pixels) to
  RGB.

  @param rgb Pointer to an array of size 3 for storing the result.
  @param row Image row.
  @param i   Index of pixel in row that should be converted.
*/

void convYCbCr411toRGB(uint8_t rgb[3], const uint8_t *row, int i);

/**
  Conversion of a group of four pixels from YCbCr411 format (6 bytes for four
  pixels) to RGB. Conversion of four pixels is a bit more efficient than
  conversions of individual pixels.

  @param rgb Pointer to an array of size 3 for storing the result.
  @param row Image row.
  @param i   Index of first pixel in row that should be converted. The index
             must be a multiple of 4!
*/

void convYCbCr411toQuadRGB(uint8_t rgb[12], const uint8_t *row, int i);

/**
  Conversion of one pixel from YCbCr422 format (4 bytes for 2 pixels) to
  RGB.

  @param rgb Pointer to an array of size 3 for storing the result.
  @param row Image row.
  @param i   Index of pixel in row that should be converted.
*/

void convYCbCr422toRGB(uint8_t rgb[3], const uint8_t *row, int i);

/**
  Conversion of a group of four pixels from YCbCr422 format (8 bytes for 4
  pixels) to RGB. Conversion of four pixels is a bit more efficient than
  conversions of individual pixels.

  @param rgb Pointer to an array of size 3*4 for storing the result.
  @param row Image row.
  @param i   Index of first pixel in row that should be converted. The index
             must be a multiple of 4!
*/

void convYCbCr422toQuadRGB(uint8_t rgb[12], const uint8_t *row, int i);

/**
  Expects an image in Mono8, RGB8, YCbCr411_8, YCbCr422_8 or YUV422_8 format
  and returns the color as RGB value at the given pixel location. The downscale
  factor ds can be greater than one. In this case, the given pixel location
  refers to the downscaled image and the returned color is averaged over
  ds x ds pixels.

  @param rgb  Array of size 3 for returning the color.
  @param img  Pointer to image.
  @param ds   Downscale factor, i.e. >= 1
  @param i, k Pixel location in downscaled coordinates.
*/

void getColor(uint8_t rgb[3], const std::shared_ptr<const Image> &img,
              uint32_t ds, uint32_t i, uint32_t k);

/**
  Converts image to RGB and monochrome format. Supported formats can be checked
  with isFormatSupported().

  @param rgb_out     Pointer to target array for rgb image. The array must have
                     a size of 3*width*height pixel. The pointer can be 0.
  @param mono_out    Pointer to target array for monochrome image. The array
                     must have a size of width*height pixel. The pointer can be 0.
  @param raw         Pointer to input pixels.
  @param pixelformat Pixel format of input.
  @param width       Width of image.
  @param height      Height of image.
  @param xpadding    Padding of input image.
  @return            False, if pixelformat is not supported. In this case,
                     nothing is written to the target pointers.
*/

bool convertImage(uint8_t *rgb_out, uint8_t *mono_out, const uint8_t *raw, uint64_t pixelformat,
  size_t width, size_t height, size_t xpadding);

/**
  Returns true if the given pixel format is supported by the convertImage()
  function.

  Currently supported color formats are: RGB8, BayerRG8, BayerBG8, BayerGR8,
  BayerGB8, YCbCr411_8, YCbCr422_8 and YUV422_8

  Currently supported monochrome formats: Mono8, Confidence8, Error8

  @param only_color If true, the true is returned only for supported color
                    formats.
  @return           True if given pixel format is supported.
*/

bool isFormatSupported(uint64_t pixelformat, bool only_color);

}

#endif
