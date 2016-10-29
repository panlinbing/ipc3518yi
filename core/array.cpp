/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

/* ////////////////////////////////////////////////////////////////////
//
//  CvMat, CvMatND, CvSparceMat and IplImage support functions
//  (creation, deletion, copying, retrieving and setting elements etc.)
//
// */

#include "precomp.hpp"


static struct
{
    Cv_iplCreateImageHeader  createHeader;
    Cv_iplAllocateImageData  allocateData;
    Cv_iplDeallocate  deallocate;
    Cv_iplCreateROI  createROI;
    Cv_iplCloneImage  cloneImage;
}
CvIPL;

/****************************************************************************************\
*                               CvMat creation and basic operations                      *
\****************************************************************************************/

// Creates CvMat and underlying data
CV_IMPL CvMat*
cvCreateMat( int height, int width, int type )
{
////printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    CvMat* arr = cvCreateMatHeader( height, width, type );
//    cvCreateData( arr );

    return arr;
}


static void icvCheckHuge( CvMat* arr )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    if( (int64)arr->step*arr->rows > INT_MAX )
        arr->type &= ~CV_MAT_CONT_FLAG;
}

// Creates CvMat header only
CV_IMPL CvMat*
cvCreateMatHeader( int rows, int cols, int type )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    type = CV_MAT_TYPE(type);

    if( rows < 0 || cols <= 0 )
        CV_Error( CV_StsBadSize, "Non-positive width or height" );

    int min_step = CV_ELEM_SIZE(type)*cols;
    if( min_step <= 0 )
        CV_Error( CV_StsUnsupportedFormat, "Invalid matrix type" );

    CvMat* arr = (CvMat*)cvAlloc( sizeof(*arr));

    arr->step = min_step;
    arr->type = CV_MAT_MAGIC_VAL | type | CV_MAT_CONT_FLAG;
    arr->rows = rows;
    arr->cols = cols;
    arr->data.ptr = 0;
    arr->refcount = 0;
    arr->hdr_refcount = 1;

    icvCheckHuge( arr );
    return arr;
}


// Deallocates the CvMat structure and underlying data
CV_IMPL void
cvReleaseMat( CvMat** array )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    if( !array )
        CV_Error( CV_HeaderIsNull, "" );

    if( *array )
    {
        CvMat* arr = *array;

        if( !CV_IS_MAT_HDR_Z(arr) && !CV_IS_MATND_HDR(arr) )
            CV_Error( CV_StsBadFlag, "" );

        *array = 0;

        cvDecRefData( arr );
        cvFree( &arr );
    }
}



/****************************************************************************************\
*                            CvSparseMat creation and basic operations                   *
\****************************************************************************************/

// Creates CvMatND and underlying data
CV_IMPL void
cvReleaseSparseMat( CvSparseMat** array )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    if( !array )
        CV_Error( CV_HeaderIsNull, "" );

    if( *array )
    {
        CvSparseMat* arr = *array;

        if( !CV_IS_SPARSE_MAT_HDR(arr) )
            CV_Error( CV_StsBadFlag, "" );

        *array = 0;

//        CvMemStorage* storage = arr->heap->storage;
//        cvReleaseMemStorage( &storage );
        cvFree( &arr->hashtable );
        cvFree( &arr );
    }
}


// Deallocates array's data
CV_IMPL void
cvReleaseData( CvArr* arr )
{
		//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    if( CV_IS_MAT_HDR( arr ) || CV_IS_MATND_HDR( arr ))
    {
        CvMat* mat = (CvMat*)arr;
        cvDecRefData( mat );
    }
    else if( CV_IS_IMAGE_HDR( arr ))
    {
        IplImage* img = (IplImage*)arr;

        if( !CvIPL.deallocate )
        {
            char* ptr = img->imageDataOrigin;
            img->imageData = img->imageDataOrigin = 0;
            cvFree( &ptr );
        }
        else
        {
            CvIPL.deallocate( img, IPL_IMAGE_DATA );
        }
    }
    else
        CV_Error( CV_StsBadArg, "unrecognized or unsupported array type" );
}


static  void
icvGetColorModel( int nchannels, const char** colorModel, const char** channelSeq )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    static const char* tab[][2] =
    {
        {"GRAY", "GRAY"},
        {"",""},
        {"RGB","BGR"},
        {"RGB","BGRA"}
    };

    nchannels--;
    *colorModel = *channelSeq = "";

    if( (unsigned)nchannels <= 3 )
    {
        *colorModel = tab[nchannels][0];
        *channelSeq = tab[nchannels][1];
    }
}


