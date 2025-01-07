#include "DisplayDevice.hpp"
using namespace std;
DisplayDevice::DisplayDevice(std::string devnode) : I2CBusAccess(devnode) {}
DisplayDevice::~DisplayDevice() {}
