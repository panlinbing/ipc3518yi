/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
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
//   * The name of the copyright holders may not be used to endorse or promote products
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


#ifndef __OPENCV_CORE_C_H__
#define __OPENCV_CORE_C_H__

#include "opencv2/core/types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************************\
*          Array allocation, deallocation, initialization and access to elements         *
\****************************************************************************************/

/* <malloc> wrapper.
   If there is no enough memory, the function
   (as well as other OpenCV functions that call cvAlloc)
   raises an error. */
CVAPI(void*)  cvAlloc( size_t size );

/* <free> wrapper.
   Here and further all the memory releasing functions
   (that all call cvFree) take double pointer in order to
   to clear pointer to the data after releasing it.
   Passing pointer to NULL pointer is Ok: nothing happens in this case
*/
CVAPI(void)   cvFree_( void* ptr );
#define cvFree(ptr) (cvFree_(*(ptr)), *(ptr)=0)

/* Allocates and initializes IplImage header */
CVAPI(IplImage*)  cvCreateImageHeader( CvSize size, int depth, int channels );

/* Inializes IplImage header */
CVAPI(IplImage*) cvInitImageHeader( IplImage* image, CvSize size, int depth,
                                   int channels, int origin CV_DEFAULT(0),
                                   int align CV_DEFAULT(4));

/* Creates IPL image (header and data) */
CVAPI(IplImage*)  cvCreateImage( CvSize size, int depth, int channels );

/* Releases (i.e. deallocates) IPL image header */
CVAPI(void)  cvReleaseImageHeader( IplImage** image );

/* Releases IPL image header and data */
CVAPI(void)  cvReleaseImage( IplImage** image );

/* Allocates and initializes CvMat header */
CVAPI(CvMat*)  cvCreateMatHeader( int rows, int cols, int type );

#define CV_AUTOSTEP  0x7fffffff

/* Initializes CvMat header */
//CVAPI(CvMat*) cvInitMatHeader( CvMat* mat, int rows, int cols,
//                              int type, void* data CV_DEFAULT(NULL),
//                              int step CV_DEFAULT(CV_AUTOSTEP) );

/* Allocates and initializes CvMat header and allocates data */
CVAPI(CvMat*)  cvCreateMat( int rows, int cols, int type );

/* Releases CvMat header and deallocates matrix data
   (reference counting is used for data) */
CVAPI(void)  cvReleaseMat( CvMat** mat );

/* Decrements CvMat data reference counter and deallocates the data if
   it reaches 0 */
CV_INLINE  void  cvDecRefData( CvArr* arr )
{
    if( CV_IS_MAT( arr ))
    {
        CvMat* mat = (CvMat*)arr;
        mat->data.ptr = NULL;
        if( mat->refcount != NULL && --*mat->refcount == 0 )
            cvFree( &mat->refcount );
        mat->refcount = NULL;
    }
    else if( CV_IS_MATND( arr ))
    {
        CvMatND* mat = (CvMatND*)arr;
        mat->data.ptr = NULL;
        if( mat->refcount != NULL && --*mat->refcount == 0 )
            cvFree( &mat->refcount );
        mat->refcount = NULL;
    }
}

/* Increments CvMat data reference counter */
CV_INLINE  int  cvIncRefData( CvArr* arr )
{
    int refcount = 0;
    if( CV_IS_MAT( arr ))
    {
        CvMat* mat = (CvMat*)arr;
        if( mat->refcount != NULL )
            refcount = ++*mat->refcount;
    }
    else if( CV_IS_MATND( arr ))
    {
        CvMatND* mat = (CvMatND*)arr;
        if( mat->refcount != NULL )
            refcount = ++*mat->refcount;
    }
    return refcount;
}

/* Releases CvMatND */
CV_INLINE  void  cvReleaseMatND( CvMatND** mat )
{
    cvReleaseMat( (CvMat**)mat );
}

/* Creates a copy of CvMatND (except, may be, steps) */
//CVAPI(CvMatND*) cvCloneMatND( const CvMatND* mat );

