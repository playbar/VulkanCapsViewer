/*
*
* Vulkan hardware capability viewer
*
* Device information class
*
* Copyright (C) 2015 by Sascha Willems (www.saschawillems.de)
*
* This code is free software, you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License version 3 as published by the Free Software Foundation.
*
* Please review the following information to ensure the GNU Lesser
* General Public License version 3 requirements will be met:
* http://opensource.org/licenses/lgpl-3.0.html
*
* The code is distributed WITHOUT ANY WARRANTY; without even the
* implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE.  See the GNU LGPL 3.0 for more details.
*
*/

#pragma once

#include <vector>
#include <assert.h>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <fstream>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

#include "vulkan/vulkan.h"
#include "vulkanresources.h"
#include "vulkanLayerInfo.hpp"
#include "vulkanFormatInfo.hpp"
#include "vulkansurfaceinfo.hpp"
#include "vulkanpfn.h"

#ifdef __linux__
#include <sys/system_properties.h>
#endif

#include "vulkanandroid.h"

struct OSInfo
{
	std::string name;
	std::string version;
	std::string architecture;
};

struct VulkanQueueFamilyInfo
{
    VkQueueFamilyProperties properties;
    VkBool32 supportsPresent;
};

struct Feature2 {
    std::string name;
    VkBool32 supported;
    const char* extension;
    Feature2(std::string n, VkBool32 supp, const char* ext) : name(n), supported(supp), extension(ext) {}
};

struct Property2 {
    std::string name;
    std::string value;
    const char* extension;
    Property2(std::string n, QString val, const char* ext) : name(n), value(val.toStdString()), extension(ext) {}
};

class VulkanDeviceInfo
{
private:
	std::vector<VulkanLayerInfo> layers;
public:
	uint32_t id;

	std::map<std::string, std::string> properties;
	std::map<std::string, std::string> limits;
	std::map<std::string, VkBool32> features;
    std::map<std::string, std::string> platformdetails;

    // VK_KHR_get_physical_device_properties2
    std::vector<Feature2> features2;
    std::vector<Property2> properties2;

	VkPhysicalDevice device;
	VkDevice dev;

	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkPhysicalDeviceFeatures deviceFeatures;

    VkPhysicalDeviceProperties2KHR deviceProperties2;
    VkPhysicalDeviceFeatures2KHR deviceFeatures2;

	std::vector<VkExtensionProperties> extensions;
    std::vector<VulkanQueueFamilyInfo> queueFamilies;

	int32_t supportedFormatCount;
	std::vector<VulkanFormatInfo> formats;

    VulkanSurfaceInfo surfaceInfo;

	OSInfo os;

	std::string reportVersion;

	std::vector<VulkanLayerInfo> getLayers()
	{
		return layers;
	}

	/// <summary>
	///	Get list of global extensions for this device (not specific to any layer)
	/// </summary>
	void readExtensions()
	{
		assert(device != NULL);
		VkResult vkRes;
		do {
			uint32_t extCount;
			vkRes = vkEnumerateDeviceExtensionProperties(device, NULL, &extCount, NULL);
			assert(!vkRes);
			std::vector<VkExtensionProperties> exts(extCount);
			vkRes = vkEnumerateDeviceExtensionProperties(device, NULL, &extCount, &exts.front());
			for (auto& ext : exts) 
				extensions.push_back(ext);
		} while (vkRes == VK_INCOMPLETE);
		assert(!vkRes);
	}

    /// <summary>
    ///	Checks if the given extension is supported on this device
    /// </summary>
    bool extensionSupported(const char* extensionName)
    {
        for (auto& ext : extensions) {
            if (strcmp(ext.extensionName, extensionName) == 0) {
                return true;
            }
        }
        return false;
    }

	/// <summary>
	///	Get list of available device layers
	/// </summary>
	void readLayers()
	{
		assert(device != NULL);
		VkResult vkRes;
		do {
			uint32_t layerCount;			
			vkRes = vkEnumerateDeviceLayerProperties(device, &layerCount, NULL);
			std::vector<VkLayerProperties> props(layerCount);
			if (layerCount > 0)
			{
				vkRes = vkEnumerateDeviceLayerProperties(device, &layerCount, &props.front());
				
			}
			for (auto& prop : props)
			{
				VulkanLayerInfo layerInfo;
				layerInfo.properties = prop;
				// Get Layer extensions
				VkResult vkRes;
				do {
					uint32_t extCount;
					vkRes = vkEnumerateDeviceExtensionProperties(device, prop.layerName, &extCount, NULL);
					assert(!vkRes);
					if (extCount > 0) {
						std::vector<VkExtensionProperties> exts(extCount);
						vkRes = vkEnumerateDeviceExtensionProperties(device, prop.layerName, &extCount, &exts.front());
						for (auto& ext : exts)
							layerInfo.extensions.push_back(ext);
					}
				} while (vkRes == VK_INCOMPLETE);
				assert(!vkRes);
				// Push to layer list
				layers.push_back(layerInfo);
			}
		} while (vkRes == VK_INCOMPLETE);
	}

