#include "linalg.h"
#include "math.h"

int NumArrayDotCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
    Tcl_Obj *naObj1, *naObj2, *resultObj;
    NumArrayInfo *info1, *info2, *resultinfo;
    NumArraySharedBuffer *sharedbuf;
	
    if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 1, objv, "numarray1 numarray2");
	    return TCL_ERROR;
    }
    
    naObj1 = objv[1];
    naObj2 = objv[2];
    
    if (Tcl_ConvertToType(interp, naObj1, &NumArrayTclType) != TCL_OK) {
	    return TCL_ERROR;
    }

    if (Tcl_ConvertToType(interp, naObj2, &NumArrayTclType) != TCL_OK) {
	    return TCL_ERROR;
    }

    info1 = naObj1->internalRep.twoPtrValue.ptr2;
    info2 = naObj2->internalRep.twoPtrValue.ptr2;
	
	/* handle case of scalar multiplication */
	if ((info1 -> nDim == 1 && info1 -> dims[0] == 1)
		|| (info2 -> nDim == 1 && info2 -> dims[0] == 1)) {
		/* same as elementwise multiplication */
		return NumArrayTimesCmd(dummy, interp, objc, objv);
	}
/*
    if (info1 -> type != NumArray_Float64 || info2 -> type != NumArray_Float64) {
		Tcl_SetResult(interp, "Dot product implemented only for doubles", NULL);
		return TCL_ERROR;
	}
*/
	
	#define T1 int
	#define T2 int
	#define TRES int
	#include "dotproductloop.h"	
	
	#define T1 int
	#define T2 double
	#define TRES double
	#include "dotproductloop.h"	
	
	#define T1 double
	#define T2 int
	#define TRES double
	#include "dotproductloop.h"	
	
	#define T1 double
	#define T2 double
	#define TRES double
	#include "dotproductloop.h"	

	#define T1 int
	#define T2 NumArray_Complex
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 NumArray_Complex
	#define T2 int
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 double
	#define T2 NumArray_Complex
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 NumArray_Complex
	#define T2 double
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 NumArray_Complex
	#define T2 NumArray_Complex
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	
	{
		Tcl_SetResult(interp, "Unknown datatypes", NULL);
		return TCL_ERROR;
	}

	resultObj=Tcl_NewObj();
	NumArraySetInternalRep(resultObj, sharedbuf, resultinfo);
	Tcl_SetObjResult(interp, resultObj);

    return TCL_OK;
}

/* Compute QR factorization of matrix object
 * store result in qr and diagonal elements in rdiag 
 * qr and rdiag are new objects */
