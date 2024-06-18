#ifndef _EBI_IMAGE_H__INCLUDED_
#define _EBI_IMAGE_H__INCLUDED_

#include <cstdint>
#include <vector>
#include <string>

#ifdef LIBTIFF
# include "tiffio.h"
#endif

#include "ebi_structs.h"
#include "ebi_data.h"

namespace EBI {

	class EventImage
	{
	public:
		EventImage();
		EventImage(const EBI::EventData& src, const EBI::EventPolarity polMode,
			const uint32_t offsetUSec = 0, const uint32_t durationUSec = 0,
			const int32_t refTimeUSec = 0, const bool bSumEvents = false);

		void clear();
		bool fromEventData(const EBI::EventData& src,
			const EBI::EventPolarity polMode,
			const uint32_t offsetUSec = 0,
			const uint32_t durationUSec = 0,
			const int32_t refTimeUSec = 0,
			const bool bSumEvents = false);
		bool addFromEventData(const EBI::EventData& data, const EBI::EventPolarity polMode, const bool bSumEvents = false);
		void setReferenceTime(const int32_t refTimeUSec);
		int32_t referenceTime() const;

		bool save(const std::string& fnameOut);
		bool saveTIFF(const std::string& fnameOut, ImageType imgTyp = ImageType_Gray16Bit,
			const bool bScaleToMax = false, const bool bCompressImage = true);
#ifdef LIBTIFF
		bool saveMultiTIFFPage(TIFF *tiffFilePtr, ImageType imgTyp = ImageType_Gray16Bit,
			const bool bScaleToMax = false, const bool bCompressImage = true);
#endif
		int32_t removeDuplicateEvents(EventImage& prevImg);

		std::vector<float> data();	// access to image data
		std::vector<float>& dataRef();	// access to reference of image data
		float pixel(const int i) const;
		int32_t width() const { return static_cast<int32_t>(m_imgWidth); }
		int32_t height() const { return static_cast<int32_t>(m_imgHeight); }
		double minimum();
		double maximum();
		double mean();
		double var();
		bool isNull() const;

		bool despeckle(); // remove isolated pixels
		bool binarize(const int nMaxInt = 255); // set all events to specified intensity

		void setDebugLevel(const int32_t nLevel);

	protected:
		uint32_t m_imgWidth, m_imgHeight;
		uint32_t m_duration;
		uint64_t m_eventsUsed;
		int32_t m_refTime;

		std::string m_errMsg;
		int32_t m_nDebugLevel;

		std::vector<float> m_imgData;
		void doStats();
		bool m_bNeedStats;
		double m_statsMean, m_statsVar, m_statsMin, m_statsMax;
	private:
		void init();
		bool alloc(const EBI::EventData& src);
	};
}

#endif /* _EBI_IMAGE_H__INCLUDED_  */