// create IplImage header
CV_IMPL IplImage *
cvCreateImageHeader( CvSize size, int depth, int channels )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    IplImage *img = 0;

    if( !CvIPL.createHeader )
    {
        img = (IplImage *)cvAlloc( sizeof( *img ));
        cvInitImageHeader( img, size, depth, channels, IPL_ORIGIN_TL,
                                    CV_DEFAULT_IMAGE_ROW_ALIGN );
    }
    else
    {
        const char *colorModel, *channelSeq;

        icvGetColorModel( channels, &colorModel, &channelSeq );

        img = CvIPL.createHeader( channels, 0, depth, (char*)colorModel, (char*)channelSeq,
                                  IPL_DATA_ORDER_PIXEL, IPL_ORIGIN_TL,
                                  CV_DEFAULT_IMAGE_ROW_ALIGN,
                                  size.width, size.height, 0, 0, 0, 0 );
    }

    return img;
}


// create IplImage header and allocate underlying data
CV_IMPL IplImage *
cvCreateImage( CvSize size, int depth, int channels )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    IplImage *img = cvCreateImageHeader( size, depth, channels );
    assert( img );
//    cvCreateData( img );

    return img;
}


// initialize IplImage header, allocated by the user
CV_IMPL IplImage*
cvInitImageHeader( IplImage * image, CvSize size, int depth,
                   int channels, int origin, int align )
{
//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    const char *colorModel, *channelSeq;

    if( !image )
        CV_Error( CV_HeaderIsNull, "null pointer to header" );

    memset( image, 0, sizeof( *image ));
    image->nSize = sizeof( *image );

    icvGetColorModel( channels, &colorModel, &channelSeq );
    strncpy( image->colorModel, colorModel, 4 );
    strncpy( image->channelSeq, channelSeq, 4 );

    if( size.width < 0 || size.height < 0 )
        CV_Error( CV_BadROISize, "Bad input roi" );

    if( (depth != (int)IPL_DEPTH_1U && depth != (int)IPL_DEPTH_8U &&
         depth != (int)IPL_DEPTH_8S && depth != (int)IPL_DEPTH_16U &&
         depth != (int)IPL_DEPTH_16S && depth != (int)IPL_DEPTH_32S &&
         depth != (int)IPL_DEPTH_32F && depth != (int)IPL_DEPTH_64F) ||
         channels < 0 )
        CV_Error( CV_BadDepth, "Unsupported format" );
    if( origin != CV_ORIGIN_BL && origin != CV_ORIGIN_TL )
        CV_Error( CV_BadOrigin, "Bad input origin" );

    if( align != 4 && align != 8 )
        CV_Error( CV_BadAlign, "Bad input align" );

    image->width = size.width;
    image->height = size.height;

    if( image->roi )
    {
        image->roi->coi = 0;
        image->roi->xOffset = image->roi->yOffset = 0;
        image->roi->width = size.width;
        image->roi->height = size.height;
    }

    image->nChannels = MAX( channels, 1 );
    image->depth = depth;
    image->align = align;
    image->widthStep = (((image->width * image->nChannels *
         (image->depth & ~IPL_DEPTH_SIGN) + 7)/8)+ align - 1) & (~(align - 1));
    image->origin = origin;
    image->imageSize = image->widthStep * image->height;

    return image;
}


CV_IMPL void
cvReleaseImageHeader( IplImage** image )
{
		//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    if( !image )
        CV_Error( CV_StsNullPtr, "" );

    if( *image )
    {
        IplImage* img = *image;
        *image = 0;

        if( !CvIPL.deallocate )
        {
            cvFree( &img->roi );
            cvFree( &img );
        }
        else
        {
            CvIPL.deallocate( img, IPL_IMAGE_HEADER | IPL_IMAGE_ROI );
        }
    }
}


CV_IMPL void
cvReleaseImage( IplImage ** image )
{
		//printf(" %s - %s - %d\n", __func__, __FILE__, __LINE__);
    if( !image )
        CV_Error( CV_StsNullPtr, "" );

    if( *image )
    {
        IplImage* img = *image;
        *image = 0;

        cvReleaseData( img );
        cvReleaseImageHeader( &img );
    }
}

/****************************************************************************************\
*                            Additional operations on CvTermCriteria                     *
\****************************************************************************************/

namespace cv
{

template<> void Ptr<CvMat>::delete_obj()
{ cvReleaseMat(&obj); }

template<> void Ptr<IplImage>::delete_obj()
{ cvReleaseImage(&obj); }

template<> void Ptr<CvMatND>::delete_obj()
{ cvReleaseMatND(&obj); }

template<> void Ptr<CvSparseMat>::delete_obj()
{ cvReleaseSparseMat(&obj); }

//template<> void Ptr<CvMemStorage>::delete_obj()
//{ cvReleaseMemStorage(&obj); }

//template<> void Ptr<CvFileStorage>::delete_obj()
//{ cvReleaseFileStorage(&obj); }

}

/* End of file. */
