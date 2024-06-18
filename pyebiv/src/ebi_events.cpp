#include "ebi.h"
#include <iostream>
#include <fstream>
#include <sstream>
//#define _DEBUG2

static std::vector<std::string> _stringSplit(const std::string str, char delim)
{
	std::vector<std::string> result;
	std::istringstream ss{ str };
	std::string token;
	while (std::getline(ss, token, delim)) {
		if (!token.empty()) {
			result.push_back(token);
		}
	}
	return result;
}

// The following code is derived from Prophesee's Metavision SDK
// see: ./ Prophesee / share / metavision / standalone_samples / metavision_evt3_raw_file_decoder/
namespace Metavision {
	namespace Evt3 {

		// Event Data 3.0 is a 16 bits vectorized data format. It has been designed to comply with both data compactness
		// necessity and vector event data support.
		// This EVT3.0 event format avoids transmitting redundant event data for the time, y and x values.
		enum class EventTypes : uint8_t {
			EVT_ADDR_Y = 0x0,  // Identifies a CD event and its y coordinate
			EVT_ADDR_X = 0x2,  // Marks a valid single event and identifies its polarity and X coordinate. The event's type and
							   // timestamp are considered to be the last ones sent
			VECT_BASE_X = 0x3, // Transmits the base address for a subsequent vector event and identifies its polarity and base
								// X coordinate. This event does not represent a CD sensor event in itself and should not be
								// processed as such,  it only sets the base x value for following VECT_12 and VECT_8 events.
			VECT_12 = 0x4, // Vector event with 12 valid bits. This event encodes the validity bits for events of the same type,
							// timestamp and y coordinate as previously sent events, while consecutive in x coordinate with
							// respect to the last sent VECT_BASE_X event. After processing this event, the X position value
							// on the receiver side should be incremented by 12 with respect to the X position when the event was
							// received, so that the VECT_BASE_X is updated like follows: VECT_BASE_X.x = VECT_BASE_X.x + 12
			VECT_8 = 0x5,  // Vector event with 8 valid bits. This event encodes the validity bits for events of the same type,
						// timestamp and y coordinate as previously sent events, while consecutive in x coordinate with
						// respect to the last sent VECT_BASE_X event. After processing this event, the X position value
						// on the receiver side should be incremented by 8 with respect to the X position when the event was
						// received, so that the VECT_BASE_X is updated like follows: VECT_BASE_X.x = VECT_BASE_X.x + 8
			EVT_TIME_LOW = 0x6, // Encodes the lower 12b of the timebase range (range 11 to 0). Note that the TIME_LOW value is
								// only monotonic for a same event source, but can be non-monotonic when multiple event sources
								// are considered. They should however refer to the same TIME_HIGH value. As the time low has
								// 12b with a 1us resolution, it can encode time values from 0us to 4095us (4095 = 2^12 - 1).
								// After 4095us, the time_low value wraps and returns to 0us, at which point the TIME_HIGH value
								// should be incremented.
			EVT_TIME_HIGH = 0x8, // Encodes the higher portion of the timebase (range 23 to 12). Since it encodes the 12 higher
									// bits over the 24 used to encode a timestamp, it has a resolution of 4096us (= 2^(24-12)) and
									// it can encode time values from 0us to 16777215us (= 16.777215s). After 16773120us the
									// time_high value wraps and returns to 0us.																													 EXT_TRIGGER = 0xA    // External trigger output
			EXT_TRIGGER = 0xA    // External trigger output
		};
		// Evt3 raw events are 16-bit words
		struct RawEvent {
			uint16_t pad : 12; // Padding
			uint16_t type : 4; // Event type
		};

		struct RawEventTime {
			uint16_t time : 12;
			uint16_t type : 4; // Event type : EventTypes::EVT_TIME_LOW OR EventTypes::EVT_TIME_HIGH
		};

		struct RawEventXPos {
			uint16_t x : 11;   // Pixel X coordinate
			uint16_t pol : 1;  // Event polarity:
							   // '0': decrease in illumination
							   // '1': increase in illumination
			uint16_t type : 4; // Event type : EventTypes::X_POS
		};

		struct RawEventVect12 {
			uint16_t valid : 12; // Encodes the validity of the events in the vector :
								 // foreach i in 0 to 11
								 //   if valid[i] is '1'
								 //      valid event at X = X_BASE.x + i
			uint16_t type : 4;   // Event type : EventTypes::VECT_12
		};

		struct RawEventVect8 {
			uint16_t valid : 8; // Encodes the validity of the events in the vector :
								// foreach i in  0 to 7
								//   if valid[i] is '1'
								//      valid event at X = X_BASE.x + i
			uint16_t unused : 4;
			uint16_t type : 4; // Event type : EventTypes::VECT_8
		};

		struct RawEventY {
			uint16_t y : 11;   // Pixel Y coordinate
			uint16_t orig : 1; // Identifies the System Type:
							   // '0': Master Camera (Left Camera in Stereo Systems)
							   // '1': Slave Camera (Right Camera in Stereo Systems)
			uint16_t type : 4; // Event type : EventTypes::CD_Y OR EventTypes::EM_Y
		};

		struct RawEventXBase {
			uint16_t x : 11;   // Pixel X coordinate
			uint16_t pol : 1;  // Event polarity:
							   // '0': decrease in illumination
							   // '1': increase in illumination
			uint16_t type : 4; // Event type : EventTypes::X_BASE
		};

		struct RawEventExtTrigger {
			uint16_t value : 1; // Trigger current value (edge polarity):
								// - '0' (falling edge);
								// - '1' (rising edge).
			uint16_t unused : 7;
			uint16_t id : 4;   // Trigger channel ID.
			uint16_t type : 4; // Event type : EventTypes::EXT_TRIGGER
		};

		using timestamp_t = uint64_t; // Type for timestamp, in microseconds

	} // namespace Evt3
} // namespace Metavision