/* Allocates and initializes CvSparseMat header and allocates data */
//CVAPI(CvSparseMat*)  cvCreateSparseMat( int dims, const int* sizes, int type );

/* Releases CvSparseMat */
CVAPI(void)  cvReleaseSparseMat( CvSparseMat** mat );

/* Creates a copy of CvSparseMat (except, may be, zero items) */
//CVAPI(CvSparseMat*) cvCloneSparseMat( const CvSparseMat* mat );

/* Initializes sparse array iterator
   (returns the first node or NULL if the array is empty) */
//CVAPI(CvSparseNode*) cvInitSparseMatIterator( const CvSparseMat* mat,
//                                              CvSparseMatIterator* mat_iterator );

// returns next sparse array node (or NULL if there is no more nodes)
CV_INLINE CvSparseNode* cvGetNextSparseNode( CvSparseMatIterator* mat_iterator )
{
    if( mat_iterator->node->next )
        return mat_iterator->node = mat_iterator->node->next;
    else
    {
        int idx;
        for( idx = ++mat_iterator->curidx; idx < mat_iterator->mat->hashsize; idx++ )
        {
            CvSparseNode* node = (CvSparseNode*)mat_iterator->mat->hashtable[idx];
            if( node )
            {
                mat_iterator->curidx = idx;
                return mat_iterator->node = node;
            }
        }
        return NULL;
    }
}

/**************** matrix iterator: used for n-ary operations on dense arrays *********/

#define CV_MAX_ARR 10

typedef struct CvNArrayIterator
{
    int count; /* number of arrays */
    int dims; /* number of dimensions to iterate */
    CvSize size; /* maximal common linear size: { width = size, height = 1 } */
    uchar* ptr[CV_MAX_ARR]; /* pointers to the array slices */
    int stack[CV_MAX_DIM]; /* for internal use */
    CvMatND* hdr[CV_MAX_ARR]; /* pointers to the headers of the
                                 matrices that are processed */
}
CvNArrayIterator;

#define CV_NO_DEPTH_CHECK     1
#define CV_NO_CN_CHECK        2
#define CV_NO_SIZE_CHECK      4

#define cvReshapeND( arr, header, new_cn, new_dims, new_sizes )   \
      cvReshapeMatND( (arr), sizeof(*(header)), (header),         \
                      (new_cn), (new_dims), (new_sizes))

/* Releases array data */
CVAPI(void)  cvReleaseData( CvArr* arr );


#define cvCvtScale cvConvertScale
#define cvScale  cvConvertScale
#define cvConvert( src, dst )  cvConvertScale( (src), (dst), 1, 0 )



#define CV_CMP_EQ   0
#define CV_CMP_GT   1
#define CV_CMP_GE   2
#define CV_CMP_LT   3
#define CV_CMP_LE   4
#define CV_CMP_NE   5


#define CV_SORT_EVERY_ROW 0
#define CV_SORT_EVERY_COLUMN 1
#define CV_SORT_ASCENDING 0
#define CV_SORT_DESCENDING 16

CVAPI(void) cvSort( const CvArr* src, CvArr* dst CV_DEFAULT(NULL),
                    CvArr* idxmat CV_DEFAULT(NULL),
                    int flags CV_DEFAULT(0));

/****************************************************************************************\
*                                Matrix operations                                       *
\****************************************************************************************/

/* Calculates cross product of two 3d vectors */
CVAPI(void)  cvCrossProduct( const CvArr* src1, const CvArr* src2, CvArr* dst );

/* Matrix transform: dst = A*B + C, C is optional */
#define cvMatMulAdd( src1, src2, src3, dst ) cvGEMM( (src1), (src2), 1., (src3), 1., (dst), 0 )
#define cvMatMul( src1, src2, dst )  cvMatMulAdd( (src1), (src2), NULL, (dst))

#define CV_GEMM_A_T 1
#define CV_GEMM_B_T 2
#define CV_GEMM_C_T 4