	/// <summary>
	///	Get list of all supported image formats
	/// </summary>
	void readSupportedFormats()
	{
		supportedFormatCount = 0;
		assert(device != NULL);
        for (int32_t format = VK_FORMAT_BEGIN_RANGE+1; format < VK_FORMAT_END_RANGE+1; format++)
		{
			VulkanFormatInfo formatInfo = {};
			formatInfo.format = (VkFormat)format;
			vkGetPhysicalDeviceFormatProperties(device, formatInfo.format, &formatInfo.properties);
			formatInfo.supported =
				(formatInfo.properties.linearTilingFeatures != 0) |
				(formatInfo.properties.optimalTilingFeatures != 0) |
				(formatInfo.properties.bufferFeatures != 0);			
			formats.push_back(formatInfo);
			if (formatInfo.supported)
				supportedFormatCount++;
		}
	}

	/// <summary>
    ///	Get list of available device queue families
	/// </summary>
    void readQueueFamilies()
	{
		assert(device != NULL);
		uint32_t queueCount;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, NULL);
		assert(queueCount > 0);
		std::vector<VkQueueFamilyProperties> qs(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, &qs.front());
        uint32_t index = 0;
		for (auto& q : qs)
        {
            VulkanQueueFamilyInfo queueFamilyInfo{};
            queueFamilyInfo.properties = q;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            queueFamilyInfo.supportsPresent = vkGetPhysicalDeviceWin32PresentationSupportKHR(device, index);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
            // On Android all physical devices and queue families must support present
            queueFamilyInfo.supportsPresent = true;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
            // todo
//            queueFamilyInfo.supportsPresent = PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR(device, index);
#endif
            queueFamilies.push_back(queueFamilyInfo);
            index++;
        }
	}

    /// <summary>
    ///	Convert raw driver version read via api depending on
    /// vendor conventions
    /// </summary>
    std::string getDriverVersion()
    {
        // NVIDIA
        if (props.vendorID == 4318)
        {
            // 10 bits = major version (up to r1023)
            // 8 bits = minor version (up to 255)
            // 8 bits = secondary branch version/build version (up to 255)
            // 6 bits = tertiary branch/build version (up to 63)

            uint32_t major = (props.driverVersion >> 22) & 0x3ff;
            uint32_t minor = (props.driverVersion >> 14) & 0x0ff;
            uint32_t secondaryBranch = (props.driverVersion >> 6) & 0x0ff;
            uint32_t tertiaryBranch = (props.driverVersion) & 0x003f;

            return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(secondaryBranch) + "." + std::to_string(tertiaryBranch);
        }
        else
        {
           // todo : Add mappings for other vendors
           return vulkanResources::versionToString(props.driverVersion);
        }
    }

	/// <summary>
	///	Request physical device properties
	/// </summary>
	void readPhyiscalProperties()
	{
		assert(device != NULL);
		vkGetPhysicalDeviceProperties(device, &props);
		properties.clear();

		properties["devicename"] = props.deviceName;
        properties["driverversionraw"] = std::to_string(props.driverVersion);
        properties["driverversion"] = getDriverVersion();

		properties["apiversion"] = vulkanResources::versionToString(props.apiVersion);
		properties["headerversion"] = std::to_string(VK_HEADER_VERSION);
        properties["vendorid"] = std::to_string(props.vendorID);
        properties["deviceid"] = std::to_string(props.deviceID);
		properties["devicetype"] = vulkanResources::physicalDeviceTypeString(props.deviceType);

        // Sparse residency
        properties["residencyStandard2DBlockShape"] = std::to_string(props.sparseProperties.residencyStandard2DBlockShape);
        properties["residencyStandard2DMSBlockShape"] = std::to_string(props.sparseProperties.residencyStandard2DMultisampleBlockShape);
        properties["residencyStandard3DBlockShape"] = std::to_string(props.sparseProperties.residencyStandard3DBlockShape);
        properties["residencyAlignedMipSize"] = std::to_string(props.sparseProperties.residencyAlignedMipSize);
        properties["residencyNonResidentStrict"] = std::to_string(props.sparseProperties.residencyNonResidentStrict);

        // VK_KHR_get_physical_device_properties2
        if (pfnGetPhysicalDeviceFeatures2KHR) {
            // VK_KHX_multiview
            if (extensionSupported(VK_KHX_MULTIVIEW_EXTENSION_NAME)) {
                VkPhysicalDeviceProperties2KHR deviceProps2{};
                VkPhysicalDeviceMultiviewPropertiesKHX extProps{};
                extProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHX;
                deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
                deviceProps2.pNext = &extProps;
                pfnGetPhysicalDeviceProperties2KHR(device, &deviceProps2);
                properties2.push_back(Property2("maxMultiviewViewCount", QString::number(extProps.maxMultiviewViewCount), VK_KHX_MULTIVIEW_EXTENSION_NAME));
                properties2.push_back(Property2("maxMultiviewInstanceIndex", QString::number(extProps.maxMultiviewInstanceIndex), VK_KHX_MULTIVIEW_EXTENSION_NAME));
            }
            // VK_KHR_push_descriptor
            if (extensionSupported(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) {
                VkPhysicalDeviceProperties2KHR deviceProps2{};
                VkPhysicalDevicePushDescriptorPropertiesKHR extProps{};
                extProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
                deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
                deviceProps2.pNext = &extProps;
                pfnGetPhysicalDeviceProperties2KHR(device, &deviceProps2);
                properties2.push_back(Property2("maxPushDescriptors", QString::number(extProps.maxPushDescriptors), VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME));
            }
            // VK_EXT_discard_rectangles
            if (extensionSupported(VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME)) {
                VkPhysicalDeviceProperties2KHR deviceProps2{};
                VkPhysicalDeviceDiscardRectanglePropertiesEXT extProps{};
                extProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT;
                deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
                deviceProps2.pNext = &extProps;
                pfnGetPhysicalDeviceProperties2KHR(device, &deviceProps2);
                properties2.push_back(Property2("maxDiscardRectangles", QString::number(extProps.maxDiscardRectangles), VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME));
            }
            // VK_NVX_multiview_per_view_attributes
            if (extensionSupported(VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME)) {
                VkPhysicalDeviceProperties2KHR deviceProps2{};
                VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX extProps{};
                extProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT;
                deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX;
                deviceProps2.pNext = &extProps;
                pfnGetPhysicalDeviceProperties2KHR(device, &deviceProps2);
                properties2.push_back(Property2("perViewPositionAllComponents", QString::number(extProps.perViewPositionAllComponents), VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME));
            }
        }
	}
	
	/// <summary>
	///	Request physical device features
	/// </summary>
	void readPhyiscalFeatures()
	{
		assert(device != NULL);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		features.clear();
		features["robustBufferAccess"] = deviceFeatures.robustBufferAccess;
		features["fullDrawIndexUint32"] = deviceFeatures.fullDrawIndexUint32;
		features["imageCubeArray"] = deviceFeatures.imageCubeArray;
		features["independentBlend"] = deviceFeatures.independentBlend;
		features["geometryShader"] = deviceFeatures.geometryShader;
		features["tessellationShader"] = deviceFeatures.tessellationShader;
		features["sampleRateShading"] = deviceFeatures.sampleRateShading;
		features["dualSrcBlend"] = deviceFeatures.dualSrcBlend;
		features["logicOp"] = deviceFeatures.logicOp;
		features["multiDrawIndirect"] = deviceFeatures.multiDrawIndirect;
		features["drawIndirectFirstInstance"] = deviceFeatures.drawIndirectFirstInstance;
		features["depthClamp"] = deviceFeatures.depthClamp;
		features["depthBiasClamp"] = deviceFeatures.depthBiasClamp;
		features["fillModeNonSolid"] = deviceFeatures.fillModeNonSolid;
		features["depthBounds"] = deviceFeatures.depthBounds;
		features["wideLines"] = deviceFeatures.wideLines;
		features["largePoints"] = deviceFeatures.largePoints;
		features["alphaToOne"] = deviceFeatures.alphaToOne;
		features["multiViewport"] = deviceFeatures.multiViewport;
		features["samplerAnisotropy"] = deviceFeatures.samplerAnisotropy;
		features["textureCompressionETC2"] = deviceFeatures.textureCompressionETC2;
		features["textureCompressionASTC_LDR"] = deviceFeatures.textureCompressionASTC_LDR;
		features["textureCompressionBC"] = deviceFeatures.textureCompressionBC;
		features["occlusionQueryPrecise"] = deviceFeatures.occlusionQueryPrecise;
		features["pipelineStatisticsQuery"] = deviceFeatures.pipelineStatisticsQuery;
		features["vertexPipelineStoresAndAtomics"] = deviceFeatures.vertexPipelineStoresAndAtomics;
		features["fragmentStoresAndAtomics"] = deviceFeatures.fragmentStoresAndAtomics;
		features["shaderTessellationAndGeometryPointSize"] = deviceFeatures.shaderTessellationAndGeometryPointSize;
		features["shaderImageGatherExtended"] = deviceFeatures.shaderImageGatherExtended;
		features["shaderStorageImageExtendedFormats"] = deviceFeatures.shaderStorageImageExtendedFormats;
		features["shaderStorageImageMultisample"] = deviceFeatures.shaderStorageImageMultisample;
		features["shaderStorageImageReadWithoutFormat"] = deviceFeatures.shaderStorageImageReadWithoutFormat;
		features["shaderStorageImageWriteWithoutFormat"] = deviceFeatures.shaderStorageImageWriteWithoutFormat;
		features["shaderUniformBufferArrayDynamicIndexing"] = deviceFeatures.shaderUniformBufferArrayDynamicIndexing;
		features["shaderSampledImageArrayDynamicIndexing"] = deviceFeatures.shaderSampledImageArrayDynamicIndexing;
		features["shaderStorageBufferArrayDynamicIndexing"] = deviceFeatures.shaderStorageBufferArrayDynamicIndexing;
		features["shaderStorageImageArrayDynamicIndexing"] = deviceFeatures.shaderStorageImageArrayDynamicIndexing;
		features["shaderClipDistance"] = deviceFeatures.shaderClipDistance;
		features["shaderCullDistance"] = deviceFeatures.shaderCullDistance;
		features["shaderFloat64"] = deviceFeatures.shaderFloat64;
		features["shaderInt64"] = deviceFeatures.shaderInt64;
		features["shaderInt16"] = deviceFeatures.shaderInt16;
		features["shaderResourceResidency"] = deviceFeatures.shaderResourceResidency;
		features["shaderResourceMinLod"] = deviceFeatures.shaderResourceMinLod;
		features["sparseBinding"] = deviceFeatures.sparseBinding;
		features["sparseResidencyBuffer"] = deviceFeatures.sparseResidencyBuffer;
		features["sparseResidencyImage2D"] = deviceFeatures.sparseResidencyImage2D;
		features["sparseResidencyImage3D"] = deviceFeatures.sparseResidencyImage3D;
		features["sparseResidency2Samples"] = deviceFeatures.sparseResidency2Samples;
		features["sparseResidency4Samples"] = deviceFeatures.sparseResidency4Samples;
		features["sparseResidency8Samples"] = deviceFeatures.sparseResidency8Samples;
		features["sparseResidency16Samples"] = deviceFeatures.sparseResidency16Samples;
		features["sparseResidencyAliased"] = deviceFeatures.sparseResidencyAliased;
		features["variableMultisampleRate"] = deviceFeatures.variableMultisampleRate;
		features["inheritedQueries"] = deviceFeatures.inheritedQueries;

        // VK_KHR_get_physical_device_properties2
        if (pfnGetPhysicalDeviceFeatures2KHR) {
            // VK_KHX_multiview
            if (extensionSupported(VK_KHX_MULTIVIEW_EXTENSION_NAME)) {
                VkPhysicalDeviceFeatures2KHR deviceFeatures2{};
                VkPhysicalDeviceMultiviewFeaturesKHX multiViewFeatures{};
                multiViewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHX;
                deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
                deviceFeatures2.pNext = &multiViewFeatures;
                pfnGetPhysicalDeviceFeatures2KHR(device, &deviceFeatures2);
                features2.push_back(Feature2("multiview", multiViewFeatures.multiview, VK_KHX_MULTIVIEW_EXTENSION_NAME));
                features2.push_back(Feature2("multiviewGeometryShader", multiViewFeatures.multiviewGeometryShader, VK_KHX_MULTIVIEW_EXTENSION_NAME));
                features2.push_back(Feature2("multiviewTessellationShader", multiViewFeatures.multiviewTessellationShader, VK_KHX_MULTIVIEW_EXTENSION_NAME));
            }
        }
	}

	/// <summary>
	///	Copy physical device limits into a map
	/// </summary>
	void readPhyiscalLimits()
	{
		limits.clear();
        limits["maxImageDimension1D"] = std::to_string(props.limits.maxImageDimension1D);
        limits["maxImageDimension2D"] = std::to_string(props.limits.maxImageDimension2D);
        limits["maxImageDimension3D"] = std::to_string(props.limits.maxImageDimension3D);
        limits["maxImageDimensionCube"] = std::to_string(props.limits.maxImageDimensionCube);
        limits["maxImageArrayLayers"] = std::to_string(props.limits.maxImageArrayLayers);
        limits["maxTexelBufferElements"] = std::to_string(props.limits.maxTexelBufferElements);
        limits["maxUniformBufferRange"] = std::to_string(props.limits.maxUniformBufferRange);
        limits["maxStorageBufferRange"] = std::to_string(props.limits.maxStorageBufferRange);
        limits["maxPushConstantsSize"] = std::to_string(props.limits.maxPushConstantsSize);
        limits["maxMemoryAllocationCount"] = std::to_string(props.limits.maxMemoryAllocationCount);
        limits["maxSamplerAllocationCount"] = std::to_string(props.limits.maxSamplerAllocationCount);
        limits["bufferImageGranularity"] = std::to_string(props.limits.bufferImageGranularity);
        limits["sparseAddressSpaceSize"] = std::to_string(props.limits.sparseAddressSpaceSize);
        limits["maxBoundDescriptorSets"] = std::to_string(props.limits.maxBoundDescriptorSets);
        limits["maxPerStageDescriptorSamplers"] = std::to_string(props.limits.maxPerStageDescriptorSamplers);
        limits["maxPerStageDescriptorUniformBuffers"] = std::to_string(props.limits.maxPerStageDescriptorUniformBuffers);
        limits["maxPerStageDescriptorStorageBuffers"] = std::to_string(props.limits.maxPerStageDescriptorStorageBuffers);
        limits["maxPerStageDescriptorSampledImages"] = std::to_string(props.limits.maxPerStageDescriptorSampledImages);
        limits["maxPerStageDescriptorStorageImages"] = std::to_string(props.limits.maxPerStageDescriptorStorageImages);
        limits["maxPerStageDescriptorInputAttachments"] = std::to_string(props.limits.maxPerStageDescriptorInputAttachments);
        limits["maxPerStageResources"] = std::to_string(props.limits.maxPerStageResources);
        limits["maxDescriptorSetSamplers"] = std::to_string(props.limits.maxDescriptorSetSamplers);
        limits["maxDescriptorSetUniformBuffers"] = std::to_string(props.limits.maxDescriptorSetUniformBuffers);
        limits["maxDescriptorSetUniformBuffersDynamic"] = std::to_string(props.limits.maxDescriptorSetUniformBuffersDynamic);
        limits["maxDescriptorSetStorageBuffers"] = std::to_string(props.limits.maxDescriptorSetStorageBuffers);
        limits["maxDescriptorSetStorageBuffersDynamic"] = std::to_string(props.limits.maxDescriptorSetStorageBuffersDynamic);
        limits["maxDescriptorSetSampledImages"] = std::to_string(props.limits.maxDescriptorSetSampledImages);
        limits["maxDescriptorSetStorageImages"] = std::to_string(props.limits.maxDescriptorSetStorageImages);
        limits["maxDescriptorSetInputAttachments"] = std::to_string(props.limits.maxDescriptorSetInputAttachments);
        limits["maxVertexInputAttributes"] = std::to_string(props.limits.maxVertexInputAttributes);
        limits["maxVertexInputBindings"] = std::to_string(props.limits.maxVertexInputBindings);
        limits["maxVertexInputAttributeOffset"] = std::to_string(props.limits.maxVertexInputAttributeOffset);
        limits["maxVertexInputBindingStride"] = std::to_string(props.limits.maxVertexInputBindingStride);
        limits["maxVertexOutputComponents"] = std::to_string(props.limits.maxVertexOutputComponents);
        limits["maxTessellationGenerationLevel"] = std::to_string(props.limits.maxTessellationGenerationLevel);
        limits["maxTessellationPatchSize"] = std::to_string(props.limits.maxTessellationPatchSize);
        limits["maxTessellationControlPerVertexInputComponents"] = std::to_string(props.limits.maxTessellationControlPerVertexInputComponents);
        limits["maxTessellationControlPerVertexOutputComponents"] = std::to_string(props.limits.maxTessellationControlPerVertexOutputComponents);
        limits["maxTessellationControlPerPatchOutputComponents"] = std::to_string(props.limits.maxTessellationControlPerPatchOutputComponents);
        limits["maxTessellationControlTotalOutputComponents"] = std::to_string(props.limits.maxTessellationControlTotalOutputComponents);
        limits["maxTessellationEvaluationInputComponents"] = std::to_string(props.limits.maxTessellationEvaluationInputComponents);
        limits["maxTessellationEvaluationOutputComponents"] = std::to_string(props.limits.maxTessellationEvaluationOutputComponents);
        limits["maxGeometryShaderInvocations"] = std::to_string(props.limits.maxGeometryShaderInvocations);
        limits["maxGeometryInputComponents"] = std::to_string(props.limits.maxGeometryInputComponents);
        limits["maxGeometryOutputComponents"] = std::to_string(props.limits.maxGeometryOutputComponents);
        limits["maxGeometryOutputVertices"] = std::to_string(props.limits.maxGeometryOutputVertices);
        limits["maxGeometryTotalOutputComponents"] = std::to_string(props.limits.maxGeometryTotalOutputComponents);
        limits["maxFragmentInputComponents"] = std::to_string(props.limits.maxFragmentInputComponents);
        limits["maxFragmentOutputAttachments"] = std::to_string(props.limits.maxFragmentOutputAttachments);
        limits["maxFragmentDualSrcAttachments"] = std::to_string(props.limits.maxFragmentDualSrcAttachments);
        limits["maxFragmentCombinedOutputResources"] = std::to_string(props.limits.maxFragmentCombinedOutputResources);
        limits["maxComputeSharedMemorySize"] = std::to_string(props.limits.maxComputeSharedMemorySize);
        limits["maxComputeWorkGroupCount[0]"] = std::to_string(props.limits.maxComputeWorkGroupCount[0]);
        limits["maxComputeWorkGroupCount[1]"] = std::to_string(props.limits.maxComputeWorkGroupCount[1]);
        limits["maxComputeWorkGroupCount[2]"] = std::to_string(props.limits.maxComputeWorkGroupCount[2]);
        limits["maxComputeWorkGroupInvocations"] = std::to_string(props.limits.maxComputeWorkGroupInvocations);
        limits["maxComputeWorkGroupSize[0]"] = std::to_string(props.limits.maxComputeWorkGroupSize[0]);
        limits["maxComputeWorkGroupSize[1]"] = std::to_string(props.limits.maxComputeWorkGroupSize[1]);
        limits["maxComputeWorkGroupSize[2]"] = std::to_string(props.limits.maxComputeWorkGroupSize[2]);
        limits["subPixelPrecisionBits"] = std::to_string(props.limits.subPixelPrecisionBits);
        limits["subTexelPrecisionBits"] = std::to_string(props.limits.subTexelPrecisionBits);
        limits["mipmapPrecisionBits"] = std::to_string(props.limits.mipmapPrecisionBits);
        limits["maxDrawIndexedIndexValue"] = std::to_string(props.limits.maxDrawIndexedIndexValue);
        limits["maxDrawIndirectCount"] = std::to_string(props.limits.maxDrawIndirectCount);
        limits["maxSamplerLodBias"] = std::to_string(props.limits.maxSamplerLodBias);
        limits["maxSamplerAnisotropy"] = std::to_string(props.limits.maxSamplerAnisotropy);
        limits["maxViewports"] = std::to_string(props.limits.maxViewports);
        limits["maxViewportDimensions[0]"] = std::to_string(props.limits.maxViewportDimensions[0]);
        limits["maxViewportDimensions[1]"] = std::to_string(props.limits.maxViewportDimensions[1]);
        limits["viewportBoundsRange[0]"] = std::to_string(props.limits.viewportBoundsRange[0]);
        limits["viewportBoundsRange[1]"] = std::to_string(props.limits.viewportBoundsRange[1]);
        limits["viewportSubPixelBits"] = std::to_string(props.limits.viewportSubPixelBits);
        limits["minMemoryMapAlignment"] = std::to_string(props.limits.minMemoryMapAlignment);
        limits["minTexelBufferOffsetAlignment"] = std::to_string(props.limits.minTexelBufferOffsetAlignment);
        limits["minUniformBufferOffsetAlignment"] = std::to_string(props.limits.minUniformBufferOffsetAlignment);
        limits["minStorageBufferOffsetAlignment"] = std::to_string(props.limits.minStorageBufferOffsetAlignment);
        limits["minTexelOffset"] = std::to_string(props.limits.minTexelOffset);
        limits["maxTexelOffset"] = std::to_string(props.limits.maxTexelOffset);
        limits["minTexelGatherOffset"] = std::to_string(props.limits.minTexelGatherOffset);
        limits["maxTexelGatherOffset"] = std::to_string(props.limits.maxTexelGatherOffset);
        limits["minInterpolationOffset"] = std::to_string(props.limits.minInterpolationOffset);
        limits["maxInterpolationOffset"] = std::to_string(props.limits.maxInterpolationOffset);
        limits["subPixelInterpolationOffsetBits"] = std::to_string(props.limits.subPixelInterpolationOffsetBits);
        limits["maxFramebufferWidth"] = std::to_string(props.limits.maxFramebufferWidth);
        limits["maxFramebufferHeight"] = std::to_string(props.limits.maxFramebufferHeight);
        limits["maxFramebufferLayers"] = std::to_string(props.limits.maxFramebufferLayers);
        limits["framebufferColorSampleCounts"] = std::to_string(props.limits.framebufferColorSampleCounts);
        limits["framebufferDepthSampleCounts"] = std::to_string(props.limits.framebufferDepthSampleCounts);
        limits["framebufferStencilSampleCounts"] = std::to_string(props.limits.framebufferStencilSampleCounts);
        limits["framebufferNoAttachmentsSampleCounts"] = std::to_string(props.limits.framebufferNoAttachmentsSampleCounts);
        limits["maxColorAttachments"] = std::to_string(props.limits.maxColorAttachments);
        limits["sampledImageColorSampleCounts"] = std::to_string(props.limits.sampledImageColorSampleCounts);
        limits["sampledImageIntegerSampleCounts"] = std::to_string(props.limits.sampledImageIntegerSampleCounts);
        limits["sampledImageDepthSampleCounts"] = std::to_string(props.limits.sampledImageDepthSampleCounts);
        limits["sampledImageStencilSampleCounts"] = std::to_string(props.limits.sampledImageStencilSampleCounts);
        limits["storageImageSampleCounts"] = std::to_string(props.limits.storageImageSampleCounts);
        limits["maxSampleMaskWords"] = std::to_string(props.limits.maxSampleMaskWords);
        limits["timestampComputeAndGraphics"] = std::to_string(props.limits.timestampComputeAndGraphics);
        limits["timestampPeriod"] = std::to_string(props.limits.timestampPeriod);
        limits["maxClipDistances"] = std::to_string(props.limits.maxClipDistances);
        limits["maxCullDistances"] = std::to_string(props.limits.maxCullDistances);
        limits["maxCombinedClipAndCullDistances"] = std::to_string(props.limits.maxCombinedClipAndCullDistances);
        limits["discreteQueuePriorities"] = std::to_string(props.limits.discreteQueuePriorities);
        limits["pointSizeRange[0]"] = std::to_string(props.limits.pointSizeRange[0]);
        limits["pointSizeRange[1]"] = std::to_string(props.limits.pointSizeRange[1]);
        limits["lineWidthRange[0]"] = std::to_string(props.limits.lineWidthRange[0]);
        limits["lineWidthRange[1]"] = std::to_string(props.limits.lineWidthRange[1]);
        limits["pointSizeGranularity"] = std::to_string(props.limits.pointSizeGranularity);
        limits["lineWidthGranularity"] = std::to_string(props.limits.lineWidthGranularity);
        limits["strictLines"] = std::to_string(props.limits.strictLines);
        limits["standardSampleLocations"] = std::to_string(props.limits.standardSampleLocations);
        limits["optimalBufferCopyOffsetAlignment"] = std::to_string(props.limits.optimalBufferCopyOffsetAlignment);
        limits["optimalBufferCopyRowPitchAlignment"] = std::to_string(props.limits.optimalBufferCopyRowPitchAlignment);
        limits["nonCoherentAtomSize"] = std::to_string(props.limits.nonCoherentAtomSize);
	}

	/// <summary>
	///	Request physical memory properties
	/// </summary>
	void readPhyiscalMemoryProperties()
	{
		assert(device != NULL);
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);
	}

    /// <summary>
    ///	Read OS dependent surface information
    /// </summary>
    void readSurfaceInfo(VkSurfaceKHR surface, std::string surfaceExtension)
    {
        assert(device != NULL);
        surfaceInfo.validSurface = (surface != VK_NULL_HANDLE);
        surfaceInfo.surfaceExtension = surfaceExtension;
        surfaceInfo.get(device, surface);
    }