static bool _loadRawEventData(const std::string& fname, 
	std::vector<EBI::Event>& evData,
	std::vector<EBI::TriggerEvent>& evTrigger,
	uint64_t& timeStamp,	// from first event in file
	EBI::EventCameraSpecs& camSpecs,
	const uint64_t nStartTime,	//!< offset within file in microseconds (input)
	const uint64_t nMaxEventCount,	//!< maximum number of events to load
	const bool bDebugMessages	//!< true to enable diagnostic output
	)
{
	bool retCode = true; // on success
	uint32_t trigCount = 0;
	uint32_t nEventOutOfBounds = 0;
	Metavision::Evt3::timestamp_t firstTimeStamp = 0;

	// open file
	std::ifstream input_file(fname, std::ios::in | std::ios::binary);
	try {
		if (!input_file.is_open()) {
			std::cerr << "Error : could not open file '" << fname.c_str() << "' for reading" << std::endl;
			throw (-1);
		}
		// header reading part is modified to keep compatability with older raw data 
		// (i.e. missing %end statement, missing sensor size info,...)

		// header looks like this:
		//	% camera_integrator_name CenturyArks
		//	% date 2023 - 06 - 23 19:45 : 32
		//	% evt 3.0
		//	% format EVT3; height = 480; width = 640
		//	% generation 3.1
		//	% geometry 640x480
		//	% integrator_name CenturyArks
		//	% plugin_integrator_name CenturyArks
		//	% plugin_name evc3a_plugin_gen31
		//	% sensor_generation 3.1
		//	% serial_number 00000097
		//	% system_ID 40
		//	% end
		//-------------or------------
		//	% date 2022 - 08 - 11 16:33 : 16
		//	% evt 3.0
		//	% firmware_version 4.3.0
		//	% format EVT3
		//	% geometry 1280x720
		//	% integrator_name Prophesee
		//	% plugin_name hal_plugin_gen41_evk2
		//	% sensor_generation 4.1
		//	% serial_number 0000a4e9
		//	% system_ID 39
		//----------- very early version ----
		//  % date 2022 - 01 - 23 17:39 : 01
		//	% evt 3.0
		//	% firmware_version 4.3.0
		//	% integrator_name Prophesee
		//	% plugin_name hal_plugin_gen41_evk2
		//	% serial_number 0000a4e9
		//	% subsystem_ID 0
		//	% system_ID 39
		//-------------or-------------
		//  % camera_integrator_name CenturyArks
		//  % date 2024 - 01 - 26 15:39 : 08
		//  % evt 3.0
		//  % format EVT3; height = 720; width = 1280
		//  % generation 4.2
		//  % geometry 1280x720
		//  % integrator_name CenturyArks
		//  % plugin_integrator_name CenturyArks
		//  % plugin_name evc4a_plugin_imx636
		//  % sensor_generation 4.2
		//  % serial_number 00000204
		//  % system_ID 49
		//  % end
		//-------------or------------
		//  % camera_integrator_name CenturyArks
		//  % date 2024 - 02 - 01 14:54 : 01
		//  % evt 3.0
		//  % format EVT3; height = 720; width = 1280
		//  % generation 4.2
		//  % geometry 1280x720
		//  % integrator_name CenturyArks
		//  % plugin_integrator_name CenturyArks
		//  % plugin_name silky_common_plugin
		//  % sensor_generation 4.2
		//  % sensor_name IMX636
		//  % serial_number 00000204
		//  % system_ID 49
		//  % end

		// Read the header of the input file, if present :
		int line_first_char = input_file.peek();

		if(bDebugMessages)
			std::cout << "File: " << fname.c_str() << std::endl;
		bool bIsEventFileType3 = false;
		bool bhaveGeometry = false;
		while (line_first_char == '%') {
			std::string line;
			std::getline(input_file, line);
			//std::cout << line << std::endl;
			if (line == "% end") {	// added in v4.0.0
				break;
			}

			std::vector<std::string> vals = _stringSplit(line, ' ');
#ifdef _DEBUG2
			for (std::string v : vals) {
				std::cout << " '" << v << "' ";
			}
			std::cout << std::endl;
#endif

			if (vals.size() == 3) {
				if (vals[1] == "integrator_name")
					camSpecs.strIntegrator = vals[2];
				else if (vals[1] == "plugin_name")
					camSpecs.strPlugin = vals[2];
				else if (vals[1] == "firmware_version")
					camSpecs.strFirmware = vals[2];
				else if (vals[1] == "evt")
					camSpecs.strEventType = vals[2];
				else if (vals[1] == "serial_number")
					camSpecs.strSerialNo = vals[2];
				else if (vals[1] == "sensor_generation")
					camSpecs.strSensorGeneration = vals[2];
				else if (vals[1] == "generation") {
					camSpecs.strSensorGeneration = vals[2];
					// some implementations don't produce long headers so we assume EVT3.0 format 
					// for later generation sensors
					if (camSpecs.strSensorGeneration == "4.2") {
						bIsEventFileType3 = true;
					}
				}

				else if (vals[1] == "date") {
					camSpecs.strRecordingDate = vals[2];
					camSpecs.strRecordingTime = vals[3];
				}
				else if (vals[1] == "geometry") {
					if (vals[2] == "640x480") {
						camSpecs.sensorH = 480;
						camSpecs.sensorW = 640;
						bhaveGeometry = true;
					}
					else if (vals[2] == "1280x720") {
						camSpecs.sensorH = 720;
						camSpecs.sensorW = 1280;
						bhaveGeometry = true;
					}
				}
			}
			//std::cout << std::endl;

			if (line == "% evt 3.0")
				bIsEventFileType3 = true;
			//else if(line.find("integrator_name"))
			line_first_char = input_file.peek();
		};
		if (!bIsEventFileType3) {
			std::cerr << "Error : not an event data file: '" << fname.c_str() << "'" 
				<< std::endl;
			throw (-2);
		}

		if (camSpecs.strIntegrator == "Prophesee") {
			if (camSpecs.strPlugin == "hal_plugin_gen41_evk2") {
				camSpecs.sensorH = 720;
				camSpecs.sensorW = 1280;
				bhaveGeometry = true;
			}
			else if (camSpecs.strPlugin == "hal_plugin_imx636_evk4") {
				camSpecs.sensorH = 720;
				camSpecs.sensorW = 1280;
				bhaveGeometry = true;
			}
			else if (camSpecs.strPlugin == "evc3a_plugin_gen31") {
				camSpecs.sensorH = 480;
				camSpecs.sensorW = 640; 
				bhaveGeometry = true;
			}
		}
		else if (camSpecs.strIntegrator == "CenturyArks") {
			
			if (camSpecs.strPlugin == "evc4a_plugin_imx636") {
				camSpecs.sensorH = 720;
				camSpecs.sensorW = 1280;
				bhaveGeometry = true;
			}
			else if (!bhaveGeometry) {
				camSpecs.sensorH = 720;
				camSpecs.sensorW = 1280;
				//std::cerr << "Error : no geometry info in header - trying with default 1280x720" << std::endl;
			}
		}
		if (!bhaveGeometry) {
			camSpecs.sensorH = 720;
			camSpecs.sensorW = 1280;
			std::cerr << "Error : no geometry info in header - trying with default 1280x720" << std::endl;
		}
		if(bDebugMessages)
			std::cout << "Sensor: " << camSpecs.strIntegrator << " - " << camSpecs.strPlugin 
				<< "\nSize: " << camSpecs.sensorW << "(W) x " << camSpecs.sensorH << "(H)"
				<< std::endl;

		// Vector where we'll read the raw data
		static constexpr uint32_t WORDS_TO_READ = 1000000; // Number of words to read at a time
		std::vector<Metavision::Evt3::RawEvent> buffer_read(WORDS_TO_READ);

		// State variables needed for decoding
		enum class EvType { CD, EM }; // To keep track of the current event type, event if we just decode CD in this example
		EvType current_type = EvType::CD;
		bool first_time_base_set = false;
		Metavision::Evt3::timestamp_t current_time_base = 0; // time high bits
		Metavision::Evt3::timestamp_t current_time_low = 0;
		Metavision::Evt3::timestamp_t current_time = 0;
		uint16_t current_cd_y = 0;
		uint16_t current_x_base = 0;
		uint16_t current_polarity = 0;
		unsigned int n_time_high_loop = 0; // Counter of the time high loops
		bool keep_triggers = true;

		uint64_t evCount = 0;
		timeStamp = 0UL;
		std::string cd_str = "", trigg_str = "";

		while (input_file) {
			input_file.read(reinterpret_cast<char *>(buffer_read.data()),
				WORDS_TO_READ * sizeof(Metavision::Evt3::RawEvent));
			Metavision::Evt3::RawEvent *current_word = buffer_read.data();
			Metavision::Evt3::RawEvent *last_word = current_word + input_file.gcount() / sizeof(Metavision::Evt3::RawEvent);

			// If the first event in the input file is not of type EVT_TIME_HIGH, then the times
			// of the first events might be wrong, because we don't have a time base yet. This is why
			// we skip the events until we find the first time high, so that we can correctly set
			// the current_time_base
			for (; !first_time_base_set && current_word != last_word; ++current_word) {
				Metavision::Evt3::EventTypes type = static_cast<Metavision::Evt3::EventTypes>(current_word->type);
				if (type == Metavision::Evt3::EventTypes::EVT_TIME_HIGH) {
					Metavision::Evt3::RawEventTime *ev_timehigh =
						reinterpret_cast<Metavision::Evt3::RawEventTime *>(current_word);
					current_time_base = (Metavision::Evt3::timestamp_t(ev_timehigh->time) << 12);					
					//std::cout << "timestamp: " << current_time_base << std::endl;
					if(!first_time_base_set)
						firstTimeStamp = current_time_base;
					first_time_base_set = true;
					break;
				}
			}
			for (; current_word != last_word; ++current_word) {
				Metavision::Evt3::EventTypes type = static_cast<Metavision::Evt3::EventTypes>(current_word->type);
				switch (type) {
				case Metavision::Evt3::EventTypes::EVT_ADDR_X: {
					Metavision::Evt3::RawEventXPos *ev_cd_posx =
						reinterpret_cast<Metavision::Evt3::RawEventXPos *>(current_word);
					// disabled in v2.3.1:
					// current_x_base = ev_cd_posx->x; // X_POS also updates the X_BASE
					if (current_type == EvType::CD) {
						if (evCount == 0)
							timeStamp = current_time;
						// We have a new Event CD with
						// x = ev_cd_posx->x //removed: ( = current_x_base as we just updated this variable in previous statement)
						// y = current_cd_y
						// polarity = ev_cd_posx->pol
						// time = current_time (in us)
						//cd_str += std::to_string(current_x_base) + "," + std::to_string(current_cd_y) + "," +
						//	std::to_string(ev_cd_posx->pol) + "," + std::to_string(current_time) + "\n";
						if (current_time - timeStamp > nStartTime) {
							evData.push_back(EBI::Event(
								(ev_cd_posx->x % camSpecs.sensorW), // range check added 20240328, 
								// in v2.3.0: current_x_base,
								current_cd_y,
								ev_cd_posx->pol,
								static_cast<uint32_t>(current_time - timeStamp)));
#ifdef _DEBUG2
							if (ev_cd_posx->x >= camSpecs.sensorW) {
								nEventOutOfBounds++;
							}
#endif
						}
						evCount++;
					}
					break;
				}
				case Metavision::Evt3::EventTypes::VECT_12: {
					uint16_t end = current_x_base + 12;

					if (current_type == EvType::CD) {
						Metavision::Evt3::RawEventVect12 *ev_vec_12 =
							reinterpret_cast<Metavision::Evt3::RawEventVect12 *>(current_word);
						uint32_t valid = ev_vec_12->valid;
						for (uint16_t i = current_x_base; i != end; ++i) {
							if (valid & 0x1) {
								// We have a new Event CD with
								// x = i
								// y = current_cd_y
								// polarity = current_polarity
								// time = current_time (in us)
								//cd_str += std::to_string(i) + "," + std::to_string(current_cd_y) + "," +
								//	std::to_string(current_polarity) + "," + std::to_string(current_time) + "\n";
								if (evCount == 0)
									timeStamp = current_time;
								if (current_time - timeStamp > nStartTime) {
									evData.push_back(EBI::Event(
										(i % camSpecs.sensorW), // range check added 20240328
										current_cd_y,
										static_cast<int8_t>(current_polarity),
										static_cast<uint32_t>(current_time - timeStamp)
									));
#ifdef _DEBUG2
									if (i >= camSpecs.sensorW) {
										nEventOutOfBounds++;
									}
#endif
								}
								evCount++;
							}
							valid >>= 1;
						}
					}
					current_x_base = end;
					break;
				}
				case Metavision::Evt3::EventTypes::VECT_8: {
					uint16_t end = current_x_base + 8;

					if (current_type == EvType::CD) {
						Metavision::Evt3::RawEventVect8 *ev_vec_8 =
							reinterpret_cast<Metavision::Evt3::RawEventVect8 *>(current_word);
						uint32_t valid = ev_vec_8->valid;
						for (uint16_t i = current_x_base; i != end; ++i) {
							if (valid & 0x1) {
								// We have a new Event CD with
								// x = i
								// y = current_cd_y
								// polarity = current_polarity
								// time = current_time (in us)
								//cd_str += std::to_string(i) + "," + std::to_string(current_cd_y) + "," +
								//	std::to_string(current_polarity) + "," + std::to_string(current_time) + "\n";
								if (evCount == 0)
									timeStamp = current_time;
								if (current_time - timeStamp > nStartTime) {
									evData.push_back(EBI::Event(
										(i % camSpecs.sensorW), // range check added 20240328
										current_cd_y,
										static_cast<int8_t>(current_polarity),
										static_cast<uint32_t>(current_time - timeStamp)
									));
#ifdef _DEBUG2
									if (i >= camSpecs.sensorW) {
										nEventOutOfBounds++;
									}
#endif
								}
								evCount++;
							}
							valid >>= 1;
						}
					}
					current_x_base = end;
					break;
				}
				case Metavision::Evt3::EventTypes::EVT_ADDR_Y: {
					current_type = EvType::CD;

					Metavision::Evt3::RawEventY *ev_cd_y = reinterpret_cast<Metavision::Evt3::RawEventY *>(current_word);
					// bugfix 20230208: issue with y out-of-bounds for data recorded with CenturyArks SilkyEvCam VGA
#ifdef _DEBUG2
					if (current_cd_y >= camSpecs.sensorH) {
						nEventOutOfBounds++;
					}
#endif
					//current_cd_y = ev_cd_y->y;
					current_cd_y = ev_cd_y->y % camSpecs.sensorH;	// quick method to treat out-of-bounds
					break;
				}

				case Metavision::Evt3::EventTypes::VECT_BASE_X: {
					Metavision::Evt3::RawEventXBase *ev_xbase =
						reinterpret_cast<Metavision::Evt3::RawEventXBase *>(current_word);
					current_polarity = ev_xbase->pol;
					current_x_base = ev_xbase->x;
					break;
				}
				case Metavision::Evt3::EventTypes::EVT_TIME_HIGH: {
					// Compute some useful constant variables :
					//
					// -> MaxTimestampBase is the maximum value that the variable current_time_base can have. It corresponds
					// to the case where an event Metavision::Evt3::RawEventTime of type EVT_TIME_HIGH has all the bits of
					// the field "timestamp" (12 bits total) set to 1 (value is (1 << 12) - 1). We then need to shift it by
					// 12 bits because this field represents the most significant bits of the event time base (range 23 to
					// 12). See the event description at the beginning of the file.
					//
					// -> TimeLoop is the loop duration (in us) before the time_high value wraps and returns to 0. Its value
					// is MaxTimestampBase + (1 << 12)
					//
					// -> LoopThreshold is a threshold value used to detect if a new value of the time high has decreased
					// because it looped. Theoretically, if the new value of the time high is lower than the last one, then
					// it means that is has looped. In practice, to protect ourselves from a transmission error, we use a
					// threshold value, so that we consider that the time high has looped only if it differs from the last
					// value by a sufficient difference (i.e. greater than the threshold)
					static constexpr Metavision::Evt3::timestamp_t MaxTimestampBase =
						((Metavision::Evt3::timestamp_t(1) << 12) - 1) << 12;                               // = 16773120us
					static constexpr Metavision::Evt3::timestamp_t TimeLoop = MaxTimestampBase + (1 << 12); // = 16777216us
					static constexpr Metavision::Evt3::timestamp_t LoopThreshold =
						(10 << 12); // It could be another value too, as long as it is a big enough value that we can be
									// sure that the time high looped

					Metavision::Evt3::RawEventTime *ev_timehigh =
						reinterpret_cast<Metavision::Evt3::RawEventTime *>(current_word);
					Metavision::Evt3::timestamp_t new_time_base = (Metavision::Evt3::timestamp_t(ev_timehigh->time) << 12);
					new_time_base += n_time_high_loop * TimeLoop;

					if ((current_time_base > new_time_base) &&
						(current_time_base - new_time_base >= MaxTimestampBase - LoopThreshold)) {
						// Time High loop :  we consider that we went in the past because the timestamp looped
						new_time_base += TimeLoop;
						++n_time_high_loop;
					}

					current_time_base = new_time_base;
					current_time = current_time_base;
					break;
				}
				case Metavision::Evt3::EventTypes::EVT_TIME_LOW: {
					Metavision::Evt3::RawEventTime *ev_timelow =
						reinterpret_cast<Metavision::Evt3::RawEventTime *>(current_word);
					current_time_low = ev_timelow->time;
					current_time = current_time_base + current_time_low;
					break;
				}
				case Metavision::Evt3::EventTypes::EXT_TRIGGER: {
					if (keep_triggers) {
						Metavision::Evt3::RawEventExtTrigger *ev_trigg =
							reinterpret_cast<Metavision::Evt3::RawEventExtTrigger *>(current_word);

						// We have a new Event Trigger with
						// value = ev_trigg->value
						// id = ev_trigg->id
						// time = current_time (in us)
						if (current_time - timeStamp > nStartTime) {
							evTrigger.push_back(EBI::TriggerEvent(ev_trigg->value, ev_trigg->id, static_cast<uint32_t>(current_time - timeStamp + nStartTime)));
							trigCount++;
						}
						//trigg_str += std::to_string(ev_trigg->value) + "," + std::to_string(ev_trigg->id) + "," +
						//	std::to_string(current_time) + "\n";
					}
					break;
				}
				default:
					break;
				}
			}
			if (evData.size() >= nMaxEventCount) {
			//if (evCount >= nMaxEventCount) {
				// buffer full
				break;
			}
		}
	}
	catch (int errCode) {
		std::cerr << "_loadRawEventData() - ERROR(" << errCode << ")" << std::endl;
		retCode = false;
	}
	if(input_file.is_open())
		input_file.close();

	size_t lastIndex = evData.size() - 1;
#ifdef _DEBUG2
	std::cout << "_loadRawEventData() - Have " << nEventOutOfBounds << " out-of-bound events" << std::endl;
	std::cout << "First time stamp at t=" << firstTimeStamp << " us\n" 
		<< "First event at t=" << evData[0].t << " us  [x:" << evData[0].x
		<< " y:" << evData[0].y << "]  pol=" << (evData[0].p ? "+" : "-") << std::endl;
	std::cout << "Last event at  t=" << evData[lastIndex].t << " us  [x:" << evData[lastIndex].x
		<< " y:" << evData[lastIndex].y << "]  pol=" << (evData[lastIndex].p ? "+" : "-") << std::endl;

#endif
	// consistency check - not part of Metavision SDK
	// tries to deal with corrupt data
	// check if time is monotonic
	int64_t t_prev = evData[0].t;
	int64_t t_end = evData[lastIndex].t;

	if (t_prev > t_end) {
		std::cout << "CAUTION: data may be faulty! first event (" << t_prev 
			<< " us) after end (" << t_end << " us)" << std::endl;
		evData[0].t = 0;
		t_prev = evData[0].t;
	}
	int nBadTiming = 0;
	for (size_t i = 0; i < evData.size(); i++)
	{
		int64_t t_now = evData[i].t;
		if (t_now > t_end) {
			nBadTiming++;
			// correct if exceeding t_max
			evData[i].t = static_cast<uint32_t>(t_prev);
		}
		else if (t_now < t_prev) {
			nBadTiming++;
		}
		t_prev = evData[i].t;
	}
	if (nBadTiming > 0) {
		std::cout << "CAUTION: data may be faulty! Have " << nBadTiming << " timing inconsistencies (non-monotonic)" << std::endl;
	}
	if (nEventOutOfBounds) {
		std::cout << "CAUTION: data may be faulty! Have " << nEventOutOfBounds << " out-of-bound events" << std::endl;
	}

	if (bDebugMessages) {
		std::cout << "Number of events: " << evData.size() 
			<< "\nNumber of trigger events: " << trigCount << std::endl;
	}
	return retCode;
}

