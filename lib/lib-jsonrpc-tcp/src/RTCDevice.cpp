#include "RTCDevice.hpp"
using namespace std;
RTCDevice::RTCDevice(std::string devnode) : I2CBusAccess(devnode) {}
RTCDevice::~RTCDevice() {}
