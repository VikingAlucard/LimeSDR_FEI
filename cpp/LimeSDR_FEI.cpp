/**************************************************************************

    This is the device code. This file contains the child class where
    custom functionality can be added to the device. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "LimeSDR_FEI.h"
#include <limits>
#include "lime/LimeSuite.h"

typedef std::numeric_limits<double> dbl;

// TODO UploadWFM and EnableTxWFM functions?

// device structure, should be initialize to NULL
lms_device_t* device = NULL;

// TODO handle more than one stream, how to pass/access proper stream when stuff happens?
lms_stream_t streamId;

bool channel_active = false;

PREPARE_LOGGING(LimeSDR_FEI_i)

LimeSDR_FEI_i::LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl) :
    LimeSDR_FEI_base(devMgr_ior, id, lbl, sftwrPrfl)
{
}

LimeSDR_FEI_i::LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl, char *compDev) :
    LimeSDR_FEI_base(devMgr_ior, id, lbl, sftwrPrfl, compDev)
{
}

LimeSDR_FEI_i::LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl, CF::Properties capacities) :
    LimeSDR_FEI_base(devMgr_ior, id, lbl, sftwrPrfl, capacities)
{
}

LimeSDR_FEI_i::LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl, CF::Properties capacities, char *compDev) :
    LimeSDR_FEI_base(devMgr_ior, id, lbl, sftwrPrfl, capacities, compDev)
{
}

/* See below
LimeSDR_FEI_i::~LimeSDR_FEI_i()
{
}
*/

// TODO how do I properly call the destructor if something goes wrong?
LimeSDR_FEI_i::~LimeSDR_FEI_i()
{
	// close device
    if (device != NULL) { LMS_Close(device); }
    LOG_DEBUG(LimeSDR_FEI_i, "~LimeSDR_FEI_i() releasing LimeSDR_FEI FEI");
}

void LimeSDR_FEI_i::Error(std::string err) {
	LOG_ERROR(LimeSDR_FEI_i, err);
    throw FRONTEND::FrontendException(err.c_str());
}

