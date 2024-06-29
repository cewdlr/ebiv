#include "ebi.h"
#include "ebi_image.h"
#include <iostream>
#include <fstream>

#ifdef LIBTIFF
# include "tiffio.h"
#endif

EBI::EventImage::EventImage()
{
	init();

}

/*!
Construct pseudo-image from event data 
*/
EBI::EventImage::EventImage(const EBI::EventData& src, 
	const EBI::EventPolarity polMode,
	const uint32_t offsetUSec, const uint32_t durationUSec, 
	const int nRefTimeUSec,
	const bool bSumEvents)
{
	init();
	fromEventData(src, polMode, offsetUSec, durationUSec, nRefTimeUSec, bSumEvents);
}

void EBI::EventImage::init()
{
	clear();
}

/*!
Enable debugging messages
*/
void EBI::EventImage::setDebugLevel(const int32_t nLevel)
{
	m_nDebugLevel = nLevel;
	if (m_nDebugLevel > 0)
		std::cout << "EBI::EventImage: debugging set to level " << m_nDebugLevel << std::endl;
}

void EBI::EventImage::clear()
{
	m_imgHeight = m_imgWidth = 0;
	m_duration = 0;
	m_eventsUsed = 0;
	m_refTime = 0;
	m_imgData.resize(0);

	m_statsMean = m_statsVar = m_statsMin = m_statsMax = 0;
	m_bNeedStats = false;
}

bool EBI::EventImage::alloc(const EBI::EventData& src)
{
	uint32_t h = src.m_camSpecs.sensorH;
	uint32_t w = src.m_camSpecs.sensorW;
	if ((h == 0) || (w == 0)) {
		std::cerr << "EBI::EventImage::alloc() - failed allocating space for image" << std::endl;
			return false;
	}
	m_imgHeight = h;
	m_imgWidth = w;
	m_imgData.resize(w*h);
	for (size_t i = 0; i < (w*h); i++)
		m_imgData[i] = 0;
	m_statsMean = m_statsVar = m_statsMin = m_statsMax = 0;
	m_bNeedStats = false;
	return true;
}

bool EBI::EventImage::isNull() const
{
	if ((m_imgData.size() == 0)
		|| (m_imgHeight < 2)
		|| (m_imgWidth < 2))
		return true;
	return false;
}

void EBI::EventImage::setReferenceTime(const int32_t refTimeUSec)
{
	m_refTime = refTimeUSec;
}

int32_t EBI::EventImage::referenceTime() const
{
	return m_refTime;
}

bool EBI::EventImage::addFromEventData(const EBI::EventData& dataIN, const EBI::EventPolarity polMode, const bool bSumEvents)
{
	if ((m_imgHeight == 0) || (m_imgWidth == 0)) {
		if (!alloc(dataIN))
			return false;
	}
	if ((m_imgHeight != dataIN.m_camSpecs.sensorH)
		|| (m_imgWidth != dataIN.m_camSpecs.sensorW)) {
		std::cerr << "EBI::EventImage::addFromEventData() - size mismatch" << std::endl;
		return false;
	}
	// fill image
	for (EBI::Event ev : dataIN.m_events) {
		uint32_t ixy = (ev.y * m_imgWidth) + ev.x;
		// TODO: choice of how to encode intensity
		float val = m_imgData[ixy];
		switch (polMode) {
		case EBI::PolarityNegative:
			if (ev.p == 0) {
				val = static_cast<float>(ev.t);
				m_eventsUsed++;
			}
			if (bSumEvents)
				m_imgData[ixy]++;
			else
				m_imgData[ixy] = val;
			break;
		case EBI::PolarityBoth:
			val = static_cast<float>(ev.t);
			m_eventsUsed++;
			if (bSumEvents)
				m_imgData[ixy]++;
			else
				// place newer events on top of older ones (overwrite pixel value)
				m_imgData[ixy] = val;
			break;
		case EBI::PolarityPositive:
		default:
			if (ev.p > 0) {
				val = static_cast<float>(ev.t);
				m_eventsUsed++;
			}
			if (bSumEvents)
				m_imgData[ixy]++;
			else
				// place newer events on top of older ones (overwrite pixel value)
				m_imgData[ixy] = val;
			break;
		}
	}
	m_bNeedStats = true;
	return true;
}

