// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Precompiled.h"
#include "Core/Str.h"
#include "Core/Heap.h"
#include "Math/Math.h"
#include "Image/Image.h"
#include "ImageInternal.h"

BE_NAMESPACE_BEGIN

static bool DecompressImage(const Image &srcImage, Image &dstImage) {
    assert(dstImage.GetFormat() == Image::Format::RGBA_8_8_8_8);
    assert(dstImage.GetPixels());

    switch (srcImage.GetFormat()) {
    case Image::Format::RGBA_DXT1:
        DecompressDXT1(srcImage, dstImage);
        break;
    case Image::Format::RGBA_DXT3:
        DecompressDXT3(srcImage, dstImage);
        break;
    case Image::Format::RGBA_DXT5:
        DecompressDXT5(srcImage, dstImage);
        break;
    case Image::Format::XGBR_DXT5:
        DecompressDXT5(srcImage, dstImage);
        BE_WARNLOG("XGBR_DXT5n\n");
        break;
    case Image::Format::DXN2:
        DecompressDXN2(srcImage, dstImage);
        break;
    case Image::Format::RGB_PVRTC_2BPPV1:
    case Image::Format::RGBA_PVRTC_2BPPV1:
    case Image::Format::RGBA_PVRTC_2BPPV2:
        DecompressPVRTC(srcImage, dstImage, 1);
        break;
    case Image::Format::RGB_PVRTC_4BPPV1:
    case Image::Format::RGBA_PVRTC_4BPPV1:
    case Image::Format::RGBA_PVRTC_4BPPV2:
        DecompressPVRTC(srcImage, dstImage, 0);
        break;
    case Image::Format::RGB_8_ETC1:
        DecompressETC1(srcImage, dstImage);
        break;
    case Image::Format::RGB_8_ETC2:
        DecompressETC2_RGB8(srcImage, dstImage);
        break;
    case Image::Format::RGBA_8_1_ETC2:
        DecompressETC2_RGB8A1(srcImage, dstImage);
        break;
    case Image::Format::RGBA_8_8_ETC2:
        DecompressETC2_RGBA8(srcImage, dstImage);
        break;
    case Image::Format::RG_11_11_EAC:
        DecompressETC2_RG11(srcImage, dstImage);
        break;
    case Image::Format::SignedRG_11_11_EAC:
        DecompressETC2_Signed_RG11(srcImage, dstImage);
        break;
    default:
        BE_WARNLOG("DecompressImage: unsupported format %s\n", srcImage.FormatName());
        return false;
    }

    return true;
}

