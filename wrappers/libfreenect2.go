package libfreenect2

// #include ../include/libfreenect2/libfreenect2.hpp"
// #include ../include/libfreenect2/libfreenect2.hpp"
// #include ../include/libfreenect2/frame_listener_impl.h"
// #include ../include/libfreenect2/registration.h"
// #include ../include/libfreenect2/packet_pipeline.h"
// #include ../include/libfreenect2/logger.h"
import (
	"C"
	"unsafe"
)

func EnumerateDevices(freenect2 libfreenect2::Freenect2) int {
	return freenect2.enumerateDevices()
}

func GetDefaultDeviceSerialNumber(freenect2 libfreenect2::Freenect2) string {
	return freenect2.getDefaultDeviceSerialNumber()
}

func CpuPacketPipeline() *libfreenect2::PacketPipeline {
	return libfreenect2::CpuPacketPipeline()
}

func OpenGLPacketPipeline() *libfreenect2::PacketPipeline {
	return libfreenect2::OpenGLPacketPipeline()
}


func OpenCLPacketPipeline() *libfreenect2::PacketPipeline {
	return libfreenect2::OpenCLPacketPipeline()
}

func OpenDevicePipeline(freenect2 libfreenect2::Freenect2, serial string, pipeline libfreenect2::PacketPipeline) *libfreenect2::Freenect2Device {
	return freenect2.OpenDevice(serial, pipeline)
}

func OpenDevice(freenect2 libfreenect2::Freenect2, serial string) *libfreenect2::Freenect2Device {
	return freenect2.OpenDevice(serial)
}

func DeviceSetColorFrameListener(dev *libfreenect2::Freenect2Device, listener libfreenect2::SyncMultiFrameListener) {
	dev->setColorFrameListener(listener)
}

func DeviceSetIrAndDepthFrameListener(dev *libfreenect2::Freenect2Device, listener libfreenect2::SyncMultiFrameListener) {
	dev->setIrAndDepthFrameListener(listener)
}

func DeviceGetSerialNumber(dev *libfreenect2::Freenect2Device) string {
	return dev->getSerialNumber()
}

func DeviceGetFirmwareVersion(dev *libfreenect2::Freenect2Device) string {
	return dev->getFirmwareVersion()
}

func DeviceGetIrCameraParams(dev *libfreenect2::Freenect2Device) {
	return dev->getIrCameraParams()
}

func DeviceGetColorCameraParams(dev *libfreenect2::Freenect2Device) {
	return dev->getColorCameraParams()
}

func DeviceStart(dev *libfreenect2::Freenect2Device) {
	dev->start()
}

func DeviceStop(dev *libfreenect2::Freenect2Device) {
	dev->stop()
}

func DeviceClose(dev *libfreenect2::Freenect2Device) {
	dev->close()
}

func Listener(options) libfreenect2::SyncMultiFrameListener {
	return libfreenect2::SyncMultiFrameListener(options)
}

func ListenerWaitForNewFrame(listener libfreenect2::SyncMultiFrameListener, frames libfreenect2::FrameMap) {
	listener.waitForNewFrame(frames)
}

func ListenerRelease(listener libfreenect2::SyncMultiFrameListener, frames libfreenect2::FrameMap) {
	listener.release(frames)
}

func Registration(irCameraParams, colorCameraParams) *libfreenect2::Registration {
	return libfreenect2::Registration(irCameraParams, colorCameraParams)
}

func RegistrationApply(registration *libfreenect2::Registration, rgb *libfreenect2::Frame, depth *libfreenect2::Frame, undistorted *libfreenect2::Frame, registered *libfreenect2::Frame) {
	return registration->apply(rgb, depth, undistorted, registered)
}

