load("@fly//scripts/bazel/rules:copy_files_to_directory.bzl", "copy_files_to_directory")

def copy_vulkan_layers():
    copy_files_to_directory(
        name = "copy_vulkan_validation_layer",
        srcs = [
            "@vulkan_validation_layers//:VK_LAYER_KHRONOS_validation_json",
            "@vulkan_validation_layers//:VK_LAYER_KHRONOS_validation",
        ],
        output_directory = ".",
    )

def copy_molten_vk():
    copy_files_to_directory(
        name = "copy_molten_vk",
        srcs = [
            "@molten_vk//:icd",
            "@molten_vk//:molten_vk",
        ],
        output_directory = ".",
    )