EBI::EventData::EventData()
{
	init();
}

/*!
Constructor, loads data from RAW data file, if available
*/
EBI::EventData::EventData(const std::string& fnameRawEvents)
{
	init();
	loadRawData(fnameRawEvents);
}

EBI::EventData::~EventData()
{

}

void EBI::EventData::setMaximumSize(const uint64_t nMaxCnt)
{
	m_maxEvents = nMaxCnt;
}

bool EBI::EventData::isNull()
{
	return (m_events.size() == 0) 
		|| (m_camSpecs.sensorH < 1)
		|| (m_camSpecs.sensorW < 1)
		;
}

void EBI::EventData::clear()
{
	m_events.clear();
	init();
}

/*!
Constructor copying from other instance
*/
EBI::EventData::EventData(const EBI::EventData& src,
	const EBI::EventPolarity polMode,	//!< copy only events iwth the specified polarity
	const int32_t offsetUSec,
	const int32_t durationUSec,
	bool bSubtractOffsetTime)
{
	init();
	copyFrom(src, polMode, offsetUSec, durationUSec, bSubtractOffsetTime);
}

/*!
make a complete copy of \a src 
*/
bool EBI::EventData::copyFrom(const EBI::EventData& src)
{
	init();
	m_camSpecs = src.m_camSpecs;
	m_timeStamp = src.m_timeStamp;
	if (m_nDebugLevel > 0)
		std::cout << "EventData::copyFrom() - complete copy" << std::endl;
	for (EBI::Event ev : src.m_events) {
		m_events.push_back(ev);
	}
	for (EBI::TriggerEvent ev : src.m_triggerEvents) {
		m_triggerEvents.push_back(ev);
	}
	return true;
}

