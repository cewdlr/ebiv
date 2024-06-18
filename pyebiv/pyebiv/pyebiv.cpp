// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>
#include "pyebiv.h"

#include "ebi.h"
#include "ebi_image.h"

#include <errno.h>
#include <string>
#include <ostream>
#include <iostream>
using namespace std;

#define _MAX_BUF_SIZE 4096

#ifdef _DEBUG
static int __gDebugLevel = 1;
static bool __gWarningsEnabled = true;
#else
static int __gDebugLevel = 0;
static bool __gWarningsEnabled = false;
#endif
// todo: show message in python console
//void SendDebugMessageToOutputWindow(char const *msg)
//{
//	printf("LIBDPIV: %s\n", msg);
//}
void SendDebugMessageToOutputWindow(const std::string& buf)
{
	std::cout << buf.c_str() << std::endl;
}

//extern "C" int EXPORT_FXN DebugLevel()
extern int DebugLevel()
{
	return __gDebugLevel;
}

//extern "C" void EXPORT_FXN DebugPrintExt(const int nLevel, const char* msg, ...)
extern void DebugPrintExt(const int nLevel, const char* msg, ...)
{
	if (DebugLevel() < nLevel)
		return;
	static char buf[_MAX_BUF_SIZE];
	buf[0] = 0;
	va_list ap;
	va_start(ap, msg);			// use variable arg list
	vsprintf_s(buf, _MAX_BUF_SIZE, msg, ap);
	va_end(ap);
	printf(buf);
	printf("\n");
}

//extern "C" void EXPORT_FXN  SetDebugLevel(const int nLevel)
extern void SetDebugLevel(const int nLevel)
//!< Set greater than 0 to enable debugging, set to 0 to disable
{
	__gDebugLevel = nLevel;
	if (__gDebugLevel > 0) {
		DebugPrintExt(0, "pyIMX: Setting debugging to level %d", __gDebugLevel);
	}
	else {
		DebugPrintExt(0, "pyIMX: Disabling debugging messages");
	}
}


/*
Implementation of the EBIV class
*/
EBIV::EBIV()
{
	init();
}

/*!
Construct EBIV by loading specified file
*/
EBIV::EBIV(const std::string& strFileName)
{
	init();
	loadRaw(strFileName);
}

EBIV::~EBIV()
{
	// clean up stuff...
}

/*!
Enable debugging messages
*/
void EBIV::setDebugLevel(const int32_t nLevel)
{
	m_nDebugLevel = nLevel;
	std::cout << "pyEBIV: debugging set to level " << m_nDebugLevel << std::endl;
}

void EBIV::init()
{
	m_nImgWidth = m_nImgHeight = 0;
	m_imgDim[0] = 0;
	m_imgDim[1] = 0;
	m_nDebugLevel = 0;
}

int* EBIV::sensorSize()
{
	m_imgDim[1] = m_nImgWidth;
	m_imgDim[0] = m_nImgHeight;
	return m_imgDim;
}

bool EBIV::isNull()
{
	return m_evData.isNull();
}

int64_t EBIV::eventCount() 
{
	return static_cast<int64_t>(m_evData.data().size());
}

int64_t EBIV::timeStamp()
{
	return static_cast<int64_t>(m_evData.timeStamp());
}


bool EBIV::save(const std::string& strFileName, const uint32_t t0, const uint32_t duration)
{
	if (m_evData.save(strFileName, t0, duration)) {
		if (m_nDebugLevel > 0)
			std::cout << "Event data stored in " << strFileName.c_str() << std::endl;
		return true;
	}
	std::cerr << "Failed storing data in " << strFileName.c_str() << std::endl;
	return false;
}


bool EBIV::loadRaw(const std::string& strFileName)
{
	if(m_nDebugLevel > 0)
		std::cout << "loading event data from: " << strFileName << std::endl;

	if(!m_evData.load(strFileName))
		return false;

	if (m_nDebugLevel > 0) {
		std::cout << "Current number of events in file: " << (m_evData.data().size()) << std::endl;
		double msecs = static_cast<double>(m_evData.data()[m_evData.data().size() - 1].t - m_evData.data()[0].t) / 1000;
		std::cout << "duration: " << msecs << " millisec\n";
	}

	m_nImgWidth = m_evData.imageWidth();
	m_nImgHeight = m_evData.imageHeight();
	return false;
}