void LimeSDR_FEI_i::constructor()
{
    /***********************************************************************************
     This is the RH constructor. All properties are properly initialized before this function is called 

     For a tuner device, the structure frontend_tuner_status needs to match the number
     of tuners that this device controls and what kind of device it is.
     The options for devices are: TX, RX, RX_DIGITIZER, CHANNELIZER, DDC, RC_DIGITIZER_CHANNELIZER
     
     For example, if this device has 5 physical
     tuners, 3 RX_DIGITIZER and 2 CHANNELIZER, then the code in the construct function 
     should look like this:

     this->addChannels(3, "RX_DIGITIZER");
     this->addChannels(2, "CHANNELIZER");
     
     The incoming request for tuning contains a string describing the requested tuner
     type. The string for the request must match the string in the tuner status.
    ***********************************************************************************/
	// Find attached devices
		int num_devices;
		lms_info_str_t device_list[8];   // NOTE should be large enough to hold all detected devices

		// NULL can be passed to only get number of devices
		if ((num_devices = LMS_GetDeviceList(device_list)) < 0) { Error(LMS_GetLastErrorMessage()); }

		if (num_devices < 1) {
			std::cout << "No LimeSDR_FEI found" << std::endl;
			// TODO how to exit properly? or just wait and have a way to try again later?
			//this->stop();

		} else {

			std::cout << "Devices found " << std::endl;
			for (int i = 0; i < num_devices; i++) {
				std::cout << i << ": " << device_list[i] << std::endl;
			}

			// If there's a specific LimeSDR_FEI device_addr specified, figure out which one that is and attach to it
			bool found = false;
			if(!device_addr.empty()) {
				for (int i = 0; i < num_devices; i++) {
					if (std::string(device_list[i]).find(device_addr) != std::string::npos) {
						found = true;
						if (LMS_Open(&device, device_list[i], NULL)) { Error(LMS_GetLastErrorMessage()); }
						std::cout << "#### Device " << i << " #####################" << std::endl;
						break;
					}
				}
				if (!found) {   // we didn't find that device
					std::string err = "LimeSDR_FEI with device address '"+device_addr+"' not found.";
					Error(err);
				}

			// Else just attach to first available LimeSDR_FEI
			} else {
				for (int i = 0; i < num_devices; i++) {
					if (!(LMS_Open(&device, device_list[i], NULL))) {
						std::cout << "#### Device " << i << " #####################" << std::endl;
						break;
					}
				}
			}

			//Initialize device with default configuration
			//Do not use if you want to keep existing configuration
			//Use LMS_LoadConfig(device, "/path/to/file.ini") to load config from INI
			if (LMS_Init(device) != 0) { Error(LMS_GetLastErrorMessage()); }

			// get number of RX channels
			int num_rx_channels;
			if ((num_rx_channels = LMS_GetNumChannels(device, LMS_CH_RX)) < 0) { Error(LMS_GetLastErrorMessage()); }
			std::cout << "Number of RX channels: " << num_rx_channels << std::endl;

			// TODO this kind of thing has to go somewhere for initial setup when adding channels...
			//frontend_tuner_status[tuner_id].tuner_number = device_channels[tuner_id].chan_num;

			for (int i = 0; i < num_rx_channels; i++) {
				std::cout << "RX Channel " << i << std::endl;
				// get ranges and current status of RX channels
				// also fills initial frontend_tuner_status structs and device_channels
				getChannelProperties(i, LMS_CH_RX);
				// get advanced status properties of RX channels
				getAdvancedControlStatus(i, LMS_CH_RX);
				std::cout << std::endl;
			}

			// get number of TX channels
			int num_tx_channels;
			if ((num_tx_channels = LMS_GetNumChannels(device, LMS_CH_TX)) < 0) { Error(LMS_GetLastErrorMessage()); }
			std::cout << "Number of TX channels: " << num_tx_channels << std::endl;

			for (int i = 0; i < num_tx_channels; i++) {
				std::cout << "TX Channel " << i << std::endl;
				// get ranges and current status of TX channels
				// also fills initial frontend_tuner_status structs and device_channels
				getChannelProperties(i, LMS_CH_TX);
				// get advanced status properties of TX channels
				getAdvancedControlStatus(i, LMS_CH_TX);
				std::cout << std::endl;
			}


			// TODO sort of unnecessary here since it's also included in getChannelProperties(). This is for when you only need status, not range.
			//      but we use this function to update the frontend_tuner_status struct so do it anyway
			// get current status of channels
			std::cout << "#### Channel Status Update #####################" << std::endl;
			for (int i = 0; i < num_rx_channels; i++) {
				std::cout << "RX Channel " << i << std::endl;
				getChannelStatus(i, LMS_CH_RX);
				std::cout << std::endl;
			}

			for (int i = 0; i < num_tx_channels; i++) {
				std::cout << "TX Channel " << i << std::endl;
				getChannelStatus(i, LMS_CH_TX);
				std::cout << std::endl;
			}

			std::cout << "#### frontend_tuner_status check #####################" << std::endl;
			for(size_t tuner_id = 0; tuner_id < frontend_tuner_status.size(); tuner_id++) {
				std::cout << "  Tuner Type: " << frontend_tuner_status[tuner_id].tuner_type << std::endl;
				std::cout << "  Tuner Number: " << frontend_tuner_status[tuner_id].tuner_number << std::endl;
				std::cout << "  Enabled: " << frontend_tuner_status[tuner_id].enabled << std::endl;
				std::cout << "  Bandwidth: " << frontend_tuner_status[tuner_id].bandwidth << std::endl;
				std::cout << "  Bandwidth Range: " << frontend_tuner_status[tuner_id].available_bandwidth << std::endl;
				std::cout << "  Bandwidth Tolerance: " << frontend_tuner_status[tuner_id].bandwidth_tolerance << std::endl;
				std::cout << "  Center Frequency: " << frontend_tuner_status[tuner_id].center_frequency << std::endl;
				std::cout << "  Center Frequency Range: " << frontend_tuner_status[tuner_id].available_frequency << std::endl;
				std::cout << "  Gain: " << frontend_tuner_status[tuner_id].gain << std::endl;
				std::cout << "  Gain Range: " << frontend_tuner_status[tuner_id].available_gain << std::endl;
				std::cout << "  Sample Rate: " << frontend_tuner_status[tuner_id].sample_rate << std::endl;
				std::cout << "  Sample Rate Range: " << frontend_tuner_status[tuner_id].available_sample_rate << std::endl;
				std::cout << "  Sample Rate Tolerance: " << frontend_tuner_status[tuner_id].sample_rate_tolerance << std::endl;
				std::cout << "  Allocation ID CSV: " << frontend_tuner_status[tuner_id].allocation_id_csv << std::endl;
				std::cout << "  AGC: " << frontend_tuner_status[tuner_id].agc << std::endl;
				std::cout << "  Complex: " << frontend_tuner_status[tuner_id].complex << std::endl;
				std::cout << "  Decimation: " << frontend_tuner_status[tuner_id].decimation << std::endl;
				std::cout << "  Group ID: " << frontend_tuner_status[tuner_id].group_id << std::endl;
				std::cout << "  Reference Source: " << frontend_tuner_status[tuner_id].reference_source << std::endl;
				std::cout << "  RF Flow ID: " << frontend_tuner_status[tuner_id].rf_flow_id << std::endl;
				std::cout << "  Scan Mode Enabled: " << frontend_tuner_status[tuner_id].scan_mode_enabled << std::endl;
				std::cout << "  Supports Scan: " << frontend_tuner_status[tuner_id].supports_scan << std::endl;
				std::cout << "  Valid: " << frontend_tuner_status[tuner_id].valid << std::endl;
				std::cout << std::endl;
			}

			//################################################################################################
			// test RX when allocation stuff isn't working. set's everything up for serviceFunction loop
			/*
			int channel = 0;
			double freq = 800e6;
			double sample_rate = 8e6;
			double oversample_ratio = 8;
			double bandwidth = 8e6;
			double gain = 0.7;
			bool transmit = false;  // RX_DIGITIZER, RX, etc receive default

			std::cout << "Requesting... " << std::endl;
			std::cout << "  Channel:          " << channel << std::endl;
			std::cout << "  Transmit:         " << transmit << std::endl;
			std::cout << "  RF Frequency:     " << freq << std::endl;
			std::cout << "  LPF Bandwidth:    " << bandwidth << std::endl;
			std::cout << "  Normalized Gain:  " << gain << std::endl;
			std::cout << "  RF Sample Rate:   " << sample_rate << std::endl;
			std::cout << "  Oversample Ratio: " << oversample_ratio << std::endl;

			// make request
			allocateLimeSDR_FEI(channel, transmit, freq, sample_rate, oversample_ratio, bandwidth, gain);

			// check actual results
			std::cout << "Got... " << std::endl;
			std::cout << "  Channel:  " << channel << std::endl;
			std::cout << "  Transmit: " << transmit << std::endl;
			getChannelStatus(channel, transmit);

			// initialize stream
			// TODO make some of these things properties?
			streamId.channel = channel;                      // channel number to stream from
			streamId.fifoSize = 1024 * 1024;                 // fifo size in samples
			streamId.throughputVsLatency = 1.0;              // optimize for max throughput
			streamId.isTx = false;                           // RX channel
			streamId.dataFmt = lms_stream_t::LMS_FMT_F32;    // 32-bit floats

			if (LMS_SetupStream(device, &streamId) != 0) { Error(LMS_GetLastErrorMessage()); }

			LMS_StartStream(&streamId);
			// allow serviceFunction to start accepting data
			channel_active = true;
			*/
			//################################################################################################

		}
}