bool EBI::EventData::copyFrom(const EBI::EventData& src,
	const int32_t offsetUSec,
	const int32_t durationUSec)
{
	init();
	m_camSpecs = src.m_camSpecs;
	m_timeStamp = src.m_timeStamp;
	if (m_nDebugLevel > 0)
		std::cout << "EventData::copyFrom(t0=" << offsetUSec << "  duration=" << durationUSec << ")" << std::endl;
	int32_t t1 = offsetUSec;
	int32_t t2 = offsetUSec + durationUSec;
	if (durationUSec == 0) {
		// use end time in source
		t2 = src.m_events[src.m_events.size() - 1].t;
	}
	for (EBI::Event ev : src.m_events) {
		if (ev.t >= static_cast<uint32_t>(t1)) {
			if (ev.t <= static_cast<uint32_t>(t2)) {
				m_events.push_back(ev);
			}
		}
	}
	return true;
}

bool EBI::EventData::copyFrom(const EBI::EventData& src,
		const EBI::EventPolarity polMode,	//!< copy only events with the specified polarity
		const int32_t offsetUSec,
		const int32_t durationUSec,
		bool bSubtractOffsetTime)
{
	init();
	m_camSpecs = src.m_camSpecs;
	m_timeStamp = src.m_timeStamp;
	if (m_nDebugLevel > 0)
		std::cout << "EventData::copyFrom(t0=" << offsetUSec << "  duration=" << durationUSec << ")" << std::endl;
	// only copy events within specified time
	int32_t t1 = offsetUSec;
	int32_t t2 = offsetUSec + durationUSec;
	if (durationUSec == 0) {
		// use end time in source
		t2 = src.m_events[src.m_events.size() - 1].t;
	}
	for (EBI::Event ev : src.m_events) {
		if (ev.t >= static_cast<uint32_t>(t1)) {
			if (ev.t <= static_cast<uint32_t>(t2)) {
				if (bSubtractOffsetTime)
					ev.t -= offsetUSec;
				if (polMode == EBI::PolarityBoth)
					m_events.push_back(ev);
				else if ((ev.p > 0) && (polMode == EBI::PolarityPositive))
					m_events.push_back(ev);
				else if ((ev.p == 0) && (polMode == EBI::PolarityNegative))
					m_events.push_back(ev);
			}
		}
	}
	return true;
}

