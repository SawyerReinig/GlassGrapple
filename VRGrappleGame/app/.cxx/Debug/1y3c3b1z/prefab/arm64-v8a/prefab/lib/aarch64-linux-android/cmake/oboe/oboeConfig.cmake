if(NOT TARGET oboe::oboe)
add_library(oboe::oboe SHARED IMPORTED)
set_target_properties(oboe::oboe PROPERTIES
    IMPORTED_LOCATION "C:/Users/Sawyer/.gradle/caches/transforms-3/63a0432d4893379bc8692dca1284b91e/transformed/jetified-oboe-1.4.3/prefab/modules/oboe/libs/android.arm64-v8a/liboboe.so"
    INTERFACE_INCLUDE_DIRECTORIES "C:/Users/Sawyer/.gradle/caches/transforms-3/63a0432d4893379bc8692dca1284b91e/transformed/jetified-oboe-1.4.3/prefab/modules/oboe/include"
    INTERFACE_LINK_LIBRARIES ""
)
endif()

