set(SHADER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/shaders)
file(
  GLOB GLSL_SOURCE_FILES
  "${SHADER_SOURCE_DIR}/*.glsl",
  "${SHADER_SOURCE_DIR}/*.vert",
  "${SHADER_SOURCE_DIR}/*.frag",
  "${SHADER_SOURCE_DIR}/*.comp",
)

# Compile shaders
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "Compiling shader ${GLSL}")
  get_filename_component(FILE_NAME ${GLSL} NAME_WLE)
  set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL}
  )
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL ${GLSL_SOURCE_FILES})
add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