/***********************************************************************************************

    Basic functionality:

        The service function is called by the serviceThread object (of type ProcessThread).
        This call happens immediately after the previous call if the return value for
        the previous call was NORMAL.
        If the return value for the previous call was NOOP, then the serviceThread waits
        an amount of time defined in the serviceThread's constructor.
        
    SRI:
        To create a StreamSRI object, use the following code:
                std::string stream_id = "testStream";
                BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);

        To create a StreamSRI object based on tuner status structure index 'idx' and collector center frequency of 100:
                std::string stream_id = "my_stream_id";
                BULKIO::StreamSRI sri = this->create(stream_id, this->frontend_tuner_status[idx], 100);

    Time:
        To create a PrecisionUTCTime object, use the following code:
                BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();

        
    Ports:

        Data is passed to the serviceFunction through by reading from input streams
        (BulkIO only). The input stream class is a port-specific class, so each port
        implementing the BulkIO interface will have its own type-specific input stream.
        UDP multicast (dataSDDS and dataVITA49) ports do not support streams.

        The input stream from which to read can be rgetChannelPropertiesequested with the getCurrentStream()
        method. The optional argument to getCurrentStream() is a floating point number that
        specifies the time to wait in seconds. A zero value is non-blocking. A negative value
        is blocking.  Constants have been defined for these values, bulkio::Const::BLOCKING and
        bulkio::Const::NON_BLOCKING.

        More advanced uses of input streams are possible; refer to the REDHAWK documentation
        for more details.

        Input streams return data blocks that automatically manage the memory for the data
        and include the SRI that was in effect at the time the data was received. It is not
        necessary to delete the block; it will be cleaned up when it goes out of scope.

        To send data using a BulkIO interface, create an output stream and write the
        data to it. When done with the output stream, the close() method sends and end-of-
        stream flag and cleans up.

        NOTE: If you have a BULKIO dataSDDS or dataVITA49  port, you must manually call 
              "port->updateStats()" to update the port statistics when appropriate.

        Example:
            // This example assumes that the device has two ports:
            //  An input (provides) port of type bulkio::InShortPort called dataShort_in
            //  An output (uses) port of type bulkio::OutFloatPort called dataFloat_out
            // The mapping between the port and the class is found
            // in the device base class header file

            bulkio::InShortStream inputStream = dataShort_in->getCurrentStream();
            if (!inputStream) { // No streams are available
                return NOOP;
            }

            // Get the output stream, creating it if it doesn't exist yet
            bulkio::OutFloatStream outputStream = dataFloat_out->getStream(inputStream.streamID());
            if (!outputStream) {
                outputStream = dataFloat_out->createStream(inputStream.sri());
            }

            bulkio::ShortDataBlock block = inputStream.read();
            if (!block) { // No data available
                // Propagate end-of-stream
                if (inputStream.eos()) {
                   outputStream.close();
                }
                return NOOP;
            }

            if (block.sriChanged()) {
                // Update output SRI
                outputStream.sri(block.sri());
            }

            // Get read-only access to the input data
            redhawk::shared_buffer<short> inputData = block.buffer();

            // Acquire a new buffer to hold the output data
            redhawk::buffer<float> outputData(inputData.size());

            // Transform input data into output data
            for (size_t index = 0; index < inputData.size(); ++index) {
                outputData[index] = (float) inputData[index];
            }

            // Write to the output stream; outputData must not be modified after
            // this method call
            outputStream.write(outputData, block.getStartTime());

            return NORMAL;

        If working with complex data (i.e., the "mode" on the SRI is set to
        true), the data block's complex() method will return true. Data blocks
        provide a cxbuffer() method that returns a complex interpretation of the
        buffer without making a copy:

            if (block.complex()) {
                redhawk::shared_buffer<std::complex<short> > inData = block.cxbuffer();
                redhawk::buffer<std::complex<float> > outData(inData.size());
                for (size_t index = 0; index < inData.size(); ++index) {
                    outData[index] = inData[index];
                }
                outputStream.write(outData, block.getStartTime());
            }

        Interactions with non-BULKIO ports are left up to the device developer's discretion
        
    Messages:
    
        To receive a message, you need (1) an input port of type MessageEvent, (2) a message prototype described
        as a structure property of kind message, (3) a callback to service the message, and (4) to register the callback
        with the input port.
        
        Assuming a property of type message is declared called "my_msg", an input port called "msg_input" is declared of
        type MessageEvent, create the following code:
        
        void LimeSDR_FEI_i::my_message_callback(const std::string& id, const my_msg_struct &msg){
        }
        
        Register the message callback onto the input port with the following form:
        this->msg_input->registerMessage("my_msg", this, &LimeSDR_FEI_i::my_message_callback);
        
        To send a message, you need to (1) create a message structure, (2) a message prototype described
        as a structure property of kind message, and (3) send the message over the port.
        
        Assuming a property of type message is declared called "my_msg", an output port called "msg_output" is declared of
        type MessageEvent, create the following code:
        
        ::my_msg_struct msg_out;
        this->msg_output->sendMessage(msg_out);

    Accessing the Device Manager and Domain Manager:
    
        Both the Device Manager hosting this Device and the Domain Manager hosting
        the Device Manager are available to the Device.
        
        To access the Domain Manager:
            CF::DomainManager_ptr dommgr = this->getDomainManager()->getRef();
        To access the Device Manager:
            CF::DeviceManager_ptr devmgr = this->getDeviceManager()->getRef();
    
    Properties:
        
        Properties are accessed directly as member variables. For example, if the
        property name is "baudRate", it may be accessed within member functions as
        "baudRate". Unnamed properties are given the property id as its name.
        Property types are mapped to the nearest C++ type, (e.g. "string" becomes
        "std::string"). All generated properties are declared in the base class
        (LimeSDR_FEI_base).
    
        Simple sequence properties are mapped to "std::vector" of the simple type.
        Struct properties, if used, are mapped to C++ structs defined in the
        generated file "struct_props.h". Field names are taken from the name in
        the properties file; if no name is given, a generated name of the form
        "field_n" is used, where "n" is the ordinal number of the field.
        
        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A boolean called scaleInput
              
            if (scaleInput) {
                dataOut[i] = dataIn[i] * scaleValue;
            } else {
                dataOut[i] = dataIn[i];
            }
            
        Callback methods can be associated with a property so that the methods are
        called each time the property value changes.  This is done by calling 
        addPropertyListener(<property>, this, &LimeSDR_FEI_i::<callback method>)
        in the constructor.

        The callback method receives two arguments, the old and new values, and
        should return nothing (void). The arguments can be passed by value,
        receiving a copy (preferred for primitive types), or by const reference
        (preferred for strings, structs and vectors).

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A struct property called status
            
        //Add to LimeSDR_FEI.cpp
        LimeSDR_FEI_i::LimeSDR_FEI_i(const char *uuid, const char *label) :
            LimeSDR_FEI_base(uuid, label)
        {
            addPropertyListener(scaleValue, this, &LimeSDR_FEI_i::scaleChanged);
            addPropertyListener(status, this, &LimeSDR_FEI_i::statusChanged);
        }

        void LimeSDR_FEI_i::scaleChanged(float oldValue, float newValue)
        {
            RH_DEBUG(this->_baseLog, "scaleValue changed from" << oldValue << " to " << newValue);
        }
            
        void LimeSDR_FEI_i::statusChanged(const status_struct& oldValue, const status_struct& newValue)
        {
            RH_DEBUG(this->_baseLog, "status changed");
        }
            
        //Add to LimeSDR_FEI.h
        void scaleChanged(float oldValue, float newValue);
        void statusChanged(const status_struct& oldValue, const status_struct& newValue);

    Logging:

        The member _baseLog is a logger whose base name is the component (or device) instance name.
        New logs should be created based on this logger name.

        To create a new logger,
            rh_logger::LoggerPtr my_logger = this->_baseLog->getChildLogger("foo");

        Assuming component instance name abc_1, my_logger will then be created with the 
        name "abc_1.user.foo".

    Allocation:
    
        Allocation callbacks are available to customize the Device's response to 
        allocation requests. For example, if the Device contains the allocation 
        property "my_alloc" of type string, the allocation and deallocation
        callbacks follow the pattern (with arbitrary function names
        my_alloc_fn and my_dealloc_fn):
        
        bool LimeSDR_FEI_i::my_alloc_fn(const std::string &value)
        {
            // perform logic
            return true; // successful allocation
        }
        void LimeSDR_FEI_i::my_dealloc_fn(const std::string &value)
        {
            // perform logic
        }
        
        The allocation and deallocation functions are then registered with the Device
        base class with the setAllocationImpl call. Note that the variable for the property is used rather
        than its id:
        
        this->setAllocationImpl(my_alloc, this, &LimeSDR_FEI_i::my_alloc_fn, &LimeSDR_FEI_i::my_dealloc_fn);
        
        

************************************************************************************************/
int LimeSDR_FEI_i::serviceFunction()
{
    RH_DEBUG(this->_baseLog, "serviceFunction() example log message");
    
    return NOOP;
}

