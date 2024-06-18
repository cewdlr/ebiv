#ifndef _EBI_EVENTDATA_H__INCLUDED_
#define _EBI_EVENTDATA_H__INCLUDED_

#include <cstdint>
#include <vector>
#include <string>
#include "ebi_structs.h"

namespace EBI {

	class EventData
	{
	public:
		EventData();
		EventData(const std::string& fnameRawEvents);
		EventData(const EBI::EventData& src,
			const EBI::EventPolarity polMode,
			const int32_t offsetUSec = 0, const int32_t durationUSec = 0,
			bool bSubtractOffsetTime = true);
		EventData(const EBI::EventData& src,
			const EBI::EventPolarity polMode,
			const int32_t x, const int32_t y,
			const int32_t w, const int32_t h,
			const int32_t t0 = 0, const int32_t dur = 0);
		EventData(const EBI::EventData& src,
			const int32_t x, const int32_t y,
			const int32_t w, const int32_t h,
			const int32_t t0 = 0, const int32_t dur = 0);
		~EventData();
		friend class EventImage;

		bool copyFrom(const EBI::EventData& src);

		bool copyFrom(const EBI::EventData& src,
			const int32_t offsetUSec, const int32_t durationUSec);

		bool copyFrom(const EBI::EventData& src,
			const EBI::EventPolarity polMode,
			const int32_t offsetUSec = 0, const int32_t durationUSec = 0,
			bool bSubtractOffsetTime = true);

		bool copyFrom(const EBI::EventData& src,
			const EBI::EventPolarity polMode, 
			const int32_t x, const int32_t y,
			const int32_t w, const int32_t h,
			const int32_t t0 = 0, const int32_t dur = 0);

		bool copyFrom(const EBI::EventData& src,
			const int32_t x, const int32_t y,
			const int32_t w, const int32_t h,
			const int32_t t0 = 0, const int32_t dur = 0);

		bool isNull();
		void clear();
		std::vector<EBI::Event> data();
		std::vector<EBI::Event>& dataRef();	// access to reference of image data
		std::vector<EBI::Event> getSample(
			const int32_t x, const int32_t y,
			const int32_t w, const int32_t h,
			const int32_t t0 = 0, const int32_t dur = 0);

		bool cropROI(const int32_t x, const int32_t y,
			const int32_t w, const int32_t h,
			const int32_t t0 = 0, const int32_t dur = 0);

		bool save(const std::string& fnameEvents,
			const uint32_t offsetUSec = 0, const uint32_t durationUSec = 0);
		bool load(const std::string& fnameEvents,
			const uint32_t offsetUSec = 0, const uint32_t durationUSec = 0);

		std::vector<EBI::TriggerEvent> triggerEvents();
		std::vector<EBI::TriggerEvent>& triggerRef();

		int32_t imageWidth() const { return static_cast<int32_t>(m_camSpecs.sensorW); }
		int32_t imageHeight() const { return static_cast<int32_t>(m_camSpecs.sensorH); }
		//int32_t timeStamp() const { return static_cast<int32_t>(m_timeStamp); }
		uint64_t timeStamp() const { return m_timeStamp; }

		void setDebugLevel(const int32_t nLevel);
		void setMaximumSize(const uint64_t nMaxSize);

	protected:
		std::vector<EBI::Event> m_events;
		std::vector<EBI::TriggerEvent> m_triggerEvents;
		EBI::EventCameraSpecs m_camSpecs;
		std::string m_errMsg;
		int32_t m_nDebugLevel;

		uint64_t m_timeStamp;
		uint64_t m_maxEvents;
		bool loadRawData(const std::string& fnameRawEvents,
			const uint32_t offsetUSec = 0, const uint32_t durationUSec = 0);
	private:
		void init();
		// specifics for camera
	};
} // namespace EBI

#endif /* _EBI_EVENTDATA_H__INCLUDED_ */
