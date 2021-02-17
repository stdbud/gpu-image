#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <budImage.hpp>

namespace bud {

namespace vk {

class ImageVK final : public Imagef {
public:
	explicit ImageVK(const int width, const int height, const int nrChannels)
		: Imagef(width, height, nrChannels),
	      m_instance(VK_NULL_HANDLE),
	      m_queueFamilyIndex(-1),
	      m_physicalDevice(VK_NULL_HANDLE),
	      m_device(VK_NULL_HANDLE),
		  m_queue(VK_NULL_HANDLE),
	      m_srcImage(VK_NULL_HANDLE),
	      m_dstImage(VK_NULL_HANDLE),
	      m_srcImageMemory(VK_NULL_HANDLE),
	      m_dstImageMemory(VK_NULL_HANDLE),
	      m_srcImageView(VK_NULL_HANDLE),
	      m_dstImageView(VK_NULL_HANDLE),
	      m_srcTransferImage(VK_NULL_HANDLE),
		  m_dstTransferImage(VK_NULL_HANDLE),
		  m_srcTransferImageMemory(VK_NULL_HANDLE),
		  m_dstTransferImageMemory(VK_NULL_HANDLE),
		  m_srcTransferImageView(VK_NULL_HANDLE),
		  m_dstTransferImageView(VK_NULL_HANDLE),
		  m_shaderModule(VK_NULL_HANDLE),
	      m_descriptorSetLayout(VK_NULL_HANDLE),
	      m_pipelineLayout(VK_NULL_HANDLE),
	      m_pipeline(VK_NULL_HANDLE),
	      m_descriptorPool(VK_NULL_HANDLE),
	      m_descriptorSet(VK_NULL_HANDLE),
		  m_commandPool(VK_NULL_HANDLE),
	      m_commandBuffer(VK_NULL_HANDLE) {}

	void compute() override
	{
		createInstance();
		pickPhysicalDevice();
		createDevice();
		createImages();
		createTransferImages();
		createComputePipeline();
		createDescriptorSet();
		createCommandBuffer();
		dispatch();
		checkAnswer();
		cleanup();
	}
private:
	void createInstance()
	{
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		VkResult err = vkCreateInstance(&createInfo, nullptr, &m_instance);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create instance!");
	}

	void pickPhysicalDevice()
	{
		uint32_t deviceCount;
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

		for (auto device : devices) {
			uint32_t queueFamiliesCount;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamiliesCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamiliesCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamiliesCount, queueFamilies.data());

			for (uint32_t i = 0; i < queueFamiliesCount; i++) {
				if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
					m_queueFamilyIndex = i;
					m_physicalDevice = device;
					return;
				}
			}
		}

		checkErrorCode<bool, true>(false, "failed to pick suitable physical device!");
	}

	void createDevice()
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		const float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		VkResult err = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create device!");

		vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
	}

	void createImages()
	{
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageCreateInfo.extent = { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkResult err = vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_srcImage);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image!");

		imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		err = vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_dstImage);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image!");

		VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &physicalDeviceMemoryProperties);

		VkMemoryRequirements srcRequirements;
		vkGetImageMemoryRequirements(m_device, m_srcImage, &srcRequirements);
		VkMemoryRequirements dstRequirements;
		vkGetImageMemoryRequirements(m_device, m_srcImage, &dstRequirements);
		VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		for (uint32_t type = 0; type < physicalDeviceMemoryProperties.memoryTypeCount; type++) {
			if ((srcRequirements.memoryTypeBits & (1 << type)) &&
				((physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags & memoryProperties) == memoryProperties)) {;
				VkMemoryAllocateInfo memoryAllocateInfo{};
				memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memoryAllocateInfo.allocationSize = srcRequirements.size;
				memoryAllocateInfo.memoryTypeIndex = type;
				err = vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_srcImageMemory);
				checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to allocate memory!");
			}

			if ((dstRequirements.memoryTypeBits & (1 << type)) &&
				((physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags & memoryProperties) == memoryProperties)) {
				VkMemoryAllocateInfo memoryAllocateInfo{};
				memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memoryAllocateInfo.allocationSize = dstRequirements.size;
				memoryAllocateInfo.memoryTypeIndex = type;
				err = vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_dstImageMemory);
				checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to allocate memory!");
			}
		}

		if (m_srcImageMemory == VK_NULL_HANDLE || m_dstImageMemory == VK_NULL_HANDLE) checkErrorCode<bool, true>(false, "failed to create memory!");

		vkBindImageMemory(m_device, m_srcImage, m_srcImageMemory, 0);
		vkBindImageMemory(m_device, m_dstImage, m_dstImageMemory, 0);

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = m_srcImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange = subresourceRange;
		err = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_srcImageView);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image view!");

		imageViewCreateInfo.image = m_dstImage;
		err = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_dstImageView);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image view!");
	}

	void createTransferImages()
	{
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageCreateInfo.extent = { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkResult err = vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_srcTransferImage);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image!");

		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		err = vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_dstTransferImage);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image!");

		VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &physicalDeviceMemoryProperties);

		VkMemoryRequirements srcRequirements;
		vkGetImageMemoryRequirements(m_device, m_srcTransferImage, &srcRequirements);
		VkMemoryRequirements dstRequirements;
		vkGetImageMemoryRequirements(m_device, m_dstTransferImage, &dstRequirements);
		VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		for (uint32_t type = 0; type < physicalDeviceMemoryProperties.memoryTypeCount; type++) {
			if ((srcRequirements.memoryTypeBits & (1 << type)) &&
				((physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags & memoryProperties) == memoryProperties)) {
				;
				VkMemoryAllocateInfo memoryAllocateInfo{};
				memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memoryAllocateInfo.allocationSize = srcRequirements.size;
				memoryAllocateInfo.memoryTypeIndex = type;
				err = vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_srcTransferImageMemory);
				checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to allocate memory!");
			}

			if ((dstRequirements.memoryTypeBits & (1 << type)) &&
				((physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags & memoryProperties) == memoryProperties)) {
				VkMemoryAllocateInfo memoryAllocateInfo{};
				memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memoryAllocateInfo.allocationSize = dstRequirements.size;
				memoryAllocateInfo.memoryTypeIndex = type;
				err = vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_dstTransferImageMemory);
				checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to allocate memory!");
			}
		}

		if (m_srcTransferImageMemory == VK_NULL_HANDLE || m_dstTransferImageMemory == VK_NULL_HANDLE) checkErrorCode<bool, true>(false, "failed to create memory!");

		vkBindImageMemory(m_device, m_srcTransferImage, m_srcTransferImageMemory, 0);
		vkBindImageMemory(m_device, m_dstTransferImage, m_dstTransferImageMemory, 0);

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = m_srcTransferImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UINT;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange = subresourceRange;
		err = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_srcTransferImageView);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image view!");

		imageViewCreateInfo.image = m_dstImage;
		err = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_dstTransferImageView);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create image view!");

		void* mappedPointer;
		err = vkMapMemory(m_device, m_srcTransferImageMemory, 0, m_data.size() * sizeof(float), 0, &mappedPointer);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to map memory!");

		std::memcpy(mappedPointer, m_data.data(), m_data.size() * sizeof(uint8_t));

		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.memory = m_srcTransferImageMemory;
		memoryRange.offset = 0;
		memoryRange.size = m_data.size() * sizeof(float);
		err = vkFlushMappedMemoryRanges(m_device, 1, &memoryRange);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to flush memory!");

		vkUnmapMemory(m_device, m_srcTransferImageMemory);
	}

	void createComputePipeline()
	{
		const std::vector<char> spirvSource = readSpirvFromFile("comp.spv");
		checkErrorCode<bool, false>(spirvSource.empty(), "failed to read shader code from file!");

		VkShaderModuleCreateInfo shaderModuleCreateInfo{};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = spirvSource.size();
		shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spirvSource.data());
		VkResult err = vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &m_shaderModule);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create shader module!");

		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageCreateInfo.module = m_shaderModule;
		shaderStageCreateInfo.pName = "main";

		std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
		for (uint32_t i = 0; i < 2; i++) {
			bindings[i].binding = i;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[i].descriptorCount = 1;
			bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			bindings[i].pImmutableSamplers = nullptr;
		}

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo{};
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.bindingCount = 2;
		descSetLayoutCreateInfo.pBindings = bindings.data();
		err = vkCreateDescriptorSetLayout(m_device, &descSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create descriptor set layout!");

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
		err = vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create pipeline layout!");

		VkComputePipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stage = shaderStageCreateInfo;
		pipelineCreateInfo.layout = m_pipelineLayout;
		err = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create pipeline!");
	}

	void createDescriptorSet()
	{
		VkDescriptorPoolCreateInfo descPoolCreateInfo{};
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		descPoolCreateInfo.maxSets = 2;
		descPoolCreateInfo.poolSizeCount = 1;
		VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE , 1 };
		descPoolCreateInfo.pPoolSizes = &poolSize;
		VkResult err = vkCreateDescriptorPool(m_device, &descPoolCreateInfo, nullptr, &m_descriptorPool);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create descriptor pool!");

		VkDescriptorSetAllocateInfo descSetAllocateInfo{};
		descSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descSetAllocateInfo.descriptorPool = m_descriptorPool;
		descSetAllocateInfo.descriptorSetCount = 1;
		descSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
		err = vkAllocateDescriptorSets(m_device, &descSetAllocateInfo, &m_descriptorSet);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create descriptor set!");

		std::array<VkDescriptorImageInfo, 2> descriptorImageInfos{};
		descriptorImageInfos[0].imageView = m_srcImageView;
		descriptorImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		descriptorImageInfos[0].sampler = VK_NULL_HANDLE;
		descriptorImageInfos[1].imageView = m_dstImageView;
		descriptorImageInfos[1].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		descriptorImageInfos[1].sampler = VK_NULL_HANDLE;

		std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};
		for (uint32_t i = 0; i < 2; i++) {
			writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[i].dstSet = m_descriptorSet;
			writeDescriptorSets[i].dstBinding = i;
			writeDescriptorSets[i].descriptorCount = 1;
			writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSets[i].pImageInfo = &descriptorImageInfos[i];
		}
	    vkUpdateDescriptorSets(m_device, 2, writeDescriptorSets.data(), 0, nullptr);
	}

	void createCommandBuffer()
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkResult err = vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create command pool!");

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = m_commandPool;
		commandBufferAllocateInfo.commandBufferCount = 1;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		err = vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_commandBuffer);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create command buffer!");
	}

	void dispatch()
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VkResult err = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to begin command buffer!");

		VkImageCopy region{};
		region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 0 };
		region.srcOffset = { 0, 0, 0 };
		region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 0 };
		region.dstOffset = { 0, 0, 0 };
		region.extent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1};
		vkCmdCopyImage(m_commandBuffer, m_srcTransferImage, VK_IMAGE_LAYOUT_UNDEFINED, m_srcImage, VK_IMAGE_LAYOUT_UNDEFINED, 1, &region);

		VkMemoryBarrier preMemoryBarrier{};
		preMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		preMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		preMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkImageMemoryBarrier preImageMemoryBarrier{};
		preImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		preImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 0 };
		preImageMemoryBarrier.srcQueueFamilyIndex = m_queueFamilyIndex;
		preImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		preImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		preImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		preImageMemoryBarrier.image = m_srcImage;
		preImageMemoryBarrier.dstQueueFamilyIndex = m_queueFamilyIndex;
		preImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
			                 1, &preMemoryBarrier, 0, nullptr, 1, &preImageMemoryBarrier);

		vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
		vkCmdDispatch(m_commandBuffer, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1);

		VkMemoryBarrier postMemoryBarrier{};
		postMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		postMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		postMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		VkImageMemoryBarrier postImageMemoryBarrier{};
		postImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 0 };
		postImageMemoryBarrier.srcQueueFamilyIndex = m_queueFamilyIndex;
		postImageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		postImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		postImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		postImageMemoryBarrier.image = m_dstImage;
		postImageMemoryBarrier.dstQueueFamilyIndex = m_queueFamilyIndex;
		postImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
			                 1, &postMemoryBarrier, 0, nullptr, 1, &postImageMemoryBarrier);

		vkCmdCopyImage(m_commandBuffer, m_dstImage, VK_IMAGE_LAYOUT_UNDEFINED, m_dstTransferImage, VK_IMAGE_LAYOUT_UNDEFINED, 1, &region);

		err = vkEndCommandBuffer(m_commandBuffer);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to end command buffer!");
        
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_commandBuffer;

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence fence;
		err = vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to create fence!");

		err = vkQueueSubmit(m_queue, 1, &submitInfo, fence);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to submit queue!");

		vkWaitForFences(m_device, 1, &fence, VK_TRUE, 4700000000);
		vkDestroyFence(m_device, fence, nullptr);

		err = vkQueueWaitIdle(m_queue);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to wait for queue finish!");
	}

	void checkAnswer()
	{
		std::vector<float> got(m_data.size());
		void* mappedPointer = got.data();
		VkResult err = vkMapMemory(m_device, m_dstTransferImageMemory, 0, got.size() * sizeof(float), 0, &mappedPointer);
		checkErrorCode<VkResult, VK_SUCCESS>(err, "failed to map memory!");

		std::memcpy(got.data(), mappedPointer, got.size() * sizeof(float));

		vkUnmapMemory(m_device, m_dstTransferImageMemory);

		bool valid = validateImageData(got);
		checkErrorCode<bool, true>(valid, "failed to validate image data!");

		std::cout << "Vulkan pass!" << std::endl;
	}

	void cleanup()
	{
		vkDestroyCommandPool(m_device, m_commandPool, nullptr);
		vkFreeDescriptorSets(m_device, m_descriptorPool, 1, &m_descriptorSet);
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
		vkDestroyShaderModule(m_device, m_shaderModule, nullptr);
		vkDestroyImageView(m_device, m_srcImageView, nullptr);
		vkDestroyImageView(m_device, m_dstImageView, nullptr);
		vkFreeMemory(m_device, m_srcImageMemory, nullptr);
		vkFreeMemory(m_device, m_dstImageMemory, nullptr);
		vkDestroyImage(m_device, m_srcImage, nullptr);
		vkDestroyImage(m_device, m_dstImage, nullptr);
		vkDestroyDevice(m_device, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}

	VkInstance m_instance;
	uint32_t m_queueFamilyIndex;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	VkQueue m_queue;

	VkImage m_srcImage;
	VkImage m_dstImage;
	VkDeviceMemory m_srcImageMemory;
	VkDeviceMemory m_dstImageMemory;
	VkImageView m_srcImageView;
	VkImageView m_dstImageView;

	VkImage m_srcTransferImage;
	VkImage m_dstTransferImage;
	VkDeviceMemory m_srcTransferImageMemory;
	VkDeviceMemory m_dstTransferImageMemory;
	VkImageView m_srcTransferImageView;
	VkImageView m_dstTransferImageView;

	VkShaderModule m_shaderModule;
    VkDescriptorSetLayout m_descriptorSetLayout;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_pipeline;

	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSet;

	VkCommandPool m_commandPool;
	VkCommandBuffer m_commandBuffer;
};

}

}