/*
 * Non-autogenerated Redhawk functions #######################################################
 * I added these helper functions for the LimeSDR_FEI
 * ###########################################################################################
 */

std::string testSigEnumToString(int code) {
	switch(code) {
		case 0: return "NONE";
		case 1: return "NCODIV8";
		case 2: return "NCODIV4";
		case 3: return "NCODIV8F";
		case 4: return "NCODIV4F";
		case 5: return "DC";
		default: return "UNKNOWN";
	}
}

// helper function for calibration?
void configureLimeSDR_FEI() {}

// helper function for getting properties that are static per device, regardless of channel type/number
void getDeviceProperties() {}

// helper function for getting static properties/ranges and status per channel type/number
void LimeSDR_FEI_i::getChannelProperties(int channel, bool transmit) {
    LOG_DEBUG(LimeSDR_FEI_i, "--> getChannelProperties()");

	channel_struct availChan;
	availChan.ch_name = ""; // TODO what should this be?
	if (transmit) { availChan.tuner_type = "TX"; }
	else { availChan.tuner_type = "RX_DIGITIZER"; }
	availChan.chan_num = channel;	// TODO this will result in 2 channel 0's and 2 channel 1's for the RX/TX difference

	lms_range_t range;
	if (LMS_GetLOFrequencyRange(device, transmit, &range) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.freq_max = range.max;
	availChan.freq_min = range.min;
	//channels.freq_step = range->step; //TODO not part of redhawk data structure
	std::cout << "  RF Frequency min:    " << range.min << std::endl;
	std::cout << "  RF Frequency max:    " << range.max << std::endl;
	std::cout << "  RF Frequency step:   " << range.step << std::endl;

	double freq;
	if (LMS_GetLOFrequency(device, transmit, channel, &freq) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.freq_current = freq;
	std::cout << "  RF Frequency current: " << freq << std::endl;

	if (LMS_GetSampleRateRange(device, transmit, &range) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.rate_max = range.max;
	availChan.rate_min = range.min;
	//availChan.rate_step = range->step; //TODO not part of redhawk data structure
	std::cout << "  RF Sample rate min:  " << range.min << std::endl;
	std::cout << "  RF Sample rate max:  " << range.max << std::endl;
	std::cout << "  RF Sample rate step: " << range.step << std::endl;

	double host_sample_rate;
	double rf_sample_rate;
	if (LMS_GetSampleRate(device, transmit, channel, &host_sample_rate, &rf_sample_rate) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.rate_current = rf_sample_rate;
	std::cout << "  RF Sample Rate current: " << rf_sample_rate << std::endl;

	if (LMS_GetLPFBWRange(device, transmit, &range) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.bandwidth_max = range.max;
	availChan.bandwidth_min = range.min;
	//availChan.rate_step = range->step; //TODO not part of redhawk data structure
	std::cout << "  LPF Bandwidth min:   " << range.min << std::endl;
	std::cout << "  LPF Bandwidth max:   " << range.max << std::endl;
	std::cout << "  LPF Bandwidth step:  " << range.step << std::endl;

	double bandwidth;
	if (LMS_GetLPFBW(device, transmit, channel, &bandwidth) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.bandwidth_current = bandwidth;
	std::cout << "  LPF Bandwidth current: " << bandwidth << std::endl;

	availChan.gain_max = 1.0;
	availChan.gain_min = 0.0;
	std::cout << "  Normalized Gain min:  " << 0.1 << std::endl;
	std::cout << "  Normalized Gain max:  " << 1.0 << std::endl;
	unsigned int normal_gain;
	if (LMS_GetGaindB(device, transmit, channel, &normal_gain) != 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.gain_current = normal_gain;
	std::cout << "  Normalized Gain current:  " << normal_gain << std::endl;

	// get antenna names? and bandwidths
	int num_antennas;
	lms_name_t antenna_list[10];
	// TODO why is there a channel arg for LMS_GetAntennaList? They always match...
	if ((num_antennas = LMS_GetAntennaList(device, transmit, channel, antenna_list)) < 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  Number of antennas on channel " << channel << ": " << num_antennas << std::endl;

	for (int i = 0; i < num_antennas; i++) {
		std::cout << "  Antenna " << i << ": " << antenna_list[i] << std::endl;
		availChan.available_antennas.push_back(antenna_list[i]);
		// TODO why is there a channel arg for LMS_GetAntennaBW? They always match...
		// TODO Redhawk has no antenna bw fields in channel struct
		if (LMS_GetAntennaBW(device, transmit, channel, i, &range) != 0) { Error(LMS_GetLastErrorMessage()); }
		std::cout << "    BW min: " << range.min << std::endl;
		std::cout << "    BW max: " << range.max << std::endl;
		std::cout << "    BW step: " << range.step << std::endl;
	}

	int n;
	if ((n = LMS_GetAntenna(device, transmit, channel)) < 0) { Error(LMS_GetLastErrorMessage()); }
	availChan.antenna = antenna_list[n];
	std::cout << "  Antenna current:  " << antenna_list[n] << std::endl;

	// TODO dunno what the Redhawk 'clock' property is for but there's not equivalent in LimeSDR_FEI
	availChan.clock_max = 0.0;
	availChan.clock_min = 0.0;

	// save device_channel properties
	device_channels.push_back(availChan);

	// create Redhawk frontend_tuner_status struct that corresponds to this channel type/number
	std::cout << "  Creating Redhawk frontend_tuner_status struct..." << std::endl;
	std::cout << std::endl;

	// add one tuner to tuner_allocation_ids list
	tuner_allocation_ids.resize(tuner_allocation_ids.size()+1);

	// set initial frontend_tuner_status_struct properties
	frontend_tuner_status_struct_struct channel_status;
	channel_status.tuner_type = availChan.tuner_type;
	channel_status.tuner_number = availChan.chan_num;
	channel_status.enabled = false;
	channel_status.center_frequency = availChan.freq_current;
	channel_status.available_frequency = boost::lexical_cast<std::string>(availChan.freq_min) + "-" + boost::lexical_cast<std::string>(availChan.freq_max);  // available frequencies (Hz) in range (XX-YY) or csv (X,Y,Z) format
	channel_status.bandwidth = availChan.bandwidth_current;
	channel_status.available_bandwidth = boost::lexical_cast<std::string>(availChan.bandwidth_min) + "-" + boost::lexical_cast<std::string>(availChan.bandwidth_max); // available bandwidth (Hz) in range (XX-YY) or csv (X,Y,Z) format
	// TODO should gain be normalized or not?
	channel_status.gain = availChan.gain_current;
	channel_status.available_gain = boost::lexical_cast<std::string>(availChan.gain_min) + "-" + boost::lexical_cast<std::string>(availChan.gain_max); // available gain (dB) in range (XX-YY) or csv (X,Y,Z) format
	channel_status.sample_rate = availChan.rate_current;
	channel_status.available_sample_rate = boost::lexical_cast<std::string>(availChan.rate_min) + "-" + boost::lexical_cast<std::string>(availChan.rate_max); // available sample_rate (sps) in range (XX-YY) or csv (X,Y,Z) format

	// TODO where do these properties come from? what should they be?
	channel_status.agc = false;
	channel_status.allocation_id_csv = "";
	channel_status.bandwidth_tolerance = 20;
	channel_status.complex = true;
	channel_status.decimation = 0;
	channel_status.group_id = "";
	channel_status.reference_source = 0;
	channel_status.rf_flow_id = "";
	channel_status.sample_rate_tolerance = 20;
	channel_status.scan_mode_enabled = false;
	channel_status.supports_scan = false;
	channel_status.valid = true;

	frontend_tuner_status.push_back(channel_status);
    LOG_DEBUG(LimeSDR_FEI_i, "<-- getChannelProperties()");
}

// helper function for getting current channel status, things that change like center frequency, bandwidth, etc.
void LimeSDR_FEI_i::getChannelStatus(int channel, bool transmit) {
    LOG_DEBUG(LimeSDR_FEI_i, "--> getChannelStatus()");

	double freq;
    if (LMS_GetLOFrequency(device, transmit, channel, &freq) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  RF Frequency:     " << freq << std::endl;

	double bandwidth;
    if (LMS_GetLPFBW(device, transmit, channel, &bandwidth) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  LPF Bandwidth:    " << bandwidth << std::endl;

	unsigned int gain;
    if (LMS_GetGaindB(device, transmit, channel, &gain) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  Gain:             " << gain << std::endl;

	unsigned int normal_gain;
    if (LMS_GetGaindB(device, transmit, channel, &normal_gain) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  Normalized Gain:  " << normal_gain << std::endl;

	double rf_sample_rate;
	double host_sample_rate;
    if (LMS_GetSampleRate(device, transmit, channel, &host_sample_rate, &rf_sample_rate) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  Host Sample Rate: " << round(host_sample_rate) << std::endl;
	std::cout << "  RF Sample Rate:   " << round(rf_sample_rate) << std::endl;

	lms_testsig_t test_sig;
    if (LMS_GetTestSignal(device, transmit, channel, &test_sig) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  Test Signal:      " << testSigEnumToString(test_sig) << std::endl;

	// get currently selected antenna index/type
	int n;
	lms_name_t antenna_list[10];
    if ((n = LMS_GetAntennaList(device, transmit, channel, antenna_list)) < 0) { Error(LMS_GetLastErrorMessage()); }
	if ((n = LMS_GetAntenna(device, transmit, channel)) < 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  Antenna " << n << ":        " << antenna_list[n] << std::endl;

	// update Redhawk frontend_tuner_status struct that corresponds to this channel type/number
	std::cout << "  Updating frontend_tuner_status with channel " << channel << std::endl;
	std::cout << std::endl;
	int num_updated = 0;
	for(size_t tuner_id = 0; tuner_id < frontend_tuner_status.size(); tuner_id++) {
		if (frontend_tuner_status[tuner_id].tuner_number == channel) {
			if (frontend_tuner_status[tuner_id].tuner_type == "RX_DIGITIZER" && not transmit) {
				// update things that we actually get from LimeSDR_FEI "Get" functions
				frontend_tuner_status[tuner_id].center_frequency = freq;
				frontend_tuner_status[tuner_id].bandwidth = bandwidth;
				frontend_tuner_status[tuner_id].gain = normal_gain;
				// TODO because of the 2x oversample thing, use host rate which reflects the SetSampleRate requests.... need to figure out what is real
				// TODO ..also, the returned sample rate always seems to be a slight fraction less than what was set... so I'm rounding for now
				frontend_tuner_status[tuner_id].sample_rate = round(host_sample_rate);
				// TODO need to track antennas?
				num_updated++;

			} else if (frontend_tuner_status[tuner_id].tuner_type == "TX" && transmit) {
				// update things that we actually get from LimeSDR_FEI "Get" functions
				frontend_tuner_status[tuner_id].center_frequency = freq;
				frontend_tuner_status[tuner_id].bandwidth = bandwidth;
				frontend_tuner_status[tuner_id].gain = normal_gain;
				// TODO because of the 2x oversample thing, use host rate which reflects the SetSampleRate requests.... need to figure out what is real
				frontend_tuner_status[tuner_id].sample_rate = round(host_sample_rate);
				// TODO need to track antennas?
				num_updated++;
			}
		}
	}

	if (num_updated == 0) { LOG_ERROR(LimeSDR_FEI_i, "No channels matched request or were available."); }
	else if (num_updated > 1) { LOG_ERROR(LimeSDR_FEI_i, "More than 1 channel matched request and was updated."); }

    LOG_DEBUG(LimeSDR_FEI_i, "<-- getChannelStatus()");
}

void LimeSDR_FEI_i::getAdvancedControlStatus(int channel, bool transmit) {

/* TuneVCO(SXR) - VCO too high
	lms_gfir_t filt;
	double coef[120];	// GFIR1 and GFIR2 need 40 values, GFIR3 needs 120 values
	filt = LMS_GFIR1;
	if (LMS_GetGFIRCoeff(device, transmit, channel, filt, coef) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "GFIR coefficients: " << std::endl;
	std::cout << "	";
	for (int i = 0; i < 40; i++) {
		std::cout << " " << coef[i];
	}
*/

	double freq[LMS_NCO_VAL_COUNT];
	double pho;
	if (LMS_GetNCOFrequency(device, transmit, channel, freq, &pho) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  NCO FCW phase offset: " << pho << std::endl;
	std::cout << "  NCO FCW frequencies:  " << std::endl;
	for (int i = 0; i < LMS_NCO_VAL_COUNT; i++) {
		std::cout << "    " << freq[i] << std::endl;
	}

/* NCO disabled error
	int index;
	if ((index = LMS_GetNCOIndex(device, transmit, channel)) < 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "NCO offset index: " << index << std::endl;
*/

	double phases[LMS_NCO_VAL_COUNT];
	double fcw;
	if (LMS_GetNCOPhase(device, transmit, channel, phases, &fcw) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "  NCO PHO frequency:    " << fcw << std::endl;
	std::cout << "  NCO PHO phases:       " << std::endl;
	for (int i = 0; i < LMS_NCO_VAL_COUNT; i++) {
		std::cout << "    " << phases[i] << std::endl;
	}
}

void LimeSDR_FEI_i::allocateLimeSDR_FEI(int channel, bool transmit, double freq, double sample_rate, int oversample_ratio, double bandwidth, double gain) {
    LOG_DEBUG(LimeSDR_FEI_i, "--> allocateLimeSDR_FEI()");
    // TODO difference between EnableChannel and SetupStream/StartStream.....?
	if (LMS_EnableChannel(device, transmit, channel, true) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "LMS_EnableChannel" << std::endl;
    if (LMS_SetLOFrequency(device, transmit, channel, freq) != 0) { Error(LMS_GetLastErrorMessage()); }
	std::cout << "LMS_SetLOFrequency" << std::endl;
    if (LMS_SetSampleRate(device, sample_rate, oversample_ratio) != 0) { Error(LMS_GetLastErrorMessage()); } // preferred oversampling in RF, this set sampling rate for all channels
	std::cout << "LMS_SetSampleRate" << std::endl;
	if (LMS_SetLPFBW(device, transmit, channel, bandwidth) != 0) { Error(LMS_GetLastErrorMessage()); }      // configure LPF
	std::cout << "LMS_SetLPFBW" << std::endl;
	if (LMS_SetGaindB(device, transmit, channel, gain) != 0) { Error(LMS_GetLastErrorMessage()); }  // set RX gain
	// TODO what does LMS_SetGFIRLPF do?
	std::cout << "LMS_SetGaindB" << std::endl;
	if (LMS_Calibrate(device, transmit, channel, bandwidth, 0) != 0) { Error(LMS_GetLastErrorMessage()); } 	// perform automatic calibration (last arg are flags)
    LOG_DEBUG(LimeSDR_FEI_i, "<-- allocateLimeSDR_FEI()");
}


/*************************************************************
Functions supporting tuning allocation
*************************************************************/
void LimeSDR_FEI_i::deviceEnable(frontend_tuner_status_struct_struct &fts, size_t tuner_id){
    /************************************************************
    modify fts, which corresponds to this->frontend_tuner_status[tuner_id]
    Make sure to set the 'enabled' member of fts to indicate that tuner as enabled
    ************************************************************/
	LOG_DEBUG(LimeSDR_FEI_i, "--> deviceEnable()");

		// tell device to start streaming data
		// TODO if that's what this does, then why is the timestamp/waitForTimestamp fields not part of SetupStream for RX?
		LMS_StartStream(&streamId);

		// update frontend_tuner_status struct
		fts.enabled = true;

		// allow serviceFunction to start accepting data
		channel_active = true;

	    LOG_DEBUG(LimeSDR_FEI_i, "<-- deviceEnable()");
	    return;
}
void LimeSDR_FEI_i::deviceDisable(frontend_tuner_status_struct_struct &fts, size_t tuner_id){
    /************************************************************
    modify fts, which corresponds to this->frontend_tuner_status[tuner_id]
    Make sure to reset the 'enabled' member of fts to indicate that tuner as disabled
    ************************************************************/
	LOG_DEBUG(LimeSDR_FEI_i, "--> deviceDisable()");

		LMS_StopStream(&streamId);   // stream is stopped but can be started again with LMS_StartStream()

		// update frontend_tuner_status struct
		fts.enabled = false;

		// stop serviceFunction from looking for data
		channel_active = false;

	    LOG_DEBUG(LimeSDR_FEI_i, "<-- deviceDisable()");
	    return;
}

bool LimeSDR_FEI_i::deviceSetTuning(const frontend::frontend_tuner_allocation_struct &request, frontend_tuner_status_struct_struct &fts, size_t tuner_id){
    /************************************************************
    modify fts, which corresponds to this->frontend_tuner_status[tuner_id]
      At a minimum, bandwidth, center frequency, and sample_rate have to be set
      If the device is tuned to exactly what the request was, the code should be:
        fts.bandwidth = request.bandwidth;
        fts.center_frequency = request.center_frequency;
        fts.sample_rate = request.sample_rate;

    return true if the tuning succeeded, and false if it failed
    ************************************************************/
    LOG_DEBUG(LimeSDR_FEI_i, "--> deviceSetTuning()");
	// setup RX stream
	std::cout << "#### Allocating Channel #####################" << std::endl;
	std::cout << "Requesting... " << std::endl;
	std::cout << "  Allocation ID:         " << request.allocation_id << std::endl;
	std::cout << "  Device Control:        " << request.device_control << std::endl;
	std::cout << "  Group ID:              " << request.group_id << std::endl;
	std::cout << "  RF Flow ID:            " << request.rf_flow_id << std::endl;
	std::cout << "  Tuner Type:            " << request.tuner_type << std::endl;
	std::cout << "  Center Frequency:      " << request.center_frequency << std::endl;
	std::cout << "  Bandwidth:             " << request.bandwidth << std::endl;
	std::cout << "  Bandwidth Tolerance:   " << request.bandwidth_tolerance << std::endl;
	std::cout << "  Sample Rate:           " << request.sample_rate << std::endl;
	std::cout << "  Sample Rate Tolerance: " << request.sample_rate_tolerance << std::endl;

	double gain = 0.7;        // TODO there is no gain value in the tuner allocation struct
	// TODO oversample ratio sets the actual RF sample rate, aka the SetSampleRate() function uses sample_rate to set the Host Sample Rate, and the oversample_ratio multiplied by that to set RF sample rate...
	//      If you use oversample ratio other than 1, then the frontend_tuner_status will reflect the RF sample rate and the request sample rate is treated as the Host sample rate - a mismatch that will likely result in failed allocation
	// TODO oversample ratio of 1 seems like it still results in RF sample rate of 2x host sample rate
	// TODO there is no oversample value in the tuner allocation struct
	std::cout << "  Normalized Gain:       " << gain << std::endl;
	std::cout << "  Oversample Ratio:      " << oversample_ratio << std::endl;

	// try to allocate to the channel specified in fts/tuner_id argument
	int channel = fts.tuner_number;
	bool transmit = false;  // RX_DIGITIZER, RX, etc receive default
	if (fts.tuner_type == "TX") { transmit = true; }

	// make request
	allocateLimeSDR_FEI(channel, transmit, request.center_frequency, request.sample_rate, oversample_ratio, request.bandwidth, gain);

	// check actual results
	std::cout << "Got... " << std::endl;
	std::cout << "  Channel: " << channel << std::endl;
	std::cout << "  Transmit: " << transmit << std::endl;
	// TODO this function needs to set sample rate as the request if within tolerance....
	getChannelStatus(channel, transmit);

	// initialize stream
	// TODO make some of these things properties?
	streamId.channel = channel;                      // channel number to stream from
	streamId.fifoSize = 1024 * 1024;                 // fifo size in samples
	streamId.throughputVsLatency = 1.0;              // optimize for max throughput
	streamId.isTx = false;                           // RX channel
	streamId.dataFmt = lms_stream_t::LMS_FMT_F32;    // 32-bit floats

	if (LMS_SetupStream(device, &streamId) != 0) { Error(LMS_GetLastErrorMessage()); }

	// TODO do I need to set anything in 'fts' since getChannelStatus finds and updates the structures anyway?
	//fts.bandwidth = request.bandwidth;
	//fts.center_frequency = request.center_frequency;
	//fts.sample_rate = request.sample_rate;

    LOG_DEBUG(LimeSDR_FEI_i, "<-- deviceSetTuning()");
    return true;
}
bool LimeSDR_FEI_i::deviceDeleteTuning(frontend_tuner_status_struct_struct &fts, size_t tuner_id) {
    /************************************************************
    modify fts, which corresponds to this->frontend_tuner_status[tuner_id]
    return true if the tune deletion succeeded, and false if it failed
    ************************************************************/
	LOG_DEBUG(LimeSDR_FEI_i, "--> deviceDeleteTuning()");

		LMS_DestroyStream(device, &streamId);  // stream is deallocated and can no longer be used

	    LOG_DEBUG(LimeSDR_FEI_i, "<-- deviceDeleteTuning()");
	    return true;
}

/*************************************************************
Functions servicing the tuner control port
*************************************************************/
std::string LimeSDR_FEI_i::getTunerType(const std::string& allocation_id) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].tuner_type;
}

bool LimeSDR_FEI_i::getTunerDeviceControl(const std::string& allocation_id) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    if (getControlAllocationId(idx) == allocation_id)
        return true;
    return false;
}

std::string LimeSDR_FEI_i::getTunerGroupId(const std::string& allocation_id) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].group_id;
}

std::string LimeSDR_FEI_i::getTunerRfFlowId(const std::string& allocation_id) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].rf_flow_id;
}

void LimeSDR_FEI_i::setTunerCenterFrequency(const std::string& allocation_id, double freq) {
    /*long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    if(allocation_id != getControlAllocationId(idx))
        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
    if (freq<0) throw FRONTEND::BadParameterException("Center frequency cannot be less than 0");
    // set hardware to new value. Raise an exception if it's not possible
    this->frontend_tuner_status[idx].center_frequency = freq;
    */
	LOG_DEBUG(LimeSDR_FEI_i, "setTunerCenterFrequency");
	    long idx = getTunerMapping(allocation_id);
	    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
	    if(allocation_id != getControlAllocationId(idx))
	        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
	    if (freq<0) throw FRONTEND::BadParameterException("Center frequency cannot be less than 0");

		double* actual_freq;
	    if (frontend_tuner_status[idx].tuner_type == "RX_DIGITIZER") {
	        //scoped_tuner_lock tuner_lock(usrp_tuners[idx].lock);
	    	//TODO do anything before changing freq? stop something, lock props, set SRI?
	    	if (LMS_SetLOFrequency(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, freq) != 0) { Error(LMS_GetLastErrorMessage()); }
	    	if (LMS_GetLOFrequency(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, actual_freq) != 0) { Error(LMS_GetLastErrorMessage()); }

	    } else if (frontend_tuner_status[idx].tuner_type == "TX") {
	    	if (LMS_SetLOFrequency(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, freq) != 0) { Error(LMS_GetLastErrorMessage()); }
			if (LMS_GetLOFrequency(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, actual_freq) != 0) {	Error(LMS_GetLastErrorMessage()); }

	    } else { throw FRONTEND::FrontendException(("Unexpected tuner type found: "+frontend_tuner_status[idx].tuner_type).c_str()); }
		this->frontend_tuner_status[idx].center_frequency = *actual_freq;
		//exclusive_lock lock(prop_lock);
}

double LimeSDR_FEI_i::getTunerCenterFrequency(const std::string& allocation_id) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].center_frequency;
}

void LimeSDR_FEI_i::setTunerBandwidth(const std::string& allocation_id, double bw) {
    /*long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    if(allocation_id != getControlAllocationId(idx))
        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
    if (bw<0) throw FRONTEND::BadParameterException("Bandwidth cannot be less than 0");
    // set hardware to new value. Raise an exception if it's not possible
    this->frontend_tuner_status[idx].bandwidth = bw;
    */
	LOG_DEBUG(LimeSDR_FEI_i, "setTunerBandwidth");
	    long idx = getTunerMapping(allocation_id);
	    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
	    if(allocation_id != getControlAllocationId(idx))
	        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
	    if (bw<0) throw FRONTEND::BadParameterException("Bandwidth cannot be less than 0");

	    // set hardware to new value. Raise an exception if it's not possible
		double* actual_bw;
	    if (frontend_tuner_status[idx].tuner_type == "RX_DIGITIZER") {
	        if (LMS_SetLPFBW(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, bw) != 0) { Error(LMS_GetLastErrorMessage()); }
			if (LMS_GetLPFBW(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, actual_bw) != 0) {	Error(LMS_GetLastErrorMessage()); }

	    } else if (frontend_tuner_status[idx].tuner_type == "TX") {
	        if (LMS_SetLPFBW(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, bw) != 0) { Error(LMS_GetLastErrorMessage()); }
			if (LMS_GetLPFBW(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, actual_bw) != 0) {	Error(LMS_GetLastErrorMessage()); }

	    } else { throw FRONTEND::FrontendException(("Unexpected tuner type found: "+frontend_tuner_status[idx].tuner_type).c_str()); }
		this->frontend_tuner_status[idx].bandwidth = *actual_bw;
}

//TODO should I worry about checking real hardware value here?
double LimeSDR_FEI_i::getTunerBandwidth(const std::string& allocation_id) {
	LOG_DEBUG(LimeSDR_FEI_i, "getTunerBandwidth");
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].bandwidth;
}

//NOTE There are 2 gain functions, GainDB and NormalizedGain, there is no AGC function...
void LimeSDR_FEI_i::setTunerAgcEnable(const std::string& allocation_id, bool enable)
{
	LOG_DEBUG(LimeSDR_FEI_i, "setTunerAgcEnable");
    throw FRONTEND::NotSupportedException("LimeSDR_FEI does not support AGC, use setGainDB or setNormalizedGain");
}

bool LimeSDR_FEI_i::getTunerAgcEnable(const std::string& allocation_id)
{
	LOG_DEBUG(LimeSDR_FEI_i, "getTunerAgcEnable");
    throw FRONTEND::NotSupportedException("LimeSDR_FEI does not support AGC, use setGainDB or setNormalizedGain");
}

//NOTE There are 2 gain functions, GainDB and NormalizedGain, not sure which is preferred...
void LimeSDR_FEI_i::setTunerGain(const std::string& allocation_id, float gain)
{
	LOG_DEBUG(LimeSDR_FEI_i, "setTunerGain");
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    if(allocation_id != getControlAllocationId(idx))
        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
    if (gain<0) throw FRONTEND::BadParameterException("Gain cannot be less than 0");

	// set hardware to new value. Raise an exception if it's not possible
	unsigned int* actual_gain;
	if (frontend_tuner_status[idx].tuner_type == "RX_DIGITIZER") {
	    if (gain>70) throw FRONTEND::BadParameterException("RX gain cannot be greater than 70");
		if (LMS_SetGaindB(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, gain) != 0) { Error(LMS_GetLastErrorMessage()); }
		if (LMS_GetGaindB(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, actual_gain) != 0) {	Error(LMS_GetLastErrorMessage()); }

	} else if (frontend_tuner_status[idx].tuner_type == "TX") {
	    if (gain>60) throw FRONTEND::BadParameterException("TX gain cannot be greater than 60");
		if (LMS_SetGaindB(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, gain) != 0) { Error(LMS_GetLastErrorMessage()); }
		if (LMS_GetGaindB(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, actual_gain) != 0) {	Error(LMS_GetLastErrorMessage()); }

	} else { throw FRONTEND::FrontendException(("Unexpected tuner type found: "+frontend_tuner_status[idx].tuner_type).c_str()); }
	this->frontend_tuner_status[idx].gain = *actual_gain;
}

// used like:  sdr.getPort('DigitalTuner_in').getTunerGain('')
float LimeSDR_FEI_i::getTunerGain(const std::string& allocation_id)
{
	LOG_DEBUG(LimeSDR_FEI_i, "getTunerGain");
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].gain;
}

//TODO what is TunerReferenceSource?
void LimeSDR_FEI_i::setTunerReferenceSource(const std::string& allocation_id, long source)
{
	LOG_DEBUG(LimeSDR_FEI_i, "setTunerReferenceSource");
    throw FRONTEND::NotSupportedException("setTunerReferenceSource not supported");
}

long LimeSDR_FEI_i::getTunerReferenceSource(const std::string& allocation_id)
{
    throw FRONTEND::NotSupportedException("getTunerReferenceSource not supported");
}

// TODO is this equivalent to LMS_EnableChannel?
void LimeSDR_FEI_i::setTunerEnable(const std::string& allocation_id, bool enable) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    if(allocation_id != getControlAllocationId(idx))
        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
    // set hardware to new value. Raise an exception if it's not possible
    this->frontend_tuner_status[idx].enabled = enable;
}

// TODO is this equivalent to LMS_EnableChannel?
bool LimeSDR_FEI_i::getTunerEnable(const std::string& allocation_id) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].enabled;
}

//NOTE You can't independently set sample rate per channel, it sets for all of them, RX and TX..
//TODO need to handle override properly, or just accept that a subsequent request will override previous ones...
//TODO why is it called Tuner*Output*SampleRate? Is there supposed to be a different way to set/get TX side vs. RX side?
//TODO what should each sample rate be based on request?
void LimeSDR_FEI_i::setTunerOutputSampleRate(const std::string& allocation_id, double sr) {
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    if(allocation_id != getControlAllocationId(idx))
        throw FRONTEND::FrontendException(("ID "+allocation_id+" does not have authorization to modify the tuner").c_str());
    if (sr<0) throw FRONTEND::BadParameterException("Sample rate cannot be less than 0");
    // set hardware to new value. Raise an exception if it's not possible
	if (LMS_SetSampleRate(device, sr, sr) != 0) { Error(LMS_GetLastErrorMessage()); }
	double host_sample_rate;
	double rf_sample_rate;
    if (frontend_tuner_status[idx].tuner_type == "RX_DIGITIZER") {
    	if (LMS_GetSampleRate(device, LMS_CH_RX, frontend_tuner_status[idx].tuner_number, &host_sample_rate, &rf_sample_rate) != 0) { Error(LMS_GetLastErrorMessage()); }

    } else if (frontend_tuner_status[idx].tuner_type == "TX") {
    	if (LMS_GetSampleRate(device, LMS_CH_TX, frontend_tuner_status[idx].tuner_number, &host_sample_rate, &rf_sample_rate) != 0) { Error(LMS_GetLastErrorMessage()); }

    } else { throw FRONTEND::FrontendException(("Unexpected tuner type found: "+frontend_tuner_status[idx].tuner_type).c_str()); }
    this->frontend_tuner_status[idx].sample_rate = host_sample_rate;
}

double LimeSDR_FEI_i::getTunerOutputSampleRate(const std::string& allocation_id){
    long idx = getTunerMapping(allocation_id);
    if (idx < 0) throw FRONTEND::FrontendException("Invalid allocation id");
    return frontend_tuner_status[idx].sample_rate;
}

/*************************************************************
Functions servicing the RFInfo port(s)
- port_name is the port over which the call was received
*************************************************************/
std::string LimeSDR_FEI_i::get_rf_flow_id(const std::string& port_name)
{
    return std::string("none");
}

void LimeSDR_FEI_i::set_rf_flow_id(const std::string& port_name, const std::string& id)
{
}

frontend::RFInfoPkt LimeSDR_FEI_i::get_rfinfo_pkt(const std::string& port_name)
{
    frontend::RFInfoPkt pkt;
    return pkt;
}

void LimeSDR_FEI_i::set_rfinfo_pkt(const std::string& port_name, const frontend::RFInfoPkt &pkt)
{
}