/*!
Constructor copying from other instance
*/
EBI::EventData::EventData(const EBI::EventData& src,
	const int32_t x, const int32_t y,
	const int32_t w, const int32_t h,
	const int32_t offsetUSec,	//!< offset from start in [usec]  
	const int32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
)
{
	init();
	copyFrom(src, x, y, w, h, offsetUSec, durationUSec);
}

bool EBI::EventData::copyFrom(const EBI::EventData& src,
		const int32_t x, const int32_t y,
		const int32_t w, const int32_t h,
		const int32_t offsetUSec,	//!< offset from start in [usec]  
		const int32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
	)
{
	init();
	m_camSpecs = src.m_camSpecs;
	m_timeStamp = src.m_timeStamp;
	// only copy events within specified time
	int32_t t1 = offsetUSec;
	int32_t t2 = t1 + durationUSec;
	bool bUseFullTime = false;
	if (m_nDebugLevel > 0)
		std::cout << "EventData::copyFrom(t0=" << offsetUSec << "  duration=" << durationUSec << ")" << std::endl;
	if (src.m_events.size() > 0) {
		if (t2 == t1) {
			// use end time
			t2 = src.m_events[src.m_events.size() - 1].t;
			bUseFullTime = true;
		}

		if (bUseFullTime) {
			for (EBI::Event ev : src.m_events) {
				if ((ev.y >= y) && (ev.y < (y + h))) {
					if ((ev.x >= x) && (ev.x < (x + w))) {
						ev.x -= x;
						ev.y -= y;
						ev.t -= t1;
						m_events.push_back(ev);
					}
				}
			}
		}
		else {
			for (EBI::Event ev : src.m_events) {
				if ((ev.t >= static_cast<uint32_t>(t1)) 
					&& (ev.t < static_cast<uint32_t>(t2))
					) {
					if ((ev.y >= y) && (ev.y < (y + h))) {
						if ((ev.x >= x) && (ev.x < (x + w))) {
							ev.x -= x;
							ev.y -= y;
							ev.t -= t1;
							m_events.push_back(ev);
						}
					}
				}
			}
		}
	}
	// todo: add structure with ROI info
	m_camSpecs.sensorH = h;	// probably should use individual identifier for ROI
	m_camSpecs.sensorW = w;
	return true;
}