/* Tranposes matrix. Square matrices can be transposed in-place */
CVAPI(void)  cvTranspose( const CvArr* src, CvArr* dst );
#define cvT cvTranspose

/* Completes the symmetric matrix from the lower (LtoR=0) or from the upper (LtoR!=0) part */
CVAPI(void)  cvCompleteSymm( CvMat* matrix, int LtoR CV_DEFAULT(0) );

#define CV_SVD_MODIFY_A   1
#define CV_SVD_U_T        2
#define CV_SVD_V_T        4

#define CV_LU  0
#define CV_SVD 1
#define CV_SVD_SYM 2
#define CV_CHOLESKY 3
#define CV_QR  4
#define CV_NORMAL 16

/* Makes an identity matrix (mat_ij = i == j) */
CVAPI(void)  cvSetIdentity( CvArr* mat, CvScalar value CV_DEFAULT(cvRealScalar(1)) );

/* Fills matrix with given range of numbers */
CVAPI(CvArr*)  cvRange( CvArr* mat, double start, double end );

/* Calculates covariation matrix for a set of vectors */
/* transpose([v1-avg, v2-avg,...]) * [v1-avg,v2-avg,...] */
#define CV_COVAR_SCRAMBLED 0

/* [v1-avg, v2-avg,...] * transpose([v1-avg,v2-avg,...]) */
#define CV_COVAR_NORMAL    1

/* do not calc average (i.e. mean vector) - use the input vector instead
   (useful for calculating covariance matrix by parts) */
#define CV_COVAR_USE_AVG   2

/* scale the covariance matrix coefficients by number of the vectors */
#define CV_COVAR_SCALE     4

/* all the input vectors are stored in a single matrix, as its rows */
#define CV_COVAR_ROWS      8

/* all the input vectors are stored in a single matrix, as its columns */
#define CV_COVAR_COLS     16


#define CV_PCA_DATA_AS_ROW 0
#define CV_PCA_DATA_AS_COL 1
#define CV_PCA_USE_AVG 2

/* types of array norm */
#define CV_C            1
#define CV_L1           2
#define CV_L2           4
#define CV_NORM_MASK    7
#define CV_RELATIVE     8
#define CV_DIFF         16
#define CV_MINMAX       32

#define CV_DIFF_C       (CV_DIFF | CV_C)
#define CV_DIFF_L1      (CV_DIFF | CV_L1)
#define CV_DIFF_L2      (CV_DIFF | CV_L2)
#define CV_RELATIVE_C   (CV_RELATIVE | CV_C)
#define CV_RELATIVE_L1  (CV_RELATIVE | CV_L1)
#define CV_RELATIVE_L2  (CV_RELATIVE | CV_L2)


#define CV_REDUCE_SUM 0
#define CV_REDUCE_AVG 1
#define CV_REDUCE_MAX 2
#define CV_REDUCE_MIN 3

CVAPI(void)  cvReduce( const CvArr* src, CvArr* dst, int dim CV_DEFAULT(-1),
                       int op CV_DEFAULT(CV_REDUCE_SUM) );

/****************************************************************************************\
*                      Discrete Linear Transforms and Related Functions                  *
\****************************************************************************************/

#define CV_DXT_FORWARD  0
#define CV_DXT_INVERSE  1
#define CV_DXT_SCALE    2 /* divide result by size of array */
#define CV_DXT_INV_SCALE (CV_DXT_INVERSE + CV_DXT_SCALE)
#define CV_DXT_INVERSE_SCALE CV_DXT_INV_SCALE
#define CV_DXT_ROWS     4 /* transform each row individually */
#define CV_DXT_MUL_CONJ 8 /* conjugate the second argument of cvMulSpectrums */

/* Discrete Fourier Transform:
    complex->complex,
    real->ccs (forward),
    ccs->real (inverse) */
CVAPI(void)  cvDFT( const CvArr* src, CvArr* dst, int flags,
                    int nonzero_rows CV_DEFAULT(0) );