#if defined(__ANDROID__)
    std::string getSystemProperty(char* propname)
    {
        char prop[PROP_VALUE_MAX+1];
        int len = __system_property_get(propname, prop);
        if (len > 0) {
            return std::string(prop);
        } else {
            return "";
        }
    }
#endif

    /// <summary>
    ///	Read platform specific detail info
    /// </summary>
    void readPlatformDetails()
    {
        // Android specific build info
#if defined(__ANDROID__)
        platformdetails["android.ProductModel"] = getSystemProperty("ro.product.model");
        platformdetails["android.ProductManufacturer"] = getSystemProperty("ro.product.manufacturer");
        platformdetails["android.BuildID"] = getSystemProperty("ro.build.id");
        platformdetails["android.BuildVersionIncremental"] = getSystemProperty("ro.build.version.incremental");
#endif
    }

    /// <summary>
    ///	Save report to JSON file
    /// </summary>
    void saveToJSON(std::string fileName, std::string submitter, std::string comment)
	{
		QJsonObject root;

		// Device properties
		QJsonObject jsonProperties;
		for (auto& prop : properties)
		{
			jsonProperties[QString::fromStdString(prop.first)] = QString::fromStdString(prop.second);
		}
		root["deviceproperties"] = jsonProperties;

		// Device features
		QJsonObject jsonFeatures;
		for (auto& feature : features)
		{
			jsonFeatures[QString::fromStdString(feature.first)] = QString::number(feature.second);
		}
		root["devicefeatures"] = jsonFeatures;

		// Device limits
		QJsonObject jsonLimits;
		for (auto& limit : limits)
		{
			jsonLimits[QString::fromStdString(limit.first)] = QString::fromStdString(limit.second);
		}
		root["devicelimits"] = jsonLimits;

		// Extensions
		QJsonArray jsonExtensions;
		for (auto& ext : extensions)
		{
			QJsonObject jsonExt;
			jsonExt["extname"] = ext.extensionName;
			jsonExt["specversion"] = QString::number(ext.specVersion);
			jsonExtensions.append(jsonExt);
		}
		root["extensions"] = jsonExtensions;

		// Formats
		QJsonArray jsonFormats;
		for (auto& format : formats)
		{
			QJsonObject jsonFormat;
			jsonFormat["format"] = QString::number(format.format);
			jsonFormat["supported"] = QString::number(format.supported);
			jsonFormat["lineartilingfeatures"] = QString::number(format.properties.linearTilingFeatures);
			jsonFormat["optimaltilingfeatures"] = QString::number(format.properties.optimalTilingFeatures);
			jsonFormat["bufferfeatures"] = QString::number(format.properties.bufferFeatures);
			jsonFormats.append(jsonFormat);
		}
		root["formats"] = jsonFormats;

		// Layers
		QJsonArray jsonLayers;
		for (auto& layer : layers)
		{
			QJsonObject jsonLayer;
			jsonLayer["layername"] = layer.properties.layerName;
			jsonLayer["description"] = layer.properties.description;
			jsonLayer["specversion"] = QString::number(layer.properties.specVersion);
			jsonLayer["implversion"] = QString::number(layer.properties.implementationVersion);
			QJsonArray jsonLayerExtensions;
			for (auto& ext : layer.extensions)
			{
				QJsonObject jsonExt;
				jsonExt["extname"] = ext.extensionName;
				jsonExt["specversion"] = QString::number(ext.specVersion);
				jsonLayerExtensions.append(jsonExt);
			}
			jsonLayer["extensions"] = jsonLayerExtensions;
			jsonLayers.append(jsonLayer);
		}
		root["layers"] = jsonLayers;

		// Queues
		QJsonArray jsonQueues;
        for (auto& queueFamily : queueFamilies)
		{
			QJsonObject jsonQueue;
            jsonQueue["flags"] = QString::number(queueFamily.properties.queueFlags);
            jsonQueue["count"] = QString::number(queueFamily.properties.queueCount);
            jsonQueue["timestampValidBits"] = QString::number(queueFamily.properties.timestampValidBits);
            jsonQueue["minImageTransferGranularity.width"] = QString::number(queueFamily.properties.minImageTransferGranularity.width);
            jsonQueue["minImageTransferGranularity.height"] = QString::number(queueFamily.properties.minImageTransferGranularity.height);
            jsonQueue["minImageTransferGranularity.depth"] = QString::number(queueFamily.properties.minImageTransferGranularity.depth);
            jsonQueue["supportsPresent"] = QString::number(queueFamily.supportsPresent);
            jsonQueues.append(jsonQueue);
		}
        //todo: rename
		root["queues"] = jsonQueues;

		// Memory properties
		QJsonObject jsonMemoryProperties;
		// Available memory types
		jsonMemoryProperties["memorytypecount"] = QString::number(memoryProperties.memoryTypeCount);
		QJsonArray memtypes;
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			QJsonObject memtype;
			memtype["propertyflags"] = QString::number(memoryProperties.memoryTypes[i].propertyFlags);
			memtype["heapindex"] = QString::number(memoryProperties.memoryTypes[i].heapIndex);
			memtypes.append(memtype);
		}
		jsonMemoryProperties["memorytypes"] = memtypes;
		// Memory heaps
		jsonMemoryProperties["memoryheapcount"] = QString::number(memoryProperties.memoryHeapCount);
		QJsonArray heaps;
		for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++)
		{
			QJsonObject memheap;
			memheap["flags"] = QString::number(memoryProperties.memoryHeaps[i].flags);
			memheap["size"] = QString::number(memoryProperties.memoryHeaps[i].size);
			heaps.append(memheap);
		}
		jsonMemoryProperties["heaps"] = heaps;
		root["memoryproperties"] = jsonMemoryProperties;

        // Surface capabilities
        QJsonObject jsonSurfaceCaps;
        jsonSurfaceCaps["validSurface"] = QString::number(surfaceInfo.validSurface);
        if (surfaceInfo.validSurface)
        {
            jsonSurfaceCaps["surfaceExtension"] = QString::fromStdString(surfaceInfo.surfaceExtension);
            jsonSurfaceCaps["minImageCount"] = QString::number(surfaceInfo.capabilities.minImageCount);
            jsonSurfaceCaps["maxImageCount"] = QString::number(surfaceInfo.capabilities.maxImageCount);
            jsonSurfaceCaps["maxImageArrayLayers"] = QString::number(surfaceInfo.capabilities.maxImageArrayLayers);
            jsonSurfaceCaps["minImageExtent.width"] = QString::number(surfaceInfo.capabilities.minImageExtent.width);
            jsonSurfaceCaps["minImageExtent.height"] = QString::number(surfaceInfo.capabilities.minImageExtent.height);
            jsonSurfaceCaps["maxImageExtent.width"] = QString::number(surfaceInfo.capabilities.maxImageExtent.width);
            jsonSurfaceCaps["maxImageExtent.height"] = QString::number(surfaceInfo.capabilities.maxImageExtent.height);
            jsonSurfaceCaps["supportedUsageFlags"] = QString::number(surfaceInfo.capabilities.supportedUsageFlags);
            jsonSurfaceCaps["supportedTransforms"] = QString::number(surfaceInfo.capabilities.supportedTransforms);
            jsonSurfaceCaps["supportedCompositeAlpha"] = QString::number(surfaceInfo.capabilities.supportedCompositeAlpha);
            QJsonArray presentModes;
            for (uint32_t i = 0; i < surfaceInfo.presentModes.size(); i++)
            {
                QJsonObject presentMode;
                presentMode["presentMode"] = QString::number(surfaceInfo.presentModes[i]);
                presentModes.append(presentMode);
            }
            jsonSurfaceCaps["presentmodes"] = presentModes;
            QJsonArray surfaceFormats;
            for (uint32_t i = 0; i < surfaceInfo.formats.size(); i++)
            {
                QJsonObject surfaceFormat;
                surfaceFormat["format"] = QString::number(surfaceInfo.formats[i].format);
                surfaceFormat["colorSpace"] = QString::number(surfaceInfo.formats[i].colorSpace);
                surfaceFormats.append(surfaceFormat);
            }
            jsonSurfaceCaps["surfaceformats"] = surfaceFormats;
        }
        root["surfacecapabilites"] = jsonSurfaceCaps;

        // Platform specific details
        QJsonObject jsonPlatformDetail;
        for (auto& detail : platformdetails)
        {
            jsonPlatformDetail[QString::fromStdString(detail.first)] = QString::fromStdString(detail.second);
        }
        root["platformdetails"] = jsonPlatformDetail;

		// Environment
		QJsonObject jsonEnv;
		jsonEnv["name"] = QString::fromStdString(os.name);
		jsonEnv["version"] = QString::fromStdString(os.version);
		jsonEnv["architecture"] = QString::fromStdString(os.architecture);
		jsonEnv["submitter"] = QString::fromStdString(submitter);
		jsonEnv["comment"] = QString::fromStdString(comment);
		jsonEnv["reportversion"] = QString::fromStdString(reportVersion);
		root["environment"] = jsonEnv;

        // VK_KHR_get_physical_device_properties2
        // Device properties 2
        QJsonArray jsonProperties2;
        for (auto& property2 : properties2) {
            QJsonObject jsonProperty2;
            jsonProperty2["name"] = QString::fromStdString(property2.name);
            jsonProperty2["extension"] = QString::fromLatin1(property2.extension);
            jsonProperty2["value"] = QString::fromStdString(property2.value);
            jsonProperties2.append(jsonProperty2);
        }
        root["deviceproperties2"] = jsonProperties2;

        // Device features 2
        QJsonArray jsonFeatures2;
        for (auto& feature2 : features2) {
            QJsonObject jsonFeature2;
            jsonFeature2["name"] = QString::fromStdString(feature2.name);
            jsonFeature2["extension"] = QString::fromLatin1(feature2.extension);
            jsonFeature2["supported"] = QString::number(feature2.supported);
            jsonFeatures2.append(jsonFeature2);
        }
        root["devicefeatures2"] = jsonFeatures2;

        // Save to file
		QJsonDocument doc(root);
		QFile jsonFile(QString::fromStdString(fileName));
		jsonFile.open(QFile::WriteOnly);
        jsonFile.write(doc.toJson(QJsonDocument::Compact));
	}

};