/*!
Constructor copying from other instance
*/
EBI::EventData::EventData(const EBI::EventData& src,
	const EBI::EventPolarity polMode,	//!< copy only events iwth the specified polarity
	const int32_t x, const int32_t y,
	const int32_t w, const int32_t h,
	const int32_t offsetUSec,	//!< offset from start in [usec]  
	const int32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
)
{
	init();
	copyFrom(src, polMode, x, y, w, h, offsetUSec, durationUSec);
}

bool EBI::EventData::copyFrom(const EBI::EventData& src,
	const EBI::EventPolarity polMode,	//!< copy only events iwth the specified polarity
	const int32_t x, const int32_t y,
	const int32_t w, const int32_t h,
	const int32_t offsetUSec,	//!< offset from start in [usec]  
	const int32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
)
{
	init();
	m_camSpecs = src.m_camSpecs;
	m_timeStamp = src.m_timeStamp;
	// only copy events within specified time
	int32_t t1 = offsetUSec;
	int32_t t2 = t1 + durationUSec;
	bool bUseFullTime = false;
	if (m_nDebugLevel > 0)
		std::cout << "EventData::copyFrom(t0=" << offsetUSec << "  duration=" << durationUSec << ")" << std::endl;
	if (src.m_events.size() > 0) {
		if (t2 == t1) {
			// use end time
			t2 = src.m_events[src.m_events.size() - 1].t;
			bUseFullTime = true;
		}

		if (bUseFullTime) {
			for (EBI::Event ev : src.m_events) {
				if ((ev.y >= y) && (ev.y < (y + h))) {
					if ((ev.x >= x) && (ev.x < (x + w))) {
						ev.x -= x;
						ev.y -= y;
						ev.t -= t1;
						if (polMode == EBI::PolarityBoth)
							m_events.push_back(ev);
						else if ((ev.p > 0) && (polMode == EBI::PolarityPositive))
							m_events.push_back(ev);
						else if ((ev.p == 0) && (polMode == EBI::PolarityNegative))
							m_events.push_back(ev);
						//m_events.push_back(ev);
					}
				}
			}
		}
		else {
			for (EBI::Event ev : src.m_events) {
				if ((ev.t >= static_cast<uint32_t>(t1))
					&& (ev.t < static_cast<uint32_t>(t2))
					) {
					if ((ev.y >= y) && (ev.y < (y + h))) {
						if ((ev.x >= x) && (ev.x < (x + w))) {
							ev.x -= x;
							ev.y -= y;
							ev.t -= t1;
							m_events.push_back(ev);
						}
					}
				}
			}
		}
	}
	// todo: add structure with ROI info
	m_camSpecs.sensorH = h;	// probably should use individual identifier for ROI
	m_camSpecs.sensorW = w;
	return true;
}


void EBI::EventData::init()
{
	m_events.resize(0);
	m_triggerEvents.resize(0);
	m_timeStamp = 0;
	m_camSpecs.init();
	m_errMsg = "";
	m_nDebugLevel = 0;
	m_maxEvents = 100'000'000;	// about 3.5s at 30MEv/s
}

/*!
Enable debugging messages
*/
void EBI::EventData::setDebugLevel(const int32_t nLevel)
{
	m_nDebugLevel = nLevel;
	if(m_nDebugLevel > 0)
		std::cout << "EBI::EventData: debugging set to level " << m_nDebugLevel << std::endl;
}

/*!
Access to event data; provides complete copy of vector
*/
std::vector<EBI::Event> EBI::EventData::data()
{
	return m_events;
}

/*!
Access to reference of event data
*/
std::vector<EBI::Event>& EBI::EventData::dataRef()
{
	return m_events;
}

/*!
Access to trigger event data; provides complete copy of vector
*/
std::vector<EBI::TriggerEvent> EBI::EventData::triggerEvents()
{
	return m_triggerEvents;
}

/*!
Access to reference of trigger event data
*/
std::vector<EBI::TriggerEvent>& EBI::EventData::triggerRef()
{
	return m_triggerEvents;
}


/*!
Load data from RAW data file
\return true on success, false on failure (e.g. missing file, worng format)
*/
bool EBI::EventData::loadRawData(const std::string& fnameRawEvents,
	const uint32_t offsetUSec,	//!< offset from start in [usec]  
	const uint32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
)
{
#ifdef _DEBUG2
	m_nDebugLevel = 1;
#endif
	m_events.resize(0);
	m_triggerEvents.resize(0);
	m_timeStamp = 0;
	m_camSpecs.init();
	return _loadRawEventData(
		fnameRawEvents, 
		m_events, 
		m_triggerEvents, 
		m_timeStamp,
		m_camSpecs, 
		offsetUSec,
		m_maxEvents, 
		m_nDebugLevel>0);
}

/*! \cond
 * header for event data files
 */
