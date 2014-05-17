/* to be included with CMD and OP defined
 * it defines a reduction along the specified axis
 * which works by iterating over all elements
 * for compatible operands */

/* propagate definitions for INIT, FIRST, OP and RETURN
 * for all datatypes, if they are defined. Listen to your compilers
 * warnings about redefinition. */
#ifndef INTINIT
#define INTINIT INIT
#endif
#ifndef DBLINIT
#define DBLINIT INIT
#endif
#ifndef CPLXINIT
#define CPLXINIT INIT
#endif

#ifndef INTFIRST
#define INTFIRST FIRST
#endif
#ifndef DBLFIRST
#define DBLFIRST FIRST
#endif
#ifndef CPLXFIRST
#define CPLXFIRST FIRST
#endif

#ifdef OP
#define INTOP OP
#define DBLOP OP
#define CPLXOP OP
#endif

#ifndef INTRETURN
#define INTRETURN RETURN
#endif
#ifndef DBLRETURN
#define DBLRETURN RETURN
#endif
#ifndef CPLXRETURN
#define CPLXRETURN RETURN
#endif

/* Default values for the result type == input type */

#ifndef DBLRES
#define DBLRES double
#endif

#ifndef INTRES
#define INTRES NaWideInt
#endif

#ifndef CPLXRES
#define CPLXRES NumArray_Complex
#endif

