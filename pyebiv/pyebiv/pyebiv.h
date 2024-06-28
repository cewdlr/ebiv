#ifndef _PYEBIV_H_INCLUDED_
#define _PYEBIV_H_INCLUDED_

#ifdef PYBIND11
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // for std::vector
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>
namespace py = pybind11;
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#endif

#include <string>
#include <vector>
#include "ebi.h"

#ifndef API_CALL
# define API_CALL /* as nothing... */
#endif

extern void SetDebugLevel(const int nLevel);
extern int DebugLevel();

class API_CALL EBIV
{
public:
	EBIV();
	~EBIV();
	EBIV(const std::string& strFileName);
	//EBIV(double* npyArray2D, int npyLength1D, int npyLength2D);

	bool loadRaw(const std::string& strFileName);
	bool save(const std::string& strFileName, const uint32_t t0=0, const uint32_t duration=0);

	int width() const { return m_nImgWidth; }
	int height() const { return m_nImgHeight; }
	bool isNull();
	int64_t eventCount();
	int64_t timeStamp();
	std::vector<int32_t> sensorSize();

	//bool fromNumpyFloat(float* npyArray2D, int npyLength1D, int npyLength2D);
	//bool fromNumpy(double* npyArray2D, int npyLength1D, int npyLength2D);

	//std::vector<EBI::Event> events(); // return event data as list
	std::vector<int32_t> events(); // return event data as list of (t,x,y,p)
	std::vector<int32_t> time();
	std::vector<int32_t> x();
	std::vector<int32_t> y();
	std::vector<int32_t> p();

	std::vector<float> pseudoImage(const int32_t t0_usec, const int32_t duration, const int32_t polarity);

	int32_t estimatePulseOffsetTime(
		const double freqInHz,
		const int32_t nBinWidthInMicrosec,
		const int32_t nPeriods,
		const int32_t nStartPeriod);

	std::vector<double> meanPulseHistogram(
		const double freqInHz,
		const int32_t nBinWidthInMicrosec,
		const int32_t nPeriods,
		const int32_t nStartPeriod);

	void setDebugLevel(const int32_t nLevel);

#ifdef PYBIND11
	py::array_t<double> pseudoImagePyBind(const int32_t t0_usec, const int32_t duration, const int32_t polarity);
#endif

private:
	void init();
	bool alloc(const int nWidth, const int nHeight);
	int32_t m_nImgWidth, m_nImgHeight;
	//int m_imgDim[2];
	//unsigned char* m_pBits;		/*!< Pointer to actual image data */
	//size_t m_nBytesAllocated;
	int64_t m_nEventCount;
	int32_t m_nDebugLevel;

	EBI::EventData m_evData;
};

#endif // _PYEBIV_H_INCLUDED_