#define cvFFT cvDFT

/* Multiply results of DFTs: DFT(X)*DFT(Y) or DFT(X)*conj(DFT(Y)) */
CVAPI(void)  cvMulSpectrums( const CvArr* src1, const CvArr* src2,
                             CvArr* dst, int flags );

/* Finds optimal DFT vector size >= size0 */
CVAPI(int)  cvGetOptimalDFTSize( int size0 );

/* Discrete Cosine Transform */
CVAPI(void)  cvDCT( const CvArr* src, CvArr* dst, int flags );

/* a < b ? -1 : a > b ? 1 : 0 */
typedef int (CV_CDECL* CvCmpFunc)(const void* a, const void* b, void* userdata );


/* Removes set element given its pointer */
CV_INLINE  void cvSetRemoveByPtr( CvSet* set_header, void* elem )
{
    CvSetElem* _elem = (CvSetElem*)elem;
    assert( _elem->flags >= 0 /*&& (elem->flags & CV_SET_ELEM_IDX_MASK) < set_header->total*/ );
    _elem->next_free = set_header->free_elems;
    _elem->flags = (_elem->flags & CV_SET_ELEM_IDX_MASK) | CV_SET_ELEM_FREE_FLAG;
    set_header->free_elems = _elem;
    set_header->active_count--;
}

/* Retrieves graph vertex by given index */
#define cvGetGraphVtx( graph, idx ) (CvGraphVtx*)cvGetSetElem((CvSet*)(graph), (idx))

/* Retrieves index of a graph vertex given its pointer */
#define cvGraphVtxIdx( graph, vtx ) ((vtx)->flags & CV_SET_ELEM_IDX_MASK)

/* Retrieves index of a graph edge given its pointer */
#define cvGraphEdgeIdx( graph, edge ) ((edge)->flags & CV_SET_ELEM_IDX_MASK)

#define cvGraphGetVtxCount( graph ) ((graph)->active_count)
#define cvGraphGetEdgeCount( graph ) ((graph)->edges->active_count)

#define  CV_GRAPH_VERTEX        1
#define  CV_GRAPH_TREE_EDGE     2
#define  CV_GRAPH_BACK_EDGE     4
#define  CV_GRAPH_FORWARD_EDGE  8
#define  CV_GRAPH_CROSS_EDGE    16
#define  CV_GRAPH_ANY_EDGE      30
#define  CV_GRAPH_NEW_TREE      32
#define  CV_GRAPH_BACKTRACKING  64
#define  CV_GRAPH_OVER          -1

#define  CV_GRAPH_ALL_ITEMS    -1

/* flags for graph vertices and edges */
#define  CV_GRAPH_ITEM_VISITED_FLAG  (1 << 30)
#define  CV_IS_GRAPH_VERTEX_VISITED(vtx) \
    (((CvGraphVtx*)(vtx))->flags & CV_GRAPH_ITEM_VISITED_FLAG)
#define  CV_IS_GRAPH_EDGE_VISITED(edge) \
    (((CvGraphEdge*)(edge))->flags & CV_GRAPH_ITEM_VISITED_FLAG)
#define  CV_GRAPH_SEARCH_TREE_NODE_FLAG   (1 << 29)
#define  CV_GRAPH_FORWARD_EDGE_FLAG       (1 << 28)

typedef struct CvGraphScanner
{
    CvGraphVtx* vtx;       /* current graph vertex (or current edge origin) */
    CvGraphVtx* dst;       /* current graph edge destination vertex */
    CvGraphEdge* edge;     /* current edge */

    CvGraph* graph;        /* the graph */
    CvSeq*   stack;        /* the graph vertex stack */
    int      index;        /* the lower bound of certainly visited vertices */
    int      mask;         /* event mask */
}
CvGraphScanner;

/****************************************************************************************\
*                                     Drawing                                            *
\****************************************************************************************/