int CMD( 
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{	
	Tcl_Obj *naObj, *resultObj;
	NumArrayInfo *info, *resultinfo, *sliceinfo;
	NumArraySharedBuffer *sharedbuf, *resultbuf;
	int axis;
	int *resultdims;
	int resultnDim;
	int d;
	
	if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray ?axis?");
		return TCL_ERROR;
	}
	
	naObj = objv[1];
	
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (objc == 2) {
		axis = 0;
	} else {
		if (Tcl_GetIntFromObj(interp, objv[2], &axis) != TCL_OK) {
			return TCL_ERROR;
		}
	}

	sharedbuf =  naObj->internalRep.twoPtrValue.ptr1;
	info = naObj->internalRep.twoPtrValue.ptr2;
	
	if (axis < 0 || axis >= info -> nDim) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		return TCL_ERROR;
	}

	if (ISEMPTYINFO(info)) {
		Tcl_SetResult(interp, "Empty array", NULL);
		return TCL_ERROR;
	}

	/* cut down dimension list by removing the axis
	 * over which the reduction is performed*/
	resultdims = ckalloc(sizeof(int)*info->nDim);
	if (info->nDim == 1) {
		/* Reduction to scalar value */
		resultnDim=1;
		resultdims[0]=1;
	} else {
		resultnDim = 0;
		for (d=0; d < info->nDim; d++) {
			if (axis==0 || d!=axis) {
				/* never cut the first axis, otherwise
				 * row vectors are converted to column vectors */
				resultdims[resultnDim] = info -> dims[d];
				resultnDim++;
			}
		}

		if (axis==0) {
			resultdims[axis]=1;
		}
	}

	switch (info->type) {
		#ifdef DBLOP
		case NumArray_Float64: {
			resultinfo = CreateNumArrayInfo(resultnDim, resultdims, NATYPE_FROM_C(DBLRES));
			break;
		}
		#endif
		
		#ifdef INTOP
		case NumArray_Int:
			resultinfo = CreateNumArrayInfo(resultnDim, resultdims, NATYPE_FROM_C(INTRES));
			break;
		#endif

		#ifdef CPLXOP
		case NumArray_Complex128: {
			resultinfo = CreateNumArrayInfo(resultnDim, resultdims, NATYPE_FROM_C(CPLXRES));
			break;
		}
		#endif

		default:
			
			RESULTPRINTF(("Undefined function for datatype %s", NumArray_typename[info->type]));
            return TCL_ERROR;
	}

	ckfree(resultdims);

	/* allocate buffer of this size */
	resultbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
	void *bufptr = NumArrayGetPtrFromSharedBuffer(resultbuf);
	
	/* compute slice for first element in the reduction 
	 * axis */
	sliceinfo = DupNumArrayInfo(info);
	NumArrayInfoSlice1Axis(NULL, sliceinfo, axis, 0, 0, 1);

	/* the new shared buffer is in canonical form, 
	 * thus we can simply iterate over it by pointer
	 * arithmetics. But the input array may be non-canonical
	 * TODO optimize for canonical case */

	NumArrayIterator it;
	NumArrayIteratorInit(sliceinfo, sharedbuf, &it);

	const int nlength = info -> dims[axis];
	const int increment = info -> pitches[axis];
	switch (info -> type) {
		#ifdef DBLOP
		case NumArray_Float64: {
			DBLRES *resultptr = bufptr;
			for (; ! NumArrayIteratorFinished(&it); 
				NumArrayIteratorAdvance(&it)) {
				char *opptr = NumArrayIteratorDeRefPtr(&it);
				DBLINIT;
				DBLRES accum;
				const double op = *((double *) opptr);
				DBLFIRST; 
				opptr += increment;
				int i;
				for (i=0; i<nlength-1; i++) {
					const double op = *((double *) opptr);
					DBLOP;
					/* increment along the selected axis */
					opptr += increment;
				}
				DBLRETURN
				*resultptr++ = accum; 
			}
			break;
		}
		#endif
		
		#ifdef INTOP
		case NumArray_Int: {
			INTRES *resultptr = bufptr;
			for (; ! NumArrayIteratorFinished(&it); 
				NumArrayIteratorAdvance(&it)) {
				char *opptr = NumArrayIteratorDeRefPtr(&it);
				INTINIT;
				INTRES accum;
				const NaWideInt op = *((NaWideInt *) opptr);
				INTFIRST; 
				opptr += increment;
				int i;
				for (i=0; i<nlength-1; i++) {
					const NaWideInt op = *((NaWideInt *) opptr);
					INTOP;
					/* increment along the selected axis */
					opptr += increment;
				}
				INTRETURN
				*resultptr++ = accum; 
			}
			break;
		}
		#endif
		
		#ifdef CPLXOP
		case NumArray_Complex128: {
			CPLXRES *resultptr = bufptr;
			for (; ! NumArrayIteratorFinished(&it); 
				NumArrayIteratorAdvance(&it)) {
				char *opptr = NumArrayIteratorDeRefPtr(&it);
				CPLXINIT;
				CPLXRES accum;
				const NumArray_Complex op = *((NumArray_Complex *) opptr);
				CPLXFIRST; 
				opptr += increment;
				int i;
				for (i=0; i<nlength-1; i++) {
					const NumArray_Complex op = *((NumArray_Complex *) opptr);
					CPLXOP;
					/* increment along the selected axis */
					opptr += increment;
				}
				CPLXRETURN
				*resultptr++ = accum; 
			}
			break;
		}
		#endif
		
		default:
			/* Can't happen */
			RESULTPRINTF(("Undefined function for datatype %s", NumArray_typename[info->type]));
            return TCL_ERROR;
	}

	NumArrayIteratorFree(&it);
	DeleteNumArrayInfo(sliceinfo);

	resultObj=Tcl_NewObj();
	NumArraySetInternalRep(resultObj, resultbuf, resultinfo);
	Tcl_SetObjResult(interp, resultObj);

	return TCL_OK;
}

#undef CMD
#undef OP
#undef RETURN
#undef FIRST
#undef INIT
#undef INTOP
#undef INTRETURN
#undef INTFIRST
#undef INTINIT
#undef DBLOP
#undef DBLRETURN
#undef DBLFIRST
#undef DBLINIT
#undef CPLXOP
#undef CPLXRETURN
#undef CPLXFIRST
#undef CPLXINIT
#undef INTRES
#undef DBLRES
#undef CPLXRES