bool EBI::EventImage::fromEventData(
	const EBI::EventData& src,
	const EBI::EventPolarity polMode,
	const uint32_t offsetUSec,
	const uint32_t durationUSec,
	const int32_t nRefTimeUSec,
	const bool bSumEvents
)
{
	clear();
	if (src.m_events.size() == 0) // no data
		return false;
	if (!alloc(src)) {
		return false;
	}
	uint32_t t1 = offsetUSec;
	uint32_t t2 = t1 + durationUSec;
	if (t2 == t1) {
		// use full duration of data set
		t2 = src.m_events[src.m_events.size() - 1].t;
	}
	m_duration = (t2 - t1);
	// fill image
	for (EBI::Event ev : src.m_events) {
		if (ev.t >= t1) {
			if (ev.t <= t2) {
				uint32_t ixy = (ev.y * m_imgWidth) + ev.x;
				// TODO: choice of how to encode intensity
				float val = m_imgData[ixy];
				switch (polMode) {
				case EBI::PolarityNegative:
					if (ev.p == 0) {
						val = static_cast<float>(ev.t);
						m_eventsUsed++;
					}
					if(bSumEvents)
						m_imgData[ixy]++;
					else
						// place newer events on top of older ones (overwrite pixel value)
						m_imgData[ixy] = val;
					break;
				case EBI::PolarityBoth:
					val = static_cast<float>(ev.t);
					m_eventsUsed++;
					if (bSumEvents)
						m_imgData[ixy]++;
					else
						// place newer events on top of older ones (overwrite pixel value)
						m_imgData[ixy] = val;
					break;
				case EBI::PolarityPositive:
				default:
					if (ev.p > 0) {
						val = static_cast<float>(ev.t);
						m_eventsUsed++;
					}
					if (bSumEvents)
						m_imgData[ixy]++;
					else
						// place newer events on top of older ones (overwrite pixel value)
						m_imgData[ixy] = val;
				}
			}
		}
	}
	if (nRefTimeUSec > 0) {
		m_refTime = nRefTimeUSec;
		// set all zero intensities to refTime
		//std::cout << "setting reference time: " << nRefTimeUSec << " usec\n";
		for (size_t i = 0; i < m_imgData.size(); i++) {
			if(m_imgData[i] < 1)
				m_imgData[i] = static_cast<float>(m_refTime);
		}
	}
	m_bNeedStats = true;
	return true;
}

void EBI::EventImage::doStats()
{
	if (!m_bNeedStats)
		return;
	double sum = 0, sum2 = 0;
	float minVal = m_imgData[0];
	float maxVal = m_imgData[0];
	for (float val : m_imgData) {
		sum += static_cast<double>(val);
		sum2 += static_cast<double>(val*val);
		if (maxVal < val)
			maxVal = val;
		if (minVal > val)
			minVal = val;
	}
	double N = static_cast<double>(m_imgData.size());
	m_statsMean = sum / N;
	m_statsVar = (sum2 * N - (sum*sum)) / (N * (N-1));
	m_statsMin = minVal;
	m_statsMax = maxVal;
	m_bNeedStats = false;
}

double EBI::EventImage::mean()
{
	doStats();
	return m_statsMean;
}

double EBI::EventImage::var()
{
	doStats();
	return m_statsVar;
}

double EBI::EventImage::minimum()
{
	doStats();
	return m_statsMin;
}

double EBI::EventImage::maximum()
{
	doStats();
	return m_statsMax;
}

/*! 
	removes isolated pixels
	\return TRUE on success
*/
bool EBI::EventImage::despeckle()
{
	if (isNull())
		return false;

	uint32_t w = m_imgWidth, 
		h = m_imgHeight;

	uint32_t c = 0, r = 0;
	for (r = 1; r < h - 1; r++) {
		uint32_t rnc = (w * r);
		for (c = 1; c < w - 1; c++) {
			uint32_t pos = rnc + c;
			if (m_imgData[pos] > 0) {	// bright pixel
				// check if all neighbors are black
				bool isone = true;
				if (m_imgData[pos - 1] > 0)
					isone = false;
				else if (m_imgData[pos + 1] > 0)
					isone = false;
				else if (m_imgData[pos - w] > 0)
					isone = false;
				else if (m_imgData[pos + w] > 0)
					isone = false;
				else if (m_imgData[pos - w - 1] > 0)
					isone = false;
				else if (m_imgData[pos - w + 1] > 0)
					isone = false;
				else if (m_imgData[pos + w - 1] > 0)
					isone = false;
				else if (m_imgData[pos + w + 1] > 0)
					isone = false;

				if (isone)
					m_imgData[pos] = 0;
			}

		}
	}
	return true;
}