/****************************************************************************************\
*       Drawing functions work with images/matrices of arbitrary type.                   *
*       For color images the channel order is BGR[A]                                     *
*       Antialiasing is supported only for 8-bit image now.                              *
*       All the functions include parameter color that means rgb value (that may be      *
*       constructed with CV_RGB macro) for color images and brightness                   *
*       for grayscale images.                                                            *
*       If a drawn figure is partially or completely outside of the image, it is clipped.*
\****************************************************************************************/

#define CV_RGB( r, g, b )  cvScalar( (b), (g), (r), 0 )
#define CV_FILLED -1

#define CV_AA 16

#define cvDrawRect cvRectangle
#define cvDrawLine cvLine
#define cvDrawCircle cvCircle
#define cvDrawEllipse cvEllipse
#define cvDrawPolyLine cvPolyLine

/* Moves iterator to the next line point */
#define CV_NEXT_LINE_POINT( line_iterator )                     \
{                                                               \
    int _line_iterator_mask = (line_iterator).err < 0 ? -1 : 0; \
    (line_iterator).err += (line_iterator).minus_delta +        \
        ((line_iterator).plus_delta & _line_iterator_mask);     \
    (line_iterator).ptr += (line_iterator).minus_step +         \
        ((line_iterator).plus_step & _line_iterator_mask);      \
}


/* basic font types */
#define CV_FONT_HERSHEY_SIMPLEX         0
#define CV_FONT_HERSHEY_PLAIN           1
#define CV_FONT_HERSHEY_DUPLEX          2
#define CV_FONT_HERSHEY_COMPLEX         3
#define CV_FONT_HERSHEY_TRIPLEX         4
#define CV_FONT_HERSHEY_COMPLEX_SMALL   5
#define CV_FONT_HERSHEY_SCRIPT_SIMPLEX  6
#define CV_FONT_HERSHEY_SCRIPT_COMPLEX  7

/* font flags */
#define CV_FONT_ITALIC                 16

#define CV_FONT_VECTOR0    CV_FONT_HERSHEY_SIMPLEX


/* Font structure */
typedef struct CvFont
{
  const char* nameFont;   //Qt:nameFont
  CvScalar color;       //Qt:ColorFont -> cvScalar(blue_component, green_component, red\_component[, alpha_component])
    int         font_face;    //Qt: bool italic         /* =CV_FONT_* */
    const int*  ascii;      /* font data and metrics */
    const int*  greek;
    const int*  cyrillic;
    float       hscale, vscale;
    float       shear;      /* slope coefficient: 0 - normal, >0 - italic */
    int         thickness;    //Qt: weight               /* letters thickness */
    float       dx;       /* horizontal interval between letters */
    int         line_type;    //Qt: PointSize
}
CvFont;


/******************* Iteration through the sequence tree *****************/
typedef struct CvTreeNodeIterator
{
    const void* node;
    int level;
    int max_level;
}
CvTreeNodeIterator;


/* The function implements the K-means algorithm for clustering an array of sample
   vectors in a specified number of classes */
#define CV_KMEANS_USE_INITIAL_LABELS    1
CVAPI(int) cvKMeans2( const CvArr* samples, int cluster_count, CvArr* labels,
                      CvTermCriteria termcrit, int attempts CV_DEFAULT(1),
                      CvRNG* rng CV_DEFAULT(0), int flags CV_DEFAULT(0),
                      CvArr* _centers CV_DEFAULT(0), double* compactness CV_DEFAULT(0) );

/****************************************************************************************\
*                                    System functions                                    *
\****************************************************************************************/

/* Add the function pointers table with associated information to the IPP primitives list */
//CVAPI(int)  cvRegisterModule( const CvModuleInfo* module_info );

/* Loads optimized functions from IPP, MKL etc. or switches back to pure C code */
CVAPI(int)  cvUseOptimized( int on_off );

/* Retrieves information about the registered modules and loaded optimized plugins */
//CVAPI(void)  cvGetModuleInfo( const char* module_name,
//                              const char** version,
//                              const char** loaded_addon_plugins );

