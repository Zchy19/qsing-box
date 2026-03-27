qt_add_library(subscription STATIC
    subscription.cpp
    subscription_manager.cpp
    subscription_downloader.cpp
    subscription_dialog.cpp
    subscription_dialog.ui
    add_subscription_dialog.cpp
    add_subscription_dialog.ui
)
target_link_libraries(subscription PRIVATE
    Qt6::Widgets
    Qt6::Network
)
target_include_directories(subscription INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