struct _EVENT_FILE_HDR 
{
	int32_t		Signature;		//!< "EVT3"
	uint64_t	FileSize;		//!< size in bytes including header
	uint64_t	EventCount;		//!< number of events in file
	uint64_t	TimeStamp;		//!< time in [usec] of first event from RAW file
	uint32_t	Duration;		//!< in [usec]
	uint32_t	HeaderLength;	//!< should be 64 
	uint32_t	cols, rows;		//!< size of image
	uint32_t	_reserved1; //!< bytes 49...52
	uint32_t	_reserved2; //!< bytes 53...56
	uint32_t	_reserved3; //!< bytes 57...60
	uint32_t	_reserved4; //!< bytes 61...64
};
#define _EVENT_FILE_HDR_SIZE 64

struct PACKED_EVENT
{
	uint16_t	x, y;		//!< pixel coords
	uint32_t	timePol;	//!< time in [usec] with polarity in lowest bit
};
#define _PACKED_EVENT_SIZE 8
//! \endcond 

/* alternative way
union {
	struct {
		uint32_t year : 23;
		uint32_t day : 5;
		uint32_t month : 4;
	};
	uint32_t ydm;
} typedef BitfieldFast;

int main() {

	BitfieldFast date_f;
	date_f.ydm = 2020 | (13 << 23) | (12 << 28);
*/
union {
	struct {
		uint64_t x : 16;
		uint64_t y : 16;
		uint64_t time : 31;
		uint64_t pol : 1;
	};
	uint64_t ev;
} typedef BitEventFast;
//BitEventFast ev_f;
//ev_f.ev = x | (y << 16) | (time << 32) | (t << 63);

bool EBI::EventData::save(const std::string& fnameEvents,
	const uint32_t offsetUSec, const uint32_t durationUSec)
{
	if (m_events.size() == 0)
		return false;	// no data to save

	// figure out which samples to write
	uint32_t t1 = offsetUSec;
	uint32_t t2 = t1 + durationUSec;
	if (t1 >= m_events[m_events.size() - 1].t) {
		// out of range
		std::cerr << "EBI::EventData::save(): Error: start time beyond range" << std::endl;
		return false;
	}
	if (t2 > m_events[m_events.size() - 1].t) {
		t2 = m_events[m_events.size() - 1].t + 1;
	}
	size_t idx1 = 0;
	if (t1 > 0) {
		for (size_t i = 0; i < m_events.size(); i++) {
			if (m_events[i].t >= t1) {
				idx1 = i;
				break;
			}
		}
	}
	size_t idx2 = m_events.size();
	if (t2 < m_events[m_events.size() - 1].t) {
		for (size_t i = idx1; i < m_events.size(); i++) {
			if (m_events[i].t > t2) {
				idx2 = i;
				break;
			}
		}
	}
	size_t numEventsOut = idx2 - idx1;

	bool retCode = true;
	std::ofstream outFile(fnameEvents, std::ios::out | std::ios::binary);
	try {
		if (!outFile.is_open()) {
			m_errMsg = "failed opening file for output";
			throw (-1);
		}

		_EVENT_FILE_HDR hdr;
		hdr.Signature = 0x33545645; // "EVT3"
		//hdr.Signature = 0x32545645; // "EVT2" - older format

		hdr.FileSize = (sizeof(_EVENT_FILE_HDR) + (numEventsOut * _PACKED_EVENT_SIZE));
		hdr.EventCount = numEventsOut;
		hdr.Duration = (t2 - t1);
		hdr.TimeStamp = m_timeStamp;
		hdr.cols = m_camSpecs.sensorW;
		hdr.rows = m_camSpecs.sensorH;
		hdr.HeaderLength = sizeof(_EVENT_FILE_HDR);

		outFile.write(reinterpret_cast<char*>(&hdr), _EVENT_FILE_HDR_SIZE);

		// write out data - probably could be optimized by writing bigger chunks
		for (size_t i = idx1; i < idx1 + numEventsOut; i++) {
			PACKED_EVENT pe;
			pe.x = m_events[i].x;
			pe.y = m_events[i].y;
			pe.timePol = (m_events[i].t << 1);
			if (m_events[i].p > 0)
				pe.timePol |= 0x1;
			outFile.write((char*)&pe, _PACKED_EVENT_SIZE);
		}
		//for (EBI::Event ev : m_events) {
		//	PACKED_EVENT pe;
		//	pe.x = ev.x;
		//	pe.y = ev.y;
		//	pe.timePol = (ev.t << 1);
		//	if (ev.p > 0)
		//		pe.timePol |= 0x1;
		//	outFile.write((char*)&pe, _PACKED_EVENT_SIZE);
		//}
	}
	catch (int errCode)
	{
		std::cerr << "ERROR(" << errCode << "): EBI::EventData::save() " << m_errMsg << std::endl;
		retCode = false;
	}
	if (outFile.is_open())
		outFile.close();
	if (m_nDebugLevel > 0)
		std::cout << "EBI::EventData::save('" << fnameEvents << "') - OK" << std::endl;
	return retCode;
}

/*!
Load event data
Clears existing event data set
\return True on success
*/
bool EBI::EventData::load(
	const std::string& fnameEvents, //!< file name, can be either Metavision RAW or own EVT3
	const uint32_t offsetUSec,	//!< offset from start in [usec]  
	const uint32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
)
{
	// determine type of file
	EBI::FileFormat eType = EBI::GetFileType(fnameEvents);
	if (eType == EBI::FILE_FORMAT_UNKNOWN) {
		std::cerr << "ERROR: Unknown fiile type!" << std::endl;
		return false;
	}
	else if (eType == EBI::FILE_FORMAT_RAWEVT3) {
		return loadRawData(fnameEvents, offsetUSec, durationUSec);
	}

	// load EVT3 type instead...
	bool retCode = true;
	std::ifstream inFile(fnameEvents, std::ios::in | std::ios::binary);
	try {
		if (!inFile.is_open()) {
			m_errMsg = "failed opening file";
			throw (-1);
		}
		m_events.resize(0);

		_EVENT_FILE_HDR hdr;
		inFile.read((char*)&hdr, _EVENT_FILE_HDR_SIZE);
		//inFile.read(reinterpret_cast<char*>(hdr), _EVENT_FILE_HDR_SIZE);

		if (m_nDebugLevel > 0)
			std::cout
				<< "File size:        " << hdr.FileSize << std::endl
				<< "Event count:      "  << hdr.EventCount << std::endl
				<< "Duration [usec]:  " << hdr.Duration << std::endl
				<< "TimeStamp [usec]: " << hdr.TimeStamp << std::endl;

		if (offsetUSec > hdr.Duration) {
			m_errMsg = "start beyond end of file";
			throw (-2);
		}

		// read first event

		PACKED_EVENT pe;
		inFile.read((char*)&pe, _PACKED_EVENT_SIZE);
		uint64_t t0 = (pe.timePol >> 1) + offsetUSec;
		uint64_t tN = t0 + durationUSec;
		if (durationUSec == 0)
			tN = (pe.timePol >> 1) + hdr.Duration;

		while (inFile) {
			uint32_t curTime = (pe.timePol >> 1);
			if (curTime < t0) {
				// skip - load more events
			}
			else if (curTime > tN) {
				// skip - done
				break;
			}
			else {
				EBI::Event ev;
				ev.x = pe.x;
				ev.y = pe.y;
				ev.t = curTime;
				ev.p = (pe.timePol & 0x1) ? 1 : 0;
				m_events.push_back(ev);
			}
			// load next
			inFile.read((char*)&pe, _PACKED_EVENT_SIZE);
		}
	}
	catch (int errCode)
	{
		std::cerr << "ERROR(" << errCode << "): EBI::EventData::load() " << m_errMsg << std::endl;
		if (inFile.is_open())
			inFile.close();

		retCode = false;
	}
	if (inFile.is_open())
		inFile.close();

#ifdef _DEBUG
	if (retCode) {
		std::cout << "Current number of events: " << (m_events.size()) << std::endl;
		double msecs = static_cast<double>(m_events[m_events.size() - 1].t - m_events[0].t) / 1000;
		std::cout << "\tduration: " << msecs << " millisec\n";
		int cnt = 0;
		for (EBI::Event ev : m_events)
		{
			std::cout << ev.x << " " << ev.y << (((ev.p) > 0) ? " + " : " - ") << ev.t << std::endl;
			cnt++;
			if (cnt > 10)
				break;
		}
	}
#endif

	return retCode;
}