/*! \cond
 * header for event data files
 */
struct _EVENT_IMAGE_HDR
{
	int32_t		Signature;		//!< "EVIM"
	uint64_t	FileSize;		//!< size in bytes including header
	uint64_t	EventCount;		//!< number of events used for image
	uint64_t	TimeStamp;		//!< time in [usec] of first event from RAW file
	uint32_t	Duration;		//!< in [usec]
	uint32_t	HeaderLength;	//!< should be 64 
	uint32_t	cols, rows;		//!< size of image
	uint32_t	_reserved1; //!< bytes 49...52
	uint32_t	_reserved2; //!< bytes 53...56
	uint32_t	_reserved3; //!< bytes 57...60
	uint32_t	_reserved4; //!< bytes 61...64
};
#define _EVENT_IMAGE_HDR_SIZE 64

/*!
access to image data; provides complete copy of vector
*/
std::vector<float> EBI::EventImage::data()
{
	return m_imgData;
}

/*!
access to reference of image data
*/
std::vector<float>& EBI::EventImage::dataRef()
{
	return m_imgData;
}

/*!
\return single pixel at linear location \a i ( = row*width + column)
*/
float EBI::EventImage::pixel(const int32_t i) const
{
	if (i < 0)
		return 0;
	else if (i >= m_imgData.size())
		return 0;
	return m_imgData[i];
}

bool EBI::EventImage::binarize(const int nMaxInt)
{
	if ((m_imgHeight < 1) || (m_imgWidth < 1)) {
		return false; // no data
	}
	for (size_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
		if (m_imgData[i] > 0) {
			m_imgData[i] = static_cast<float>(nMaxInt);
		}
	}
	return true;
}


bool EBI::EventImage::save(const std::string& fnameOut)
{
	// for now, save as binary stream
	if((m_imgHeight < 1) || (m_imgWidth < 1)) {
		return false; // no data
	}

	bool retCode = true;
	std::ofstream outFile(fnameOut, std::ios::out | std::ios::binary);
	try {
		if (!outFile.is_open()) {
			m_errMsg = "failed opening file for output";
			throw (-1);
		}
		_EVENT_IMAGE_HDR hdr;
		hdr.Signature = 0x4D495645; // "EVIM"
		//hdr.Signature = 0x32545645; // "EVT2" - older format
		hdr.FileSize = (sizeof(_EVENT_IMAGE_HDR) + (m_imgWidth * m_imgHeight * sizeof(float)));
		hdr.EventCount = m_eventsUsed;
		hdr.Duration = m_duration;
		hdr.TimeStamp = 0; // m_timeStamp;
		hdr.cols = m_imgWidth;
		hdr.rows = m_imgHeight;
		hdr.HeaderLength = sizeof(_EVENT_IMAGE_HDR);

		outFile.write(reinterpret_cast<char*>(&hdr), _EVENT_IMAGE_HDR_SIZE);

		// write out data
		outFile.write((char*)&m_imgData[0], m_imgData.size() * sizeof(float));
	}

	catch (int errCode)
	{
		std::cerr << "ERROR(" << errCode <<"): EBI::EventImage::save() " << m_errMsg << std::endl;
		retCode = false;
	}
	if (outFile.is_open())
	outFile.close();
	return retCode;
}

int32_t EBI::EventImage::removeDuplicateEvents(EventImage& prevImg)
{
	// for now, save as binary stream
	if ((m_imgHeight < 1) || (m_imgWidth < 1)) {
		return 0; // no data
	}
	if ((m_imgHeight != prevImg.m_imgHeight) && (m_imgWidth != prevImg.m_imgWidth))
	{
		return 0;	// size mismatch
	}
	int32_t cnt = 0;
	for (size_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
		if (prevImg.m_imgData[i] > 0) {
			m_imgData[i] = 0;	// clear this pixel
			cnt += 1;
		}
	}
	return cnt;
}

static void MyTIFFWarningHandler(const char* module, const char* fmt, va_list ap)
{
	// ignore errors and warnings (or handle them your own way)
}



