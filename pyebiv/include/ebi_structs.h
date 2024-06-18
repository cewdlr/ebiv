#ifndef _EBI_STRUCTS_H__INCLUDED_
#define _EBI_STRUCTS_H__INCLUDED_

#include <cstdint>

namespace EBI {

	enum EventPolarity
	{
		PolarityNegative = 0,
		PolarityPositive = 1,
		PolarityBoth = 2
	};
	enum ProcessingMode
	{
		MotionCompensation = 0,
		CorrelationSum = 1,
	};
	enum FileFormat
	{
		FILE_FORMAT_UNKNOWN = -1,
		FILE_FORMAT_EVT3 = 0,		// my own simple event format
		FILE_FORMAT_RAWEVT3 = 1,	// Metavision's raw format
		FILE_FORMAT_TIFF = 2,
		FILE_FORMAT_ASCII = 3,
		FILE_FORMAT_NETCDF = 4,
	};

	enum ErrorCode
	{
		NoError = 0,
		MallocFailed = 1,
		FFTMallocFailed = 2,
		BadPointer = 3,
		FileOpenFailed = 11,
		FileCreateFailed = 12,
		FileWriteFailed = 13,
		CorrelationFailed = 20,
		OperationFailed = 99,
	};

	enum ImageType
	{
		ImageType_Unknown = 0,
		ImageType_Gray8Bit = 8,
		ImageType_Gray12Bit = 8,
		ImageType_Gray16Bit = 16,
		ImageType_Gray32Bit = 32,
		ImageType_GrayFloat = 33,
	};

	/*!
	Single event
	*/
	struct Event {
		uint32_t t;	//!< time in [usec]
		uint16_t x;	//!< pixel coordinate X
		uint16_t y;	//!< pixel coordinate Y
		int8_t	p;	//!< polarity [+,-]

		void init()
		{
			t = 0;
			x = y = 0;
			p = 0;
		}
		Event() { init(); }

		Event(uint16_t px, uint16_t py, int8_t pol, uint32_t tim)
		{
			t = tim;
			x = px;
			y = py;
			p = pol;
		}
	};

	/*!
	Single trigger event
	*/
	struct TriggerEvent {
		uint32_t t;	//!< time in [usec]
		int8_t	v;	//!< value
		int8_t	id;	//!< trigger type

		void init()
		{
			t = 0;
			v = 0;
			id = 0;
		}
		TriggerEvent() { init(); }

		TriggerEvent(uint16_t _value, uint16_t _id, uint32_t _tim)
		{
			t = _tim;
			id = static_cast<int8_t>(_id);
			v = static_cast<int8_t>(_value);
		}
	};

	struct EventCameraSpecs {
		uint32_t sensorW;	//!< detector width in [pixels]
		uint32_t sensorH;	//!< detector height in [pixels]
		std::string strIntegrator;
		std::string strPlugin;
		std::string strFirmware;
		std::string strEventType;
		std::string strSerialNo;
		std::string strSensorGeneration;
		std::string strRecordingDate;
		std::string strRecordingTime;

		void init()
		{
			sensorW = 0;
			sensorH = 0;
			strIntegrator = "";
			strPlugin = "";
			strFirmware = "";
			strEventType = "3.0";
			strSerialNo = "";
			strRecordingDate = "";
			strRecordingTime = "";
			strSensorGeneration = "";
		}
		EventCameraSpecs() { init(); }
	};

	struct EventFlowEvalParams
	{
		ProcessingMode procMode;	//!< method to use to retrieve optical flow
		int32_t imgH,		//!< height of image
			imgW;			//!< width of image
		int32_t sampleY,	//!< width of sample [pixel]
			sampleX,		//!< height of sample [pixel]
			sampleTime;		//!< duration of sampling time [usec]
		int32_t stepX,		//!< horizontal increment of sampling [pixel]
			stepY,			//!< vertical increment of sampling [pixel]
			stepTime;		//!< increment of time sample [usec]
		double vxMin,		//!< X-velocity scan range minimum in [pixel/ms]
			vxMax,			//!< X-velocity scan range maximum
			vxResol;		//!< X-velocity scan range resolution
		double vyMin,		//!< Y-velocity scan range minimum in [pixel/ms]
			vyMax,			//!< Y-velocity scan range maximum
			vyResol;		//!< Y-velocity scan range resolution
		EventPolarity evPol; //!< polarity to use
		int32_t offsetTime;	//!< time offset for sum-of-correlation approach [usec]
		int32_t nResampleTimeSteps;	//!< number of time-slices in sub-volume for sum-of-correlation approach
		int32_t nInterpolation;	//!< interpolation method for motion-compensation scheme

		double mag;		//!< image magnification in [pixel/mm]

		//!< initialize all parameters with default values
		void init() {
			procMode = CorrelationSum;
			imgW = imgH = 0;
			sampleX = sampleY = 40;
			stepX = stepY = 20;
			sampleTime = 20000;
			stepTime = 10000;
			evPol = PolarityPositive;
			offsetTime = 0;
			nResampleTimeSteps = 40;
			nInterpolation = 0;

			vxMin = vyMin = -2;
			vxMax = vyMax = 2;
			vxResol = vyResol = 0.2;
			mag = 1;
		}
		EventFlowEvalParams() { init(); }
	};

	struct PixelVelocity {
		double vx,	//!< horizontal velocity estimate [pixel/ms]
			vy,		//!< vertical velocity estimate [pixel/ms]
			ix,		//!< sample location, X-coordinate [pixel]
			iy,		//!< sample location, Y-coordinate [pixel]
			t;		//!< sample time [ms]
		double maxVar;
		double eventCount; //!< normalized number of events in sample

		void init()
		{
			vx = vy = ix = iy = 0;
			maxVar = 0;
			eventCount = 0;
			t = 0;
		}
		PixelVelocity() { init(); }
	};

}
#endif /* _EBI_STRUCTS_H__INCLUDED_ */
