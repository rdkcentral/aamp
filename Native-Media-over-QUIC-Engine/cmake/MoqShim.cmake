# Builds src/transport/moq_shim (Rust, pinned moq-dev/moq) as a static library and exposes it
# as the `moq_shim` target. Only nmqe_transport_moq links it.

find_program(CARGO cargo HINTS "$ENV{HOME}/.cargo/bin" REQUIRED)

set(MOQ_SHIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/transport/moq_shim")
set(MOQ_SHIM_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
set(MOQ_SHIM_PROFILE release)
set(MOQ_SHIM_CARGO_ARGS build --release --locked --manifest-path "${MOQ_SHIM_DIR}/Cargo.toml")
if(NMQE_RUST_TARGET)
	list(APPEND MOQ_SHIM_CARGO_ARGS --target "${NMQE_RUST_TARGET}")
	set(MOQ_SHIM_LIB "${MOQ_SHIM_TARGET_DIR}/${NMQE_RUST_TARGET}/${MOQ_SHIM_PROFILE}/libmoq_shim.a")
else()
	set(MOQ_SHIM_LIB "${MOQ_SHIM_TARGET_DIR}/${MOQ_SHIM_PROFILE}/libmoq_shim.a")
endif()

add_custom_command(
	OUTPUT "${MOQ_SHIM_LIB}"
	COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${MOQ_SHIM_TARGET_DIR} ${CARGO} ${MOQ_SHIM_CARGO_ARGS}
	DEPENDS "${MOQ_SHIM_DIR}/Cargo.toml" "${MOQ_SHIM_DIR}/Cargo.lock" "${MOQ_SHIM_DIR}/src/lib.rs"
	WORKING_DIRECTORY "${MOQ_SHIM_DIR}"
	COMMENT "Building moq_shim (moq-dev/moq) with cargo"
	USES_TERMINAL
	VERBATIM)
add_custom_target(moq_shim_build DEPENDS "${MOQ_SHIM_LIB}")

add_library(moq_shim STATIC IMPORTED GLOBAL)
set_target_properties(moq_shim PROPERTIES
	IMPORTED_LOCATION "${MOQ_SHIM_LIB}"
	INTERFACE_INCLUDE_DIRECTORIES "${MOQ_SHIM_DIR}/include")
add_dependencies(moq_shim moq_shim_build)
if(APPLE)
	# From `cargo rustc -- --print native-static-libs`.
	target_link_libraries(moq_shim INTERFACE "-framework Security" "-framework CoreFoundation" "-framework CoreServices" iconv)
else()
	target_link_libraries(moq_shim INTERFACE Threads::Threads dl m)
endif()