//bool EBIV::fromNumpy(double* npyArray2D, int npyLength1D, int npyLength2D)
//{
//	//if (!alloc(npyLength2D, npyLength1D))
//	//	return false;
//	//if ((npyLength2D != m_nWidth) || (npyLength1D != m_nHeight))
//	//	return false;
//	//float *ptr = (float*)m_pBits;
//	//uint32_t n = m_nWidth * m_nHeight;
//	//for (uint32_t i = 0; i < n; i++)
//	//	ptr[i] = (float)npyArray2D[i];
//	return true;
//}
//
//bool EBIV::fromNumpyFloat(float* npyArray2D, int npyLength1D, int npyLength2D)
//{
//	//if (!alloc(npyLength2D, npyLength1D))
//	//	return false;
//	//if ((npyLength2D != m_nWidth) || (npyLength1D != m_nHeight))
//	//	return false;
//	//float *ptr = (float*)m_pBits;
//	//uint32_t n = m_nWidth * m_nHeight;
//	//for (uint32_t i = 0; i < n; i++)
//	//	ptr[i] = npyArray2D[i];
//	return true;
//}

/*!
* \return events as int vector of length N*4
*/
std::vector<int32_t> EBIV::events()
{
	std::vector<int32_t> v;
	if (m_evData.isNull())
		return v;

	//std::cout << "copying events to vector Nx4: " << (n) << std::endl;
	size_t N = m_evData.data().size();
	v.resize(N*4);
	size_t ii = 0;
	for (size_t i = 0; i < N; i++) {
		EBI::Event ev = m_evData.dataRef()[i];
		v[ii++] = ev.t;
		v[ii++] = ev.x;
		v[ii++] = ev.y;
		v[ii++] = ev.p;
	}
	return v;
}

/*!
* \return event times as int vector
*/
std::vector<int32_t> EBIV::time()
{
	std::vector<int32_t> v;
	if (m_evData.isNull())
		return v;

	size_t N = m_evData.dataRef().size();
	v.resize(N);
	for (size_t i = 0; i < N; i++) {
		v[i] = m_evData.dataRef()[i].t;
	}
	//std::cout << "copying event times: " << (n) << std::endl;
	return v;
}

std::vector<int32_t> EBIV::x()
{
	/*!
	* \return event x-positions as int vector
	*/
	std::vector<int32_t> v;
	if (m_evData.isNull())
		return v;
	size_t N = m_evData.dataRef().size();
	v.resize(N);
	for (size_t i = 0; i < N; i++) {
		v[i] = m_evData.dataRef()[i].x;
	}
	return v;
}

std::vector<int32_t> EBIV::y()
{
	/*!
	* \return event y-positions as int vector
	*/
	std::vector<int32_t> v;
	if (m_evData.isNull())
		return v;
	size_t N = m_evData.dataRef().size();
	v.resize(N);
	for (size_t i = 0; i < N; i++) {
		v[i] = m_evData.dataRef()[i].y;
	}
	return v;
}

/*!
* \return event polarities as int vector
*/
std::vector<int32_t> EBIV::p()
{
	std::vector<int32_t> v;
	if (m_evData.isNull())
		return v;
	size_t N = m_evData.data().size();
	v.resize(N);
	for (size_t i = 0; i < N; i++) {
		v[i] = m_evData.dataRef()[i].p;
	}
	return v;
}

/*!
Create a pseudo-image from events in time-spam [t0,t0+duration) 
with time in microseconds
*/
std::vector<float> EBIV::pseudoImage(
	const int32_t t0_usec, 
	const int32_t duration,
	const int32_t polarity	//!< [1] uses positive events only, [-1] negative, [0] for both
	) 
{
	std::vector<float> v;
	if (m_evData.isNull())
		return v;
	EBI::EventPolarity evPol = EBI::PolarityBoth;
	if(polarity < 0)
		evPol = EBI::PolarityNegative;
	else if (polarity > 0)
		evPol = EBI::PolarityPositive;
	// extract slab from data set
	if (m_nDebugLevel > 0)
		std::cout << "pseudoImage(t0=" << t0_usec
			<< "  duration=" << duration 
			<< "  polarity=" << int(evPol) << ")" 
			<< std::endl;
	EBI::EventData evSlab(m_evData, evPol, t0_usec, duration);
	// convert to pseudo-image
	EBI::EventImage evImg(evSlab, evPol, 0, 0, 0, false);
	size_t N = evImg.width() * evImg.height();
	v.resize(N);
	//std::cout << "pseudoImage() - v resized to " << N << " elements" << std::endl;
	for (size_t i = 0; i < N; i++) {
		v[i] = evImg.dataRef()[i];
	}
	return v;
}

int32_t EBIV::estimatePulseOffsetTime(
	const double freqInHz,		//!< sampling frequency in [Hz] = light pulsing frequency
	const int32_t nBinWidth,	//!< in [usec] - value around 5-20usec to start
	const int32_t nPeriods,		//!< number of periods to sample (20-100)
	const int32_t nStartPeriod	//!< at which period to begin sampling (determines offset in data set)
)
{
	if (m_evData.isNull()) {
		if (m_nDebugLevel > 0) {
			std::cout << "ERROR - data is null!" << std::endl;
		}
		return 0;
	}
	return EBI::DetermineOffsetTime(
		m_evData, freqInHz, nBinWidth, nPeriods, nStartPeriod, m_nDebugLevel);
}