#ifdef LIBTIFF
bool EBI::EventImage::saveMultiTIFFPage(TIFF *tif,
	EBI::ImageType imgTyp,
	const bool bScaleToMax,	//!< set to true to stored event counts rather that time surface
	const bool bCompressImage	//!< true, to enable ZIP compression
)
{
	if (!tif)
		return false; // needs to valid

	unsigned char *buf = nullptr;
	uint8_t *tmpImg = nullptr;

	TIFFSetWarningHandler(MyTIFFWarningHandler);

	try {
		int bitsPerPixel = 16;
		double maxIntens = 65535;
		double scaleIntensity = 1;
		int32_t nBytesPerLine = m_imgWidth * 2;

		if (imgTyp == EBI::ImageType_Gray8Bit) {
			nBytesPerLine = m_imgWidth;
			bitsPerPixel = 8;
			maxIntens = 255;
			scaleIntensity = 255.0 / m_duration;
		}
		// todo: option for 32-bit image
		if (bScaleToMax) {
			// find max intensity
			maxIntens = maximum();
			//for (uint32_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
			//	double val = m_imgData[i];
			//	if (maxIntens < val)
			//		maxIntens = val;
			//}
			if (maxIntens > 65535) {
				scaleIntensity = 65535.0 / maxIntens;
			}
			else if (imgTyp == EBI::ImageType_Gray8Bit)
				scaleIntensity = 255.0 / maxIntens;
		}
		else if (m_duration > 0xFFFF) {
			std::cerr << "duration too long for 16-bit image - output data will be scaled to range" << std::endl;
			scaleIntensity = 65535.0 / m_duration;
		}

		TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, m_imgWidth);
		TIFFSetField(tif, TIFFTAG_IMAGELENGTH, m_imgHeight);
		TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitsPerPixel);
		switch (imgTyp) {
		case ImageType_Gray32Bit:
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			break;
		case ImageType_GrayFloat:
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
			break;
		default:
			break;
			//case DataTypeVoid:
			//	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_VOID );
			//	break;
		}
		TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);	// 8-bit or 16-bit data!
		TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

		// compression mode
		if (bCompressImage) {
			// use ZIP compression
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
			// alternative: PackBits
			// TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_PACKBITS);
			// alternative: LZW
			//TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
		}

		size_t rowsperstrip = (size_t)-1;
		size_t linebytes = m_imgWidth * bitsPerPixel / 8;
		if (TIFFScanlineSize(tif) > (int)linebytes)
			buf = (unsigned char *)_TIFFmalloc((tsize_t)linebytes);
		else
			buf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(tif));
		if (!buf)
			throw EBI::MallocFailed;
		TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,
			TIFFDefaultStripSize(tif, (uint32_t)rowsperstrip));

		tmpImg = new uint8_t[m_imgHeight * nBytesPerLine];
		if (!tmpImg)
			throw EBI::MallocFailed;
		if (imgTyp == EBI::ImageType_Gray8Bit) {
			// convert float image to 8 bit
			for (size_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
				double val = m_imgData[i] * scaleIntensity;
				uint8_t ival = static_cast<uint8_t>((val < 0) ? 0 : ((val > 255) ? 255 : val));
				tmpImg[i] = ival;
			}
		}
		else {
			// convert float image to 16 bit
			uint16_t *destImg = (uint16_t *)tmpImg;	// turn into 16-bit data
			for (size_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
				double val = m_imgData[i] * scaleIntensity;
				uint16_t ival = static_cast<uint16_t>((val < 0) ? 0 : ((val > 65535) ? 65535 : val));
				destImg[i] = ival;
			}
		}

		uint8_t* srcPtr = (uint8_t*)tmpImg;
		for (uint32_t r = 0; r < m_imgHeight; r++, srcPtr += nBytesPerLine) {
			memcpy(buf, srcPtr, linebytes);
			if (TIFFWriteScanline(tif, buf, r, 0) < 0)
				break;
		}
		if (buf)
			_TIFFfree(buf);
		buf = 0;

		if (tmpImg)
			delete tmpImg;
	}
	catch (EBI::ErrorCode err) {
		//m_Err.handleError(err, "saveTIFF()");
		if (tif)
			TIFFClose(tif);
		if (buf)
			_TIFFfree(buf);
		if (tmpImg)
			delete tmpImg;
		return false;
	}
	return true;
}
#endif

