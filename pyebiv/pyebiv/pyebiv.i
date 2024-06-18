/* File : pyebiv.i */

%module pyebiv
%{
     #define SWIG_FILE_WITH_INIT
     #include <vector>
     #include "pyebiv.h"
%}
%include std_string.i
%include std_vector.i
%include stdint.i
%include numpy.i
%include "typemaps.i"

%init %{
    import_array();
%}

namespace std {
	%template(IntVector)vector < int > ;
	%template(DoubleVector)vector < double > ;
	%template(FloatVector)vector < float > ;
}

// definition for function EBIV.size()
// returns [imgH,imgW]
%typemap(out) int* sensorSize {
  int i;
  //$1, $1_dim0, $1_dim1
  $result = PyList_New(2);
  for (i = 0; i < 2; i++) {
    PyObject *o = PyLong_FromLong((long) $1[i]);
    PyList_SetItem($result,i,o);
  }
  //delete $1; // Important to avoid a leak since you called new
}


// definition for putData()
// These names must exactly match the function declaration.
%apply (float* IN_ARRAY2, int DIM1, int DIM2) \
      {(float* npyArray2D, int npyLength1D, int npyLength2D)}
%apply (double* IN_ARRAY2, int DIM1, int DIM2) \
      {(double* npyArray2D, int npyLength1D, int npyLength2D)}

// finally include the header file
%include "pyebiv.h"