/*!
Reduce event data set to specified region of interest (ROI).
Events beyond range of ROI are discarded.
\return TRUE on success, FALSE on invalid ROI or missing data
*/
bool EBI::EventData::cropROI(
	const int32_t x, const int32_t y,
	const int32_t w, const int32_t h,
	//const uint32_t t1IN, const uint32_t t2IN)
	const int32_t offsetUSec,	//!< offset from start in [usec]  
	const int32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
	)
{
	if (m_events.size() < 1)
		return false;
	int32_t roiX = x;
	int32_t roiY = y;
	int32_t roiW = w;
	int32_t roiH = h;
	if (roiX < 0)
		roiX = 0;
	if (roiY < 0)
		roiY = 0;
	int32_t imgW = m_camSpecs.sensorW;
	int32_t imgH = m_camSpecs.sensorH;
	if (roiX + roiW > imgW)
		roiW = imgW - roiX;
	if (roiY + roiH > imgH)
		roiH = imgH - roiY;
	if ((roiW < 4) || (roiH < 4)) {
		// todo: fill errMsg
		std::cerr << "ERROR: invalid ROI: X=" << roiX << " Y=" << roiY
			<< " W=" << roiW << " H=" << roiH
			<< "  image size: " << imgW << "(W) x " << imgH << "(H)"
			<< std::endl;
		return false;
	}
	uint32_t t1 = offsetUSec;
	uint32_t t2 = t1 + durationUSec;
	bool bUseFullTime = false;
	if (t2 == t1) {
		// use end time
		t2 = m_events[m_events.size() - 1].t;
		bUseFullTime = true;
	}
	std::vector<EBI::Event> roiData;

	if (bUseFullTime) {
		for (EBI::Event ev : m_events) {
			if ((ev.y >= roiY) && (ev.y < (roiY + roiH))) {
				if ((ev.x >= roiX) && (ev.x < (roiX + roiW))) {
					ev.x -= roiX;
					ev.y -= roiY;
					ev.t -= t1;
					roiData.push_back(ev);
				}
			}
		}
	}
	else {
		for (EBI::Event ev : m_events) {
			if ((ev.t >= t1) && (ev.t < t2)) {
				if ((ev.y >= roiY) && (ev.y < (roiY + roiH))) {
					if ((ev.x >= roiX) && (ev.x < (roiX + roiW))) {
						ev.x -= roiX;
						ev.y -= roiY;
						ev.t -= t1;
						roiData.push_back(ev);
					}
				}
			}
		}
	}
	// copy back to current data set
	m_events.resize(0);
	//// Copying vector by assign function
	//m_events.assign(roiData.begin(), roiData.end());
	for (int i = 0; i < roiData.size(); i++)
		m_events.push_back(roiData[i]);
	m_camSpecs.sensorH = roiH;	// probably should use individual identifier for ROI
	m_camSpecs.sensorW = roiW;
	roiData.resize(0);
	return true;
}

/*!
Sample the data set
Also subtracts time \a t1 and top-left coordinates \a (x,y) from event
\note Uses all events
*/
std::vector<EBI::Event> EBI::EventData::getSample(
	const int32_t x, const int32_t y,
	const int32_t w, const int32_t h,
	const int32_t offsetUSec,	//!< offset from start in [usec]  
	const int32_t durationUSec	//!< duration to long in [usec], 0 to load entire set
	)
{
	std::vector<EBI::Event> sample;
	sample.resize(0);
	if (m_events.size() < 1)
		return sample;

	int32_t x1 = x;
	int32_t x2 = x1 + w;
	int32_t y1 = y;
	int32_t y2 = y1 + h;
	int32_t t1 = offsetUSec;
	int32_t t2 = t1 + durationUSec;
	bool bUseFullTime = false;
	if (t2 == t1) {
		// use end time
		t2 = m_events[m_events.size() - 1].t;
		bUseFullTime = true;
	}

	// data is sorted in time
	if (bUseFullTime) {
		for (EBI::Event ev : m_events) {
			if ((ev.y >= y1) && (ev.y < y2)) {
				if ((ev.x >= x1) && (ev.x < x2)) {
						ev.x -= x1;
						ev.y -= y1;
						ev.t -= t1;
						sample.push_back(ev);
				}
			}
		}
	}
	else {
		for (EBI::Event ev : m_events) {
			if ((ev.t >= static_cast<uint32_t>(t1)) 
				&& (ev.t < static_cast<uint32_t>(t2))
				) {
				if ((ev.y >= y1) && (ev.y < y2)) {
					if ((ev.x >= x1) && (ev.x < x2)) {
							ev.x -= x1;
							ev.y -= y1;
							ev.t -= t1;
							sample.push_back(ev);
					}
				}
			}
		}
	}
	return sample;
}