int QRDecompositionColMaj(Tcl_Interp * interp, Tcl_Obj *matrix, Tcl_Obj **qr, Tcl_Obj **rdiag) {
	if (Tcl_ConvertToType(interp, matrix, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
    
	NumArrayInfo *minfo = matrix -> internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *msharedbuf = matrix -> internalRep.twoPtrValue.ptr1;
	
	if (minfo -> type != NumArray_Float64) {
		Tcl_SetResult(interp, "Matrix factorization implemented only for doubles", NULL);
		return TCL_ERROR;
	}
	
	if (minfo -> nDim != 2) {
		Tcl_SetResult(interp, "not a two-dimensional matrix", NULL);
		return TCL_ERROR;
	}

	/* get dimensions */
	const int m = minfo -> dims[0];
	const int n = minfo -> dims[1];

	if (m < n) {
		Tcl_SetResult(interp, "matrix has more columns than rows in QR", NULL);
		return TCL_ERROR;
	}

	/* Create qr as a column major copy of matrix */
	*qr = Tcl_NewObj();
	NumArrayInfo *info = CreateNumArrayInfoColMaj(minfo->nDim, minfo->dims, NumArray_Float64);
	NumArraySharedBuffer *qrsharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	NumArrayCopy(minfo, msharedbuf, info, qrsharedbuf);
	void * bufptr = NumArrayGetPtrFromSharedBuffer(qrsharedbuf);
	NumArraySetInternalRep(*qr, qrsharedbuf, info);

	/* in column major form we use this pointer */
	double *QRbuf = (double *) bufptr;
	Tcl_InvalidateStringRep(*qr);
	

	/* simple 2D accessor for canonical buffer */
	#define QR(i, j) (QRbuf[i+j*m])

	/* alloc space for rdiag */
	*rdiag = Tcl_NewObj();
	NumArrayInfo *rdiaginfo = CreateNumArrayInfo(1, &n, info -> type);
	NumArraySharedBuffer *rsharedbuf = NumArrayNewSharedBuffer(rdiaginfo->bufsize);
	NumArraySetInternalRep(*rdiag, rsharedbuf, rdiaginfo);
	double *Rdiag = (double *)NumArrayGetPtrFromSharedBuffer(rsharedbuf);

	/* Main loop. */
	int k;
	for (k = 0; k < n; k++) {
		/* Compute 2-norm of k-th column without under/overflow. */
		double nrm = 0;
		int i;
		/* Use direct formula ? */
		for (i = k; i < m; i++) {
			nrm +=QR(i,k)*QR(i,k);
		}
		nrm = sqrt(nrm);

		if (nrm != 0.0) {
			/* Form k-th Householder vector. */
			if (QR(k,k) < 0) {	
				nrm = -nrm;
			}
			int i;
			for (i = k; i < m; i++) {
				QR(i,k) /= nrm;
			}
			QR(k,k) += 1.0;

			/* Apply transformation to remaining columns. */
			int j;
			for (j = k+1; j < n; j++) {
				double s = 0.0; 
				int i;
				for (i = k; i < m; i++) {
					s += QR(i,k)*QR(i,j);
				}
				s = -s/QR(k,k);
				for (i = k; i < m; i++) {
					QR(i,j) += s*QR(i, k);
				}
			}
		}
		Rdiag[k] = -nrm;
	}
	#undef QR	
	return TCL_OK;
}

/* Solve A*X = B for right-hand side B, if QR factorization is
 * already known. Return least-squares solution if n<m */
int QRsolveColMaj(Tcl_Interp *interp, Tcl_Obj *qr, Tcl_Obj *rdiag, Tcl_Obj *B) {
	
	NumArrayInfo *QRinfo = qr -> internalRep.twoPtrValue.ptr2;
	const int m = QRinfo -> dims[0];
	const int n = QRinfo -> dims[1];

	if (Tcl_ConvertToType(interp, B, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	NumArrayInfo *Binfo = B->internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *Bsharedbuf = B->internalRep.twoPtrValue.ptr1;
	
	if (Binfo -> nDim >2) {
		Tcl_SetResult(interp, "right-hand side must be a matrix or vector", NULL);
		return TCL_ERROR;
	}
	if (Binfo -> dims[0] != m) {
		Tcl_SetResult(interp, "matrix row dimensions must agree", NULL);
		return TCL_ERROR;
	}
	
	if (Binfo -> type != NumArray_Float64) {
		Tcl_SetResult(interp, "Matrix factorization implemented only for doubles", NULL);
		return TCL_ERROR;
	}

	/* simply ignore for now singular matrices
	 *
	if (!this.isFullRank()) {
		throw new RuntimeException("Matrix is rank deficient.");
	} */

	
	/* Copy right hand side in column major format */
	const int nx = Binfo -> nDim == 1 ? 1 : Binfo -> dims[1];
	Tcl_Obj *Xobj = Tcl_NewObj();
	NumArrayInfo *Xinfo = CreateNumArrayInfoColMaj(Binfo->nDim, Binfo->dims, Binfo->type);
	NumArraySharedBuffer *Xsharedbuf = NumArrayNewSharedBuffer(Xinfo->bufsize);
	NumArrayCopy(Binfo, Bsharedbuf, Xinfo, Xsharedbuf);
	NumArraySetInternalRep(Xobj, Xsharedbuf, Xinfo);

	double *Xbuf, *QRbuf, *Rdiag;
	char *bufptr;
	
	NumArrayGetBufferFromObj(NULL, qr, &bufptr);
	QRbuf = (double*) bufptr;

	Xbuf = (double*) NumArrayGetPtrFromSharedBuffer(Xsharedbuf);

	NumArrayGetBufferFromObj(NULL, rdiag, &bufptr);
	Rdiag = (double *) bufptr;

	#define X(i, j)  (Xbuf[i+j*m])
	/* QR is in column major order */
	#define QR(i, j) (QRbuf[i+j*m])

	/* Compute Y = transpose(Q)*B */
	int k;
	for (k = 0; k < n; k++) {
		int j;
		for (j = 0; j < nx; j++) {
			double s = 0.0;
			int i;
			for (i = k; i < m; i++) {
				s += QR(i,k)*X(i,j);
			}
			s = -s/QR(k,k);
			for (i = k; i < m; i++) {
				X(i,j) += s*QR(i,k);
			}
		}
	}
	/* Solve R*X = Y */
	for (k = n-1; k >= 0; k--) {
		int j;
		for (j = 0; j < nx; j++) {
			X(k,j) /= Rdiag[k];
		}
		int i;
		for (i = 0; i < k; i++) {
			int j;
			for (j = 0; j < nx; j++) {
				X(i,j) -= X(k,j)*QR(i,k);
			}
		}
	}
/*	return (new Matrix(X,n,nx).getMatrix(0,n-1,0,nx-1)); */
	
	/* cut off only the first n rows of X
	 * Note: The memory is not released */
	Xinfo -> dims[0] = n;

	Tcl_SetObjResult(interp, Xobj);
	return TCL_OK;
}

int NumArrayQRecoCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
	Tcl_Obj *qr, *rdiag;
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	if (QRDecompositionColMaj(interp, objv[1], &qr, &rdiag) != TCL_OK) {
		return TCL_ERROR;
	}

	/* join the QR vector matrix and the diagonal elements
	 * int a list */
	Tcl_Obj* result = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, result, qr);
	Tcl_ListObjAppendElement(interp, result, rdiag);

	Tcl_SetObjResult(interp, result);

	return TCL_OK;
}

int NumArrayBackslashCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
	Tcl_Obj *qr, *rdiag;
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix rhs");
		return TCL_ERROR;
	}

	if (QRDecompositionColMaj(interp, objv[1], &qr, &rdiag) != TCL_OK) {
		return TCL_ERROR;
	}

	/* join the QR vector matrix and the diagonal elements
	 * int a list */
	if (QRsolveColMaj(interp, qr, rdiag, objv[2]) != TCL_OK) {
		return TCL_ERROR;
	}

	return TCL_OK;
}


