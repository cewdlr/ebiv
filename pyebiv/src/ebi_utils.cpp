#include "ebi.h"

#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <io.h>   // For access().
#endif
#include <sys/types.h>  // For stat().
#include <sys/stat.h>   // For stat().
#include <string>
using namespace std;

/*! \cond
 * header for event data files
 */
struct _SIMPLE_FILE_HDR
{
	int8_t	_bytes[16];		
};
#define _SIMPLE_FILE_HDR_SIZE 16
//! \endcond 

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

/*!
Determine file type by reading header

Typical header looks like this:
% camera_integrator_name CenturyArks
% date 2023-06-23 19:45:32
% evt 3.0
% format EVT3;height=480;width=640
% generation 3.1
% geometry 640x480
% integrator_name CenturyArks
% plugin_integrator_name CenturyArks
% plugin_name evc3a_plugin_gen31
% sensor_generation 3.1

*/
// older versions (pre.4.0) of did not specify image size or geometry, 
// so size has to be inferred from plugin name
EBI::FileFormat EBI::GetFileType(const std::string& fnIN)
{
	std::ifstream inFile(fnIN, std::ios::in | std::ios::binary);
	if (!inFile.is_open()) {
		std::cerr << "ERROR: failed opening file: '" << fnIN << "'" << std::endl;
		return EBI::FILE_FORMAT_UNKNOWN;
	}
	// Read the header of the input file, if present :
	int line_first_char = inFile.peek();

	bool bIsRawEventFileType3 = false;
	EBI::EventCameraSpecs camSpecs;

	while (line_first_char == '%') {
		std::string line;
		std::getline(inFile, line);
		//std::cout << line << std::endl;
		if (line == "% end") {	// added in v4.0.0
			break;
		}
		if (line == "% evt 3.0")
			bIsRawEventFileType3 = true;

		std::vector<std::string> vals = _stringSplit(line, ' ');

		if (vals.size() == 3) {
			if (vals[1] == "integrator_name")
				camSpecs.strIntegrator = vals[2];

			else if (vals[1] == "plugin_name") {
				camSpecs.strPlugin = vals[2];
				if (camSpecs.strPlugin == "hal_plugin_gen41_evk2") {
					//std::cout << "Sensor: EVK2 - Gen.4.1" << std::endl;
					// infer size from plugin namwe
					camSpecs.sensorH = 720;
					camSpecs.sensorW = 1280;
				}
				else if (camSpecs.strPlugin == "hal_plugin_imx636_evk4") {
					camSpecs.sensorH = 720;
					camSpecs.sensorW = 1280;
				}
				else if (camSpecs.strPlugin == "evc3a_plugin_gen31") {
					//std::cout << "Sensor: CenturyArks VGA Gen.3.1" << std::endl;
					camSpecs.sensorH = 480;
					camSpecs.sensorW = 640;
				}
				else if (camSpecs.strPlugin == "evc4a_plugin_imx636") {
					camSpecs.sensorH = 720;
					camSpecs.sensorW = 1280;
				}
			}

			else if (vals[1] == "firmware_version")
				camSpecs.strFirmware = vals[2];

			else if (vals[1] == "evt") {
				camSpecs.strEventType = vals[2];
				if (camSpecs.strEventType == "3.0")
					bIsRawEventFileType3 = true;
			}

			else if (vals[1] == "geometry") {
				if (vals[2] == "640x480") {
					camSpecs.sensorH = 480;
					camSpecs.sensorW = 640;
				}
				else if (vals[2] == "1280x720") {
					camSpecs.sensorH = 720;
					camSpecs.sensorW = 1280;
				}
			}
			else if (vals[1] == "serial_number")
				camSpecs.strSerialNo = vals[2];
			else if (vals[1] == "date") {
				camSpecs.strRecordingDate = vals[2];
				camSpecs.strRecordingTime = vals[3];
			}
		}
		line_first_char = inFile.peek();
	};
	inFile.close();

	if ((camSpecs.sensorW == 0)
		|| (camSpecs.sensorH == 0))
	{
		//std::cout << "File is of unknown format" << std::endl;
		return EBI::FILE_FORMAT_UNKNOWN;
	}
	if (bIsRawEventFileType3) {
		//std::cout << "File is of type raw EVT3" << std::endl;
		return EBI::FILE_FORMAT_RAWEVT3;
	}

	return EBI::FILE_FORMAT_EVT3;
}

bool EBI::CheckDirectoryExistence(const std::string& dirIN)
{
#ifdef _WIN32
	if (_access(dirIN.c_str(), 0) == 0)
	{
		struct stat status;
		stat(dirIN.c_str(), &status);

		if (status.st_mode & S_IFDIR)
		{
			//std::cout << "The directory exists." << endl;
			return true;
		}
		else
		{
			//std::cout << "The path you entered is a file." << endl;
			return false;
		}
	}
	//else
	//{
	//	std::cout << "Path doesn't exist." << endl;
	//}
#else 
	// todo: implement for POSIX...
#endif
	return false;
}

std::string EBI::FileBaseName(const std::string& sIN)
{
	std::string::size_type i = sIN.rfind('.', sIN.length());
	std::string::size_type fwdslash = sIN.rfind('/', sIN.length());
	std::string::size_type backslash = sIN.rfind('\\', sIN.length());
	std::string sOUT = sIN;

	size_t slashPos = 0;
	if (fwdslash < i)
		slashPos = fwdslash+1;
	else if (backslash < i)
		slashPos = backslash+1;

	if (i != std::string::npos) {
		sOUT = sIN.substr(slashPos, i-slashPos);
	}
	return sOUT;
}