typedef void* (CV_CDECL *CvAllocFunc)(size_t size, void* userdata);
typedef int (CV_CDECL *CvFreeFunc)(void* pptr, void* userdata);

/* Set user-defined memory managment functions (substitutors for malloc and free) that
   will be called by cvAlloc, cvFree and higher-level functions (e.g. cvCreateImage) */
CVAPI(void) cvSetMemoryManager( CvAllocFunc alloc_func CV_DEFAULT(NULL),
                               CvFreeFunc free_func CV_DEFAULT(NULL),
                               void* userdata CV_DEFAULT(NULL));


typedef IplImage* (CV_STDCALL* Cv_iplCreateImageHeader)
                            (int,int,int,char*,char*,int,int,int,int,int,
                            IplROI*,IplImage*,void*,IplTileInfo*);
typedef void (CV_STDCALL* Cv_iplAllocateImageData)(IplImage*,int,int);
typedef void (CV_STDCALL* Cv_iplDeallocate)(IplImage*,int);
typedef IplROI* (CV_STDCALL* Cv_iplCreateROI)(int,int,int,int,int);
typedef IplImage* (CV_STDCALL* Cv_iplCloneImage)(const IplImage*);




/********************************** High-level functions ********************************/

CV_INLINE int cvReadInt( const CvFileNode* node, int default_value CV_DEFAULT(0) )
{
    return !node ? default_value :
        CV_NODE_IS_INT(node->tag) ? node->data.i :
        CV_NODE_IS_REAL(node->tag) ? cvRound(node->data.f) : 0x7fffffff;
}


CV_INLINE double cvReadReal( const CvFileNode* node, double default_value CV_DEFAULT(0.) )
{
    return !node ? default_value :
        CV_NODE_IS_INT(node->tag) ? (double)node->data.i :
        CV_NODE_IS_REAL(node->tag) ? node->data.f : 1e300;
}


CV_INLINE const char* cvReadString( const CvFileNode* node,
                        const char* default_value CV_DEFAULT(NULL) )
{
    return !node ? default_value : CV_NODE_IS_STRING(node->tag) ? node->data.str.ptr : 0;
}

/*********************************** Measuring Execution Time ***************************/

/* helper functions for RNG initialization and accurate time measurement:
   uses internal clock counter on x86 */
CVAPI(int64)  cvGetTickCount( void );
CVAPI(double) cvGetTickFrequency( void );

/*********************************** CPU capabilities ***********************************/

#define CV_CPU_NONE    0
#define CV_CPU_MMX     1
#define CV_CPU_SSE     2
#define CV_CPU_SSE2    3
#define CV_CPU_SSE3    4
#define CV_CPU_SSSE3   5
#define CV_CPU_SSE4_1  6
#define CV_CPU_SSE4_2  7
#define CV_CPU_POPCNT  8
#define CV_CPU_AVX    10
#define CV_CPU_AVX2   11
#define CV_HARDWARE_MAX_FEATURE 255

CVAPI(int) cvCheckHardwareSupport(int feature);


/********************************** Error Handling **************************************/

/* Get current OpenCV error status */
CVAPI(int) cvGetErrStatus( void );

/* Sets error status silently */
CVAPI(void) cvSetErrStatus( int status );

#define CV_ErrModeLeaf     0   /* Print error and exit program */
#define CV_ErrModeParent   1   /* Print error and continue */
#define CV_ErrModeSilent   2   /* Don't print and continue */

/* Retrives current error processing mode */
CVAPI(int)  cvGetErrMode( void );

/* Sets error processing mode, returns previously used mode */
CVAPI(int) cvSetErrMode( int mode );

/* Sets error status and performs some additonal actions (displaying message box,
 writing message to stderr, terminating application etc.)
 depending on the current error mode */
CVAPI(void) cvError( int status, const char* func_name,
                    const char* err_msg, const char* file_name, int line );