bool EBI::EventImage::saveTIFF(const std::string& strFileName,
	EBI::ImageType imgTyp,
	const bool bScaleToMax,	//!< set to true to stored event counts rather that time surface
	const bool bCompressImage	//!< true, to enable ZIP compression
)
{
#ifdef LIBTIFF
	TIFF *tif = 0;
	unsigned char *buf = nullptr;
	uint8_t *tmpImg = nullptr;
	// disable warnings
	TIFFSetWarningHandler(MyTIFFWarningHandler);

	int bitsPerPixel = 16;
	double maxIntens = 65535;
	double scaleIntensity = 1;
	int32_t nBytesPerLine = m_imgWidth * 2;

	if (imgTyp == EBI::ImageType_Gray8Bit) {
		nBytesPerLine = m_imgWidth;
		bitsPerPixel = 8;
		maxIntens = 255;
		scaleIntensity = 255.0 / m_duration;
	}
	// todo: option for 32-bit image
	if (bScaleToMax) {
		// find max intensity
		maxIntens = maximum();
		//for (uint32_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
		//	double val = m_imgData[i];
		//	if (maxIntens < val)
		//		maxIntens = val;
		//}
		if (maxIntens > 65535) {
			scaleIntensity = 65535.0 / maxIntens;
		}
		else if(imgTyp == EBI::ImageType_Gray8Bit) 
			scaleIntensity = 255.0 / maxIntens;
	}
	else if (m_duration > 0xFFFF) {
		std::cerr << "duration too long for 16-bit image - output data will be scaled to range" << std::endl;
		scaleIntensity = 65535.0 / m_duration;
	}

	try {
		tif = TIFFOpen(strFileName.c_str(), "w");
		if (!tif) {
			//m_Err.setMessage("Trouble creating TIFF-image: %s", strFileName.c_str());
			throw EBI::FileCreateFailed;
		}
		TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, m_imgWidth);
		TIFFSetField(tif, TIFFTAG_IMAGELENGTH, m_imgHeight);
		TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitsPerPixel);
		switch (imgTyp) {
		case ImageType_Gray32Bit:
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			break;
		case ImageType_GrayFloat:
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
			break;
		default:
			break;
			//case DataTypeVoid:
			//	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_VOID );
			//	break;
		}
		TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);	// 8-bit or 16-bit data!
		TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

		// compression mode
		if (bCompressImage) {
			// use ZIP compression
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
			// alternative: PackBits
			// TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_PACKBITS);
			// alternative: LZW
			//TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
		}

		size_t rowsperstrip = (size_t)-1;
		size_t linebytes = m_imgWidth * bitsPerPixel / 8;
		if (TIFFScanlineSize(tif) > (int)linebytes)
			buf = (unsigned char *)_TIFFmalloc((tsize_t)linebytes);
		else
			buf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(tif));
		if (!buf)
			throw EBI::MallocFailed;
		TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,
			TIFFDefaultStripSize(tif, (uint32_t)rowsperstrip));

		tmpImg = new uint8_t[m_imgHeight * nBytesPerLine];
		if (imgTyp == EBI::ImageType_Gray8Bit) {
			// convert float image to 8 bit
			for (size_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
				double val = m_imgData[i] * scaleIntensity;
				uint8_t ival = static_cast<uint8_t>((val < 0) ? 0 : ((val > 255) ? 255 : val));
				tmpImg[i] = ival;
			}
		}
		else {
			// convert float image to 16 bit
			uint16_t *destImg = (uint16_t *) tmpImg;	// turn into 16-bit data
			for (size_t i = 0; i < (m_imgWidth * m_imgHeight); i++) {
				double val = m_imgData[i] * scaleIntensity;
				uint16_t ival = static_cast<uint16_t>((val < 0) ? 0 : ((val > 65535) ? 65535 : val));
				destImg[i] = ival;
			}
		}

		uint8_t* srcPtr = (uint8_t*)tmpImg;
		for (uint32_t r = 0; r < m_imgHeight; r++, srcPtr += nBytesPerLine) {
			memcpy(buf, srcPtr, linebytes);
			if (TIFFWriteScanline(tif, buf, r, 0) < 0)
				break;
		}
		if (buf)
			_TIFFfree(buf);
		buf = 0;
		TIFFClose(tif);
		if (tmpImg)
			delete tmpImg;
	}
	catch (EBI::ErrorCode err) {
		//m_Err.handleError(err, "saveTIFF()");
		if (tif)
			TIFFClose(tif);
		if (buf)
			_TIFFfree(buf);
		if (tmpImg)
			delete tmpImg;
		return false;
	}
	return true;
#else // LIBTIFF
if (m_nDebugLevel > 0)
	std::cerr << "EBI::EventImage::saveTIFF() is not implemented"
		<< " - recompile with LIBTIFF defined" << std::endl;
	return false;
#endif
}





