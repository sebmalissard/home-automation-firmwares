cmake_minimum_required(VERSION 3.10)

project(OtaUpdater VERSION 1.0 LANGUAGES C)

find_package(OpenSSL REQUIRED)

set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")

add_executable(create_ota_image
    src/create_ota_image.c
    src/sign_rsa2048.c
    src/sha256.c
)

target_include_directories(create_ota_image PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(create_ota_image
    OpenSSL::Crypto
)

add_executable(create_der_from_pem_key
    src/create_der_from_pem_key.c
)

target_link_libraries(create_der_from_pem_key
    OpenSSL::Crypto
)