/* Retrieves textual description of the error given its code */
CVAPI(const char*) cvErrorStr( int status );

/* Retrieves detailed information about the last error occured */
CVAPI(int) cvGetErrInfo( const char** errcode_desc, const char** description,
                        const char** filename, int* line );

/* Maps IPP error codes to the counterparts from OpenCV */
CVAPI(int) cvErrorFromIppStatus( int ipp_status );

typedef int (CV_CDECL *CvErrorCallback)( int status, const char* func_name,
                                        const char* err_msg, const char* file_name, int line, void* userdata );

/* Assigns a new error-handling function */
CVAPI(CvErrorCallback) cvRedirectError( CvErrorCallback error_handler,
                                       void* userdata CV_DEFAULT(NULL),
                                       void** prev_userdata CV_DEFAULT(NULL) );

/*
 Output to:
 cvNulDevReport - nothing
 cvStdErrReport - console(fprintf(stderr,...))
 cvGuiBoxReport - MessageBox(WIN32)
 */
CVAPI(int) cvNulDevReport( int status, const char* func_name, const char* err_msg,
                          const char* file_name, int line, void* userdata );

CVAPI(int) cvStdErrReport( int status, const char* func_name, const char* err_msg,
                          const char* file_name, int line, void* userdata );

CVAPI(int) cvGuiBoxReport( int status, const char* func_name, const char* err_msg,
                          const char* file_name, int line, void* userdata );

#define OPENCV_ERROR(status,func,context)                           \
cvError((status),(func),(context),__FILE__,__LINE__)

#define OPENCV_ERRCHK(func,context)                                 \
{if (cvGetErrStatus() >= 0)                         \
{OPENCV_ERROR(CV_StsBackTrace,(func),(context));}}

#define OPENCV_ASSERT(expr,func,context)                            \
{if (! (expr))                                      \
{OPENCV_ERROR(CV_StsInternal,(func),(context));}}

#define OPENCV_RSTERR() (cvSetErrStatus(CV_StsOk))

#define OPENCV_CALL( Func )                                         \
{                                                                   \
Func;                                                           \
}


/* CV_FUNCNAME macro defines icvFuncName constant which is used by CV_ERROR macro */
#ifdef CV_NO_FUNC_NAMES
#define CV_FUNCNAME( Name )
#define cvFuncName ""
#else
#define CV_FUNCNAME( Name )  \
static char cvFuncName[] = Name
#endif


/*
 CV_ERROR macro unconditionally raises error with passed code and message.
 After raising error, control will be transferred to the exit label.
 */
#define CV_ERROR( Code, Msg )                                       \
{                                                                   \
    cvError( (Code), cvFuncName, Msg, __FILE__, __LINE__ );        \
    __CV_EXIT__;                                                   \
}

/* Simplified form of CV_ERROR */
#define CV_ERROR_FROM_CODE( code )   \
    CV_ERROR( code, "" )

/*
 CV_CHECK macro checks error status after CV (or IPL)
 function call. If error detected, control will be transferred to the exit
 label.
 */
#define CV_CHECK()                                                  \
{                                                                   \
    if( cvGetErrStatus() < 0 )                                      \
        CV_ERROR( CV_StsBackTrace, "Inner function failed." );      \
}


/*
 CV_CALL macro calls CV (or IPL) function, checks error status and
 signals a error if the function failed. Useful in "parent node"
 error procesing mode
 */
#define CV_CALL( Func )                                             \
{                                                                   \
    Func;                                                           \
    CV_CHECK();                                                     \
}


/* Runtime assertion macro */
#define CV_ASSERT( Condition )                                          \
{                                                                       \
    if( !(Condition) )                                                  \
        CV_ERROR( CV_StsInternal, "Assertion: " #Condition " failed" ); \
}

#define __CV_BEGIN__       {
#define __CV_END__         goto exit; exit: ; }
#define __CV_EXIT__        goto exit

#ifdef __cplusplus
}


#endif

#endif