std::string EBI::FileReplaceExtension(const std::string& sIN, const std::string& newExt) 
{
	std::string::size_type i = sIN.rfind('.', sIN.length());
	std::string sOUT = sIN;

	// also check that slash position is less than dot-pos (period could be part of path)
	std::string::size_type fwdslash = sIN.rfind('/', sIN.length());
	std::string::size_type backslash = sIN.rfind('\\', sIN.length());
	size_t slashPos = 0;
	if (fwdslash < i)
		slashPos = fwdslash + 1;
	else if (backslash < i)
		slashPos = backslash + 1;

	if ((i != std::string::npos) && (slashPos < i)) {
		sOUT.replace(i + 1, newExt.length(), newExt);
	}
	else {
		sOUT.append(newExt);
	}
	return sOUT;
}

/*!
Determine mean histogram of events over specified number of periods.
The data is referenced to the beginning of the event record (t=0), so the pulse
is not necessarily centered within the period.

\return Vector of histogram entries (mean counts) in events per microsecond
*/
std::vector<double> EBI::MeanPulseHistogram(
	EBI::EventData& evData, 	//!< input data
	const double freqInHz,
	const int32_t nBinWidthInMicrosec,
	const int32_t nPeriods,		//!< number of periods to sample
	const int32_t nStartPeriod, //!< at which period to begin sampling
	const int32_t nDebug		//!< enables debugging output
	)
{
	double period = 1e6 / freqInHz;
	int32_t nBins = int(period / nBinWidthInMicrosec);
	std::vector<double> histData;
	histData.resize(nBins);
	for (size_t i = 0; i < nBins; i++)
		histData[i] = 0;
	int32_t t0 = int32_t(period * nStartPeriod);	// start on some time into data set
	int32_t sampleTime = int32_t(nPeriods * period);
	// get N samples
	EBI::EventData evSlab(evData, EBI::EventPolarity::PolarityPositive, t0, sampleTime); // use only positive events

	EBI::Event ev0 = evSlab.data()[0];
	EBI::Event evN = evSlab.data().back();

	if (nDebug>0)
		std::cout << "Subset of events starting at " << t0 << " usec" << std::endl
			<< "event count  " << evSlab.data().size() << std::endl
			<< "first event  " << ev0.t << " usec" << std::endl
			<< "last event   " << evN.t << " usec" << std::endl
			<< "frequency    " << freqInHz << " Hz" << std::endl
			<< "period       " << period << " usec" << std::endl
			<< "bin width    " << nBinWidthInMicrosec << " usec" << std::endl
			<< "start time   " << t0 << " usec" << std::endl;
	for (EBI::Event ev : evSlab.data()) {
		// find remainder
		double curTime = ev.t + t0;
		double relTime = floor(curTime / period);
		double rem = floor(curTime - (relTime * period));

		// find the bin
		int32_t bin = static_cast<int32_t>(rem / nBinWidthInMicrosec);
		if ((bin >= 0) && (bin < histData.size())) {
			histData[bin]++;
		}
	}

	// normalize to get events/microsecond
	for (size_t i = 0; i < histData.size(); i++) {
		histData[i] /= (nPeriods * nBinWidthInMicrosec);
	}

	if (nDebug > 1) {
		double numEvents = 0;
		std::cout << " --- [Bin] ----- [count] ---" << std::endl;
		for (size_t i = 0; i < histData.size(); i++) {
			std::cout << "    " << (i*nBinWidthInMicrosec) << " usec  -->  " 
				<< (histData[i]) << std::endl;
			numEvents += histData[i];
		}
		std::cout << "mean events per period: " << numEvents << std::endl;
	}
	return histData;
}

/*!
Determine optimal sampling time-offset to capture events generated by pulsed illumination
at fixed frequency \a freq
\return offset-time in [usec]
*/
int32_t EBI::DetermineOffsetTime(
	EBI::EventData& evData,		//!< input data
	const double freqInHz,		//!< sampling frequency = light pulsing frequency
	const int32_t nBinWidth,	//!< in [usec]
	const int32_t nPeriods,		//!< number of periods to sample
	const int32_t nStartPeriod,	//!< at which period to begin sampling
	const int32_t nDebug		//!< enables debugging output
)
{
	std::vector<double> histData = EBI::MeanPulseHistogram(
		evData, freqInHz, nBinWidth, nPeriods, nDebug);

	int32_t bestStartTime = 0;
	int32_t maxIdx = 0;
	int32_t szHist = static_cast<int>(histData.size());

	// start from max index and move back to find minimum
	maxIdx = 0;
	double maxVal = 0;
	for (int32_t i = 0; i < szHist; i++) {
		if (maxVal < histData[i]) {
			maxVal = histData[i];
			maxIdx = i;
		}
	}
	if (nDebug>0)
		std::cout << "Maximum at " << (maxIdx*nBinWidth) << " usec  with "
		 << (maxVal) << " average events" << std::endl;
	double minVal = maxVal;
	int32_t minIdx = maxIdx;
	for (int32_t i = 0; i < szHist; i++) {
		int32_t idx = maxIdx - i;
		if(idx < 0)
			idx += szHist;
		if (minVal > histData[idx]) {
			minVal = histData[idx];
			minIdx = idx;
			bestStartTime = (idx*nBinWidth);
		}
	}
	if (nDebug>0)
		std::cout << "Optimum sampling at delay of " << (bestStartTime) 
			<< " usec at minimum of " << (minVal) << " average events" 
			<< std::endl;

	return static_cast<int32_t>(bestStartTime);
}
