set(SOURCES
        main.cpp
)

add_executable(AdsLibUnitTest ${SOURCES})
target_link_libraries(AdsLibUnitTest PRIVATE AdsLib Fructose)
