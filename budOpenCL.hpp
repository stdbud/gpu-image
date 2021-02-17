#pragma once

#include <vector>
#include <array>
#include <iostream>
#include <CL/cl.h>
#include "budUtils.hpp"
#include "budImage.hpp"

namespace bud {

namespace cl {

class ImageCL final : public Imagef {
public:
	explicit ImageCL(const int width, const int height, const int nrChannels)
		: Imagef(width, height, nrChannels),
		  m_device(nullptr),
	      m_context(nullptr),
	      m_commandQueue(nullptr),
	      m_program(nullptr),
	      m_kernel(nullptr),
	      m_srcImage(nullptr),
	      m_dstImage(nullptr) {}

	void compute() override
	{
		createContext();
		createCommandQueue();
		createProgramAndKernel();
		createImages();
		dispatch();
		checkAnswer();
		cleanup();
	}
private:
	void createContext()
	{
		cl_uint numPlatforms;
		cl_int err = clGetPlatformIDs(0, nullptr, &numPlatforms);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to get platform number!");
		std::vector<cl_platform_id> platforms(numPlatforms);
		err = clGetPlatformIDs(platforms.size(), platforms.data(), nullptr);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to get platforms!");

		cl_uint numDevices;
		err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to get device number!");
		std::vector<cl_device_id> devices(numDevices);
		err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, devices.size(), devices.data(), nullptr);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to get devices!");
		m_device = devices[0];

		m_context = clCreateContext(nullptr, 1, &m_device, nullptr, nullptr, &err);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create context!");
	}

	void createCommandQueue()
	{
		cl_int err;
		m_commandQueue = clCreateCommandQueue(m_context, m_device, 0, &err);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create command queue!");
	}

	void createProgramAndKernel()
	{
		const std::string kernelSource = readCodeFromFile("image.cl");
		const char* source = kernelSource.c_str();
		cl_int err;
		m_program = clCreateProgramWithSource(m_context, 1, &source, nullptr, &err);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create program with source!");
		err = clBuildProgram(m_program, 1, &m_device, nullptr, nullptr, nullptr);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to build program!");

		m_kernel = clCreateKernel(m_program, "image", &err);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create kernel!");
	}

	void createImages()
	{
		cl_mem_flags srcFlags = CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY;
		cl_mem_flags dstFlags = CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY;
		cl_image_format format{CL_RGBA, CL_FLOAT};
		cl_image_desc desc{CL_MEM_OBJECT_IMAGE2D, m_width, m_height, 0, 0, 0, 0, 0, 0, nullptr};
		cl_int err;
		m_srcImage = clCreateImage(m_context, srcFlags, &format, &desc, m_data.data(), &err);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create image!");
		m_dstImage = clCreateImage(m_context, dstFlags, &format, &desc, nullptr, &err);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create image!");
	}

	void dispatch()
	{
		cl_int err = clSetKernelArg(m_kernel, 0, sizeof(cl_mem), &m_srcImage);
		err |= clSetKernelArg(m_kernel, 1, sizeof(cl_mem), &m_dstImage);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to create image!");

		constexpr cl_uint dim = 2;
		std::array<size_t, dim> globalSize{m_width, m_height};
		cl_event event;
		err = clEnqueueNDRangeKernel(m_commandQueue, m_kernel, 2, nullptr, globalSize.data(), nullptr, 0, nullptr, &event);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to enqueue kernel!");

		err = clFlush(m_commandQueue);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to flush queue!");

		err = clWaitForEvents(1, &event);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to wait for event!");
		clReleaseEvent(event);

		err = clFinish(m_commandQueue);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to finish queue!");
	}

	void checkAnswer()
	{
		std::vector<float> got(m_data.size());
		std::array<size_t, 3> origin{0, 0, 0};
		std::array<size_t, 3> region{ m_width, m_height, 1 };
		cl_int err = clEnqueueReadImage(m_commandQueue, m_dstImage, CL_TRUE, origin.data(), region.data(), 0, 0, got.data(), 0, nullptr, nullptr);
		checkErrorCode<cl_int, CL_SUCCESS>(err, "failed to read image!");

		bool valid = validateImageData(got);
		checkErrorCode<bool, true>(valid, "failed to validate image data!");

		std::cout << "OpenCL pass!" << std::endl;
	}

	void cleanup()
	{
		clReleaseMemObject(m_srcImage);
		clReleaseMemObject(m_dstImage);
		clReleaseKernel(m_kernel);
		clReleaseProgram(m_program);
		clReleaseCommandQueue(m_commandQueue);
		clReleaseContext(m_context);
	}

	cl_device_id m_device;
	cl_context m_context;
	cl_command_queue m_commandQueue;
	cl_program m_program;
	cl_kernel m_kernel;
	cl_mem m_srcImage;
	cl_mem m_dstImage;
};

}

}