static bool CompressImage(const Image &srcImage, Image &dstImage, Image::CompressionQuality::Enum compressoinQuality) {
    assert(srcImage.GetFormat() == Image::Format::RGBA_8_8_8_8);
    assert(srcImage.GetPixels());

    //uint64_t startClocks = rdtsc();

    switch (dstImage.GetFormat()) {
    case Image::Format::RGBA_DXT1:
        CompressDXT1(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::RGBA_DXT3:
        CompressDXT3(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::RGBA_DXT5:
        CompressDXT5(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::DXN2:
        CompressDXN2(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::RGB_8_ETC1:
        CompressETC1(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::RGB_8_ETC2:
        CompressETC2_RGB8(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::RGBA_8_1_ETC2:
        CompressETC2_RGBA1(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::RGBA_8_8_ETC2:
        CompressETC2_RGBA8(srcImage, dstImage, compressoinQuality);
        break;
    case Image::Format::SignedRG_11_11_EAC:
        CompressETC2_Signed_RG11(srcImage, dstImage, compressoinQuality);
        break;
    default:
        BE_WARNLOG("CompressImage: unsupported format %s\n", dstImage.FormatName());
        return false;
    }

    //uint64_t endClocks = rdtsc();
    //BE_LOG("%DXT compression in %" PRIu64 " clocks\n", endClocks - startClocks);

    return true;
}

bool Image::ConvertFormat(Image::Format::Enum dstFormat, Image &dstImage, bool regenerateMipmaps, Image::CompressionQuality::Enum compressionQuality) const {
    const Image *srcImage = this;
    const ImageFormatInfo *srcFormatInfo = GetImageFormatInfo(srcImage->GetFormat());
    const ImageFormatInfo *dstFormatInfo = GetImageFormatInfo(dstFormat);
    
    if (srcFormatInfo == dstFormatInfo) {
        dstImage = *srcImage;
        return true;
    }

    int numDstMipmaps = !regenerateMipmaps ? srcImage->numMipmaps : MaxMipMapLevels(srcImage->width, srcImage->height, srcImage->depth);
    
    // Create output image based on src (this) image
    dstImage.Create(srcImage->width, srcImage->height, srcImage->depth, srcImage->numSlices, numDstMipmaps, dstFormat, nullptr, srcImage->flags);

    // If source format is compressed, then decompress to RGBA_8_8_8_8
    Image decompressedImage;
    if (srcFormatInfo->type & FormatType::Compressed) {
        decompressedImage.Create(srcImage->width, srcImage->height, srcImage->depth, srcImage->numSlices, numDstMipmaps, Format::RGBA_8_8_8_8, nullptr, srcImage->flags);

        DecompressImage(*this, decompressedImage);

        if (regenerateMipmaps) {
            decompressedImage.GenerateMipmaps();
        }

        srcImage = &decompressedImage;
        srcFormatInfo = GetImageFormatInfo(Format::RGBA_8_8_8_8);
    } else {
        if (regenerateMipmaps) {
            decompressedImage.Create(srcImage->width, srcImage->height, srcImage->depth, srcImage->numSlices, numDstMipmaps, srcImage->format, nullptr, srcImage->flags);
            decompressedImage.CopyFrom(*srcImage, 0, 1);
            decompressedImage.GenerateMipmaps();

            srcImage = &decompressedImage;
        }
    }

    Image rgba8888Image;
    if (dstFormatInfo->type & FormatType::Compressed) {
        if (srcImage->GetFormat() != Format::RGBA_8_8_8_8) {
            srcImage->ConvertFormat(Format::RGBA_8_8_8_8, rgba8888Image);
            srcImage = &rgba8888Image;
            srcFormatInfo = GetImageFormatInfo(Format::RGBA_8_8_8_8);
        }

        if (!CompressImage(*srcImage, dstImage, compressionQuality)) {
            dstImage.Clear();
            return false;
        }

        return true;
    }

    ImageUnpackFunc unpackFunc;
    ImagePackFunc packFunc;

    bool unpackFloat = (srcFormatInfo->type & FormatType::Float) || (dstFormatInfo->type & FormatType::Float);

    if (unpackFloat) {
        unpackFunc = srcFormatInfo->unpackRGBA32F;
        packFunc = dstFormatInfo->packRGBA32F;
    } else {
        unpackFunc = srcFormatInfo->unpackRGBA8888;
        packFunc = dstFormatInfo->packRGBA8888;
    }
    
    if (!unpackFunc || !packFunc) {
        BE_WARNLOG("Image::ConvertFormat: unsupported convert type (from %s to %s)\n", srcImage->FormatName(), dstImage.FormatName());
        dstImage.Clear();
        return false;
    }

    bool isGamma = !(flags & Flag::LinearSpace);

    byte *unpackedBuffer = (byte *)Mem_Alloc16(width * 4 * (unpackFloat ? sizeof(float) : 1));

    byte *srcPtr = srcImage->GetPixels();
    byte *dstPtr = dstImage.GetPixels();

    for (int mipLevel = 0; mipLevel < srcImage->numMipmaps; mipLevel++) {
        int w = srcImage->GetWidth(mipLevel);
        int h = srcImage->GetHeight(mipLevel);
        int d = srcImage->GetDepth(mipLevel);

        int srcPitch = srcImage->BytesPerPixel() * w;
        int dstPitch = dstImage.BytesPerPixel() * w;

        for (int sliceIndex = 0; sliceIndex < srcImage->numSlices; sliceIndex++) {
            for (int y = 0; y < h * d; y++) {
                unpackFunc(srcPtr, unpackedBuffer, w, isGamma);
                packFunc(unpackedBuffer, dstPtr, w, isGamma);

                srcPtr += srcPitch;
                dstPtr += dstPitch;
            }
        }
    }

    Mem_AlignedFree(unpackedBuffer);
    return true;
}

bool Image::ConvertFormatSelf(Image::Format::Enum dstFormat, bool regenerateMipmaps, Image::CompressionQuality::Enum compressionQuality) {
    if (this->format == dstFormat) {
        return true;
    }

    Image dstImage;
    bool ret = ConvertFormat(dstFormat, dstImage, regenerateMipmaps, compressionQuality);
    if (ret) {
        *this = dstImage;
    }
    return ret;
}

BE_NAMESPACE_END